#include "pch.h"

#include <Firelink/Logging.h>
#include <Firelink/MemoryUtils.h>
#include <Firelink/Pointer.h>
#include <Firelink/Process.h>

#include <atomic>
#include <chrono>
#include <format>
#include <psapi.h> // For `EnumProcessModules`
#include <ranges>
#include <thread>
#include <tlhelp32.h> // For `CreateToolhelp32Snapshot`, `PROCESSENTRY32`

#pragma comment(lib, "Kernel32.lib")

using namespace Firelink;

namespace
{
    /// @brief Check if a memory page is searchable.
    bool IsSearchable(const DWORD protect, const bool pageExecuteOnly)
    {
        if (protect & (PAGE_GUARD | PAGE_NOACCESS))
            return false; // never searchable

        if (pageExecuteOnly)
            return protect & (PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE);

        return protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE);
    }

    /// @brief Search for a byte pattern in memory.
    BYTE* SearchBuffer(
        const BYTE* buffer,
        const SIZE_T bufferSize,
        const BYTE* pattern,
        const SIZE_T patternSize,
        const bool* wildcardMask = nullptr)
    {
        for (SIZE_T i = 0; i <= bufferSize - patternSize; ++i)
        {
            bool found = true;
            for (SIZE_T j = 0; j < patternSize; ++j)
            {
                // Skip comparison if wildcardMask[j] is true (wildcard)
                if (wildcardMask && wildcardMask[j])
                    continue; // this is a wild card byte

                if (pattern[j] != buffer[i + j])
                {
                    found = false;
                    break;
                }
            }
            if (found)
                return const_cast<BYTE*>(buffer) + i; // Return the address of the match within the buffer
        }
        return nullptr; // No match found in the buffer
    }
} // namespace

ManagedProcess::ManagedProcess(void* processHandle)
: m_processHandle(processHandle, &CloseHandle) // NOLINT
{
    // Enumerate memory modules.
    DWORD cbNeeded;
    if (!EnumProcessModules(processHandle, m_hModules, sizeof(m_hModules), &cbNeeded))
    {
        WinError("Failed to enumerate modules in process.");
    }
}

ManagedProcess::~ManagedProcess()
{
    // Clear pointers before closing process handle.
    m_pointers.clear();
}

bool ManagedProcess::GetMainModuleBaseAddressAndSize(void*& baseAddress, SIZE_T& mainModuleSize) const
{
    MODULEINFO moduleInfo;
    if (!GetModuleInformation(GetHandle(), m_hModules[0], &moduleInfo, sizeof(moduleInfo)))
    {
        Error("Could not retrieve main module info for process.");
        // Failed to get module information
        baseAddress = nullptr;
        mainModuleSize = 0;
        return false;
    }

    baseAddress = moduleInfo.lpBaseOfDll;
    mainModuleSize = moduleInfo.SizeOfImage;
    return true;
}

bool ManagedProcess::IsHandleValid() const
{
    if (m_processHandle == nullptr)
    {
        Error("ManagedProcess handle is null.");
        return false;
    }
    if (GetProcessId(GetHandle()) == 0)
    {
        WinError("ManagedProcess handle is invalid.");
        return false;
    }
    return true;
}

bool ManagedProcess::IsProcessTerminated() const
{
    DWORD exitCode;
    if (!GetExitCodeProcess(GetHandle(), &exitCode))
    {
        WinError("Failed to get process exit code.");
        return false;
    }
    if (exitCode != STILL_ACTIVE)
    {
        Info("Process has terminated.");
        return true;
    }
    return false;
}

bool ManagedProcess::IsAddressValid(const void* address) const
{
    if (!IsHandleValid())
        return false;

    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQueryEx(GetHandle(), address, &mbi, sizeof(mbi)) != sizeof(mbi))
    {
        WinError("VirtualQueryEx failed.");
        return false;
    }

    if (mbi.State != MEM_COMMIT)
    {
        Error(std::format("Address {} is not committed.", address));
        return false;
    }

    Debug(std::format("Address {} is valid.", address));
    return true;
}

const BasePointer* ManagedProcess::GetPointer(const std::string& name) const
{
    try
    {
        return m_pointers.at(name).get();
    }
    catch (const std::out_of_range&)
    {
        Error(std::format("Pointer name '{}' not found in process.", name));
        return nullptr;
    }
}

void ManagedProcess::DeletePointer(const std::string& name)
{
    if (m_pointers.contains(name))
        m_pointers.erase(name);
    else
        Error(std::format("Pointer name '{}' not found in process.", name));
}

bool ManagedProcess::TryDeletePointer(const std::string& name)
{
    if (m_pointers.contains(name))
    {
        m_pointers.erase(name);
        return true;
    }
    return false;
}

// region Pointer Construction

BasePointer* ManagedProcess::CreatePointer(
    const std::string& name, const void* baseAddress, const std::vector<int>& offsets)
{
    if (!m_allowPointerOverwrite && m_pointers.contains(name))
        throw std::invalid_argument(std::format("Pointer name already exists in process: {}", name));

    const auto pointer = new BasePointer(*this, baseAddress, name, offsets);
    m_pointers[name] = std::unique_ptr<BasePointer>(pointer);
    return m_pointers[name].get();
}

BasePointer* ManagedProcess::CreatePointerWithJumpInstruction(
    const std::string& name, const void* baseAddress, const int jumpRelativeOffset, const std::vector<int>& offsets)
{
    if (!m_allowPointerOverwrite && m_pointers.contains(name))
        throw std::invalid_argument(std::format("Pointer name already exists in process: {}", name));

    Debug(std::format("AOB pattern for BasePointer {} found at: {}", name, baseAddress));

    // Resolve relative jump by reading 32-bit offset.
    const void* jumpLocation = GetOffsetPointer(baseAddress, jumpRelativeOffset);
    const int relativeJump = Read<int32_t>(jumpLocation);
    // Jump from end of relative offset.
    const void* jumpedAddress = GetOffsetPointer(jumpLocation, 4 + relativeJump);
    Debug(std::format("Resolved pointer {} relative address to: {}", name, jumpedAddress));

    const auto pointer = new BasePointer(*this, jumpedAddress, name, offsets);
    m_pointers[name] = std::unique_ptr<BasePointer>(pointer);
    return m_pointers[name].get();
}

// AOB search constructor (Absolute AOB)
BasePointer* ManagedProcess::CreatePointerToAob(
    const std::string& name, const std::string& aobPattern, const std::vector<int>& offsets)
{
    if (!m_allowPointerOverwrite && m_pointers.contains(name))
        throw std::invalid_argument(std::format("Pointer name already exists in process: {}", name));

    const void* baseAddress = FindPattern(aobPattern);
    if (baseAddress == nullptr)
        Error("Failed to find AOB pattern for BasePointer " + name);
    else
        Debug(std::format("AOB pattern for BasePointer {} found at: {}", name, baseAddress));

    const auto pointer = new BasePointer(*this, baseAddress, name, offsets);
    m_pointers[name] = std::unique_ptr<BasePointer>(pointer);
    return m_pointers[name].get();
}

// AOB search constructor (with relative jump offset)
BasePointer* ManagedProcess::CreatePointerToAobWithJumpInstruction(
    const std::string& name,
    const std::string& aobPattern,
    const int jumpRelativeOffset,
    const std::vector<int>& offsets)
{
    if (!m_allowPointerOverwrite && m_pointers.contains(name))
        throw std::invalid_argument(std::format("Pointer name already exists in process: {}", name));

    const void* aobAddress = FindPattern(aobPattern);
    if (aobAddress == nullptr)
    {
        Error(std::format("Failed to find AOB pattern for relative jump BasePointer '{}'.", name));
        const auto pointer = new BasePointer(*this, nullptr, name, offsets);
        m_pointers[name] = std::unique_ptr<BasePointer>(pointer);
        return pointer;
    }
    Debug(std::format("AOB pattern for relative jump BasePointer {} found at: ", name, aobAddress));

    return CreatePointerWithJumpInstruction(name, aobAddress, jumpRelativeOffset, offsets);
}

BasePointer ManagedProcess::CreateTempPointer(const std::string& name, const void* baseAddress) const
{
    return {*this, baseAddress, name};
}

ChildPointer* ManagedProcess::CreateChildPointer(
    const BasePointer& parent, const std::string& name, const std::vector<int>& offsets)
{
    if (!m_allowPointerOverwrite && m_pointers.contains(name))
        throw std::invalid_argument(std::format("Pointer name already exists in process: {}", name));

    // Check that `parent` is actually a pointer in this process.
    bool parentFound = false;
    for (const auto& value : std::views::values(m_pointers))
    {
        if (value.get() == &parent)
        {
            parentFound = true;
            break;
        }
    }
    if (!parentFound)
    {
        Error(std::format("Parent pointer '{}' not present in this process.", parent.GetName()));
        return nullptr;
    }

    const auto child = new ChildPointer(parent, name, offsets);
    m_pointers[name] = std::unique_ptr<BasePointer>(child);
    return dynamic_cast<ChildPointer*>(m_pointers[name].get());
}

ChildPointer* ManagedProcess::CreateChildPointer(
    const std::string& parentName, const std::string& name, const std::vector<int>& offsets)
{
    if (!m_allowPointerOverwrite && m_pointers.contains(name))
        throw std::invalid_argument(std::format("Pointer name already exists in process: {}", name));

    try
    {
        const BasePointer* parent = GetPointer(parentName);
        return CreateChildPointer(*parent, name, offsets);
    }
    catch (const std::out_of_range&)
    {
        Error(std::format("Parent pointer name '{}' not found in process.", parentName));
        return nullptr;
    }
}

std::string ManagedProcess::ReadString(const void* address, const size_t length) const
{
    if (!IsAddressValid(address))
        return "";

    auto buffer = new char[length]; // NOLINT
    if (!ReadProcessBytes(address, buffer, length))
    {
        delete[] buffer;
        return "";
    }

    std::string str(buffer, length);
    delete[] buffer;
    return str;
}

bool ManagedProcess::WriteString(const void* address, const std::string& str) const
{
    return WriteProcessBytes(address, str.c_str(), str.size());
}

std::u16string ManagedProcess::ReadUTF16String(const void* address, const size_t length) const
{
    if (!IsAddressValid(address))
        return u"";

    auto buffer = new char16_t[length]; // NOLINT
    if (!ReadProcessBytes(address, buffer, length * sizeof(char16_t)))
    {
        delete[] buffer;
        return u"";
    }

    std::u16string str(buffer, length);
    delete[] buffer;
    return str;
}

bool ManagedProcess::WriteUTF16String(const void* address, const std::u16string& u16str) const
{
    return WriteProcessBytes(address, u16str.c_str(), u16str.size() * sizeof(char16_t));
}

// endregion

const void* ManagedProcess::FindPattern(
    const BYTE* pattern, const SIZE_T patternSize, const bool* wildcardMask, const bool pageExecuteOnly) const
{
    void* processHandle = GetHandle();
    if (processHandle == nullptr)
    {
        Error("Invalid process handle. Cannot find AOB pattern in memory.");
        return nullptr;
    }

    void* baseAddress;
    SIZE_T mainModuleSize;
    if (!GetMainModuleBaseAddressAndSize(baseAddress, mainModuleSize))
    {
        Error("Failed to get main module base address and size. Cannot find AOB pattern in memory.");
        return nullptr;
    }

    Debug(
        std::format(
            "Searching for pattern in process memory from base address {} (main module size {})",
            baseAddress,
            mainModuleSize));

    const void* currentAddr = baseAddress;                              // start of search
    LPBYTE endAddr = static_cast<LPBYTE>(baseAddress) + mainModuleSize; // NOLINT
    MEMORY_BASIC_INFORMATION mbi;
    const auto buffer = new BYTE[4096]; // reusable 4 KB buffer

    // Iterate over all memory regions in the target process.
    while (currentAddr < endAddr && VirtualQueryEx(processHandle, currentAddr, &mbi, sizeof(mbi)) == sizeof(mbi))
    {
        const SIZE_T regionSize = mbi.RegionSize;
        const auto regionBase = static_cast<LPBYTE>(mbi.BaseAddress);

        if (!IsSearchable(mbi.Protect, pageExecuteOnly))
        {
            // Move to the next memory region.
            currentAddr = regionBase + regionSize;
            continue;
        }

        Debug(std::format("Searching memory region at {} with size {}.", static_cast<void*>(regionBase), regionSize));

        // Ensure we do not read beyond the module's size.
        const SIZE_T readableSize = min(regionSize, static_cast<SIZE_T>(endAddr - regionBase));

        // Search the memory region in 4 KB chunks.
        SIZE_T offset = 0;
        while (offset < regionSize)
        {
            // Read up to 4 KB at a time.
            const SIZE_T bytesToRead = min(static_cast<uint64_t>(4096), regionSize - offset);
            SIZE_T bytesRead = 0;

            if (ReadProcessMemory(processHandle, regionBase + offset, buffer, bytesToRead, &bytesRead) && bytesRead > 0)
            {
                if (const BYTE* match = SearchBuffer(buffer, bytesRead, pattern, patternSize, wildcardMask))
                {
                    // Calculate match address in the process memory
                    const void* matchAddr = regionBase + offset + (match - buffer); // NOLINT
                    delete[] buffer;
                    return matchAddr;
                }
            }

            // Move offset forward, overlapping chunks by patternSize - 1 bytes
            offset += bytesToRead - patternSize + 1;

            // Ensure we don't pass the readable size
            if (offset > readableSize)
                break;
        }

        // Move to the next memory region
        currentAddr = regionBase + regionSize;
    }

    delete[] buffer;
    return nullptr; // No match found
}

const void* ManagedProcess::FindPattern(const std::string& patternString, const bool pageExecuteOnly) const
{
    std::vector<BYTE> pattern;
    std::vector<bool> wildcardMask;
    if (const bool valid = ParsePatternString(patternString, pattern, wildcardMask); !valid)
    {
        Error("Invalid hex byte pattern string: " + patternString);
        return nullptr;
    }

    const auto wildcardMaskArray = new bool[wildcardMask.size()];
    std::ranges::copy(wildcardMask, wildcardMaskArray);

    const void* result = FindPattern(pattern.data(), pattern.size(), wildcardMaskArray, pageExecuteOnly);

    delete[] wildcardMaskArray;
    return result;
}

bool ManagedProcess::FindProcessByName(const std::wstring& processName, ManagedProcess*& outProcess)
{
    outProcess = nullptr;

    // Take a snapshot of all running processes
    void* snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        Error(std::format("Failed to create process snapshot. Error code: {}", GetLastError()));
        return false; // Indicate an error in taking the snapshot
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(PROCESSENTRY32W);

    // Iterate over processes
    if (Process32FirstW(snapshot, &entry))
    {
        do
        {
            // Use _wcsicmp for wide string comparison
            if (_wcsicmp(entry.szExeFile, processName.c_str()) == 0)
            {
                void* processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, entry.th32ProcessID);
                if (processHandle == nullptr)
                {
                    const std::string lastError = std::to_string(GetLastError());
                    Error(
                        std::format("Failed to open process '{}'. Error code: {}", UTF16ToUTF8(processName), lastError));
                    CloseHandle(snapshot);
                    return false; // Error opening the process
                }

                // Make sure process has not terminated.
                DWORD exitCode;
                if (!GetExitCodeProcess(processHandle, &exitCode))
                {
                    WinError("Failed to get process exit code.");
                    continue; // ignore
                }
                if (exitCode != STILL_ACTIVE)
                {
                    Warning("Candidate process has terminated. Ignoring.");
                    continue; // ignore
                }

                // Process found, is not terminated, and opened successfully
                Info(std::format("Successfully found and opened process: {}", UTF16ToUTF8(processName)));

                outProcess = new ManagedProcess(processHandle);
                CloseHandle(snapshot); // Clean up snapshot
                return true;           // Successfully found and opened process
            }
        } while (Process32NextW(snapshot, &entry));
    }
    else
    {
        Error(std::format("Failed to retrieve process list. Error code: {}", GetLastError()));
        CloseHandle(snapshot);
        return false; // Error in retrieving process list
    }

    // Clean up snapshot if process not found
    CloseHandle(snapshot);
    return true; // No error occurred, but process not found
}

bool ManagedProcess::FindProcessByWindowTitle(const std::wstring& windowTitle, ManagedProcess*& outProcess)
{
    outProcess = nullptr; // Initialize out parameter to nullptr

    // Find the window with the specified title
    HWND hwnd = FindWindowW(nullptr, windowTitle.c_str()); // NOLINT
    if (!hwnd)
    {
        Info(std::format("Could not find process with window title: {}", UTF16ToUTF8(windowTitle)));
        return true; // No error occurred, but window not found
    }

    // Get the process ID associated with this window
    DWORD processId;
    GetWindowThreadProcessId(hwnd, &processId);

    // Open the process with desired access rights
    void* processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (!processHandle)
    {
        Error(std::format("Failed to open process with window title: {}", UTF16ToUTF8(windowTitle)));
        return false; // Error occurred when opening the process
    }

    // Successfully found the process and opened a handle
    Info(std::format("Successfully found process with window title: {}", UTF16ToUTF8(windowTitle)));

    outProcess = new ManagedProcess(processHandle);
    return true; // Success
}

std::unique_ptr<ManagedProcess> ManagedProcess::WaitForProcess(
    const std::wstring& processName, const int timeoutMs, const int refreshIntervalMs, const std::atomic<bool>& stopFlag)
{
    Info(std::format("Waiting until process with name '{}' is found.", UTF16ToUTF8(processName)));

    ManagedProcess* process;
    std::chrono::milliseconds timeout(timeoutMs);

    while (true)
    {
        if (stopFlag.load())
            return nullptr;

        if (timeout <= std::chrono::milliseconds(0))
        {
            Warning("Timeout reached. Process not found.");
            break;
        }

        if (FindProcessByName(processName, process))
        {
            // Search was valid, but process may not have been found.
            if (process)
            {
                Info("Process found!");
                return std::unique_ptr<ManagedProcess>(process);
            }
            Warning(std::format("Process not found. Retrying in {} ms... ", refreshIntervalMs));
        }
        else
        {
            Error("Error occurred during process search.");
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(refreshIntervalMs));
        timeout -= std::chrono::milliseconds(refreshIntervalMs);
    }

    return nullptr;
}

bool ManagedProcess::ReadProcessBytes(const void* address, void* buffer, const size_t size) const
{
    SIZE_T bytesRead;
    if (!ReadProcessMemory(GetHandle(), address, buffer, size, &bytesRead))
    {
        WinError(std::format("Failed to read ANY bytes from address: {}", address));
        return false;
    }
    if (bytesRead != size)
    {
        WinError(std::format("Could only read {} out of {} bytes from address: {}", bytesRead, size, address));
        return false;
    }

    Debug(std::format("Successfully read {} bytes from address: {}", size, address));
    return true;
}

bool ManagedProcess::WriteProcessBytes(const void* address, const void* data, const size_t size) const
{
    SIZE_T bytesWritten;
    DWORD oldProtect;

    // Windows API requires mutable `void* lpAddress`.
    auto lpAddress = const_cast<void*>(address);

    if (void* process = GetHandle(); VirtualProtectEx(process, lpAddress, size, PAGE_EXECUTE_READWRITE, &oldProtect))
    {
        if (WriteProcessMemory(process, lpAddress, data, size, &bytesWritten) && bytesWritten == size)
        {
            VirtualProtectEx(process, lpAddress, size, oldProtect, &oldProtect);
            Debug(std::format("Successfully wrote {} bytes to address: {}", size, address));
            return true;
        }

        WinError(std::format("Failed to write {} bytes to address: {} ", size, address));
        return false;
    }

    WinError(std::format("Failed to change memory protection for address: {}", address));
    return false;
}
