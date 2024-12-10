#include "../pch.h"

#include <psapi.h>  // For `EnumProcessModules`
#include <tlhelp32.h>  // For `CreateToolhelp32Snapshot`, `PROCESSENTRY32`

#include "GrimHook/Logging.h"
#include "GrimHook/Process.h"
#include "GrimHook/MemoryUtils.h"
#include "MemoryReadWrite.h"

#pragma comment(lib, "Kernel32.lib")

using namespace std;
using namespace GrimHook;

namespace
{
    bool IsSearchable(const DWORD protect, const bool pageExecuteOnly)
    {
        if (protect & (PAGE_GUARD | PAGE_NOACCESS))
            return false;  // never searchable

        if (pageExecuteOnly)
            return protect & (PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE);

        return protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE);
    }

    // Search for a byte pattern in memory
    BYTE* SearchBuffer(const BYTE* buffer, const SIZE_T bufferSize, const BYTE* pattern, const SIZE_T patternSize, const bool* wildcardMask = nullptr)
    {
        for (SIZE_T i = 0; i <= bufferSize - patternSize; ++i)
        {
            bool found = true;
            for (SIZE_T j = 0; j < patternSize; ++j)
            {
                // Skip comparison if wildcardMask[j] is true (wildcard)
                if (wildcardMask && wildcardMask[j])
                    continue;  // this is a wild card byte
            
                if (pattern[j] != buffer[i + j])
                {
                    found = false;
                    break;
                }
            }
            if (found)
                return const_cast<BYTE*>(buffer) + i;  // Return the address of the match within the buffer
        }
        return nullptr;  // No match found in the buffer
    }
}

ManagedProcess::ManagedProcess(const HANDLE processHandle)
{
    m_processHandle = processHandle;

    // Enumerate memory modules.
    DWORD cbNeeded;
    if (!EnumProcessModules(processHandle, m_hModules, sizeof(m_hModules), &cbNeeded))
    {
        WinError(L"Failed to enumerate modules in process.");        
    }
}

ManagedProcess::~ManagedProcess()
{
    if (m_processHandle != nullptr)
    {
        CloseHandle(m_processHandle);
    }
}

ManagedProcess::ManagedProcess(ManagedProcess&& other) noexcept
{
    // Moves the handle to this object and nulls out the other object's handle.
    m_processHandle = other.m_processHandle;
    other.m_processHandle = nullptr;
}

ManagedProcess& ManagedProcess::operator=(ManagedProcess&& other) noexcept
{
    if (this != &other)
    {
        // Close the current handle.
        if (m_processHandle != nullptr)
        {
            CloseHandle(m_processHandle);
        }

        // Moves the handle to this object and nulls out the other object's handle.
        m_processHandle = other.m_processHandle;
        other.m_processHandle = nullptr;
    }
    return *this;
}

HANDLE ManagedProcess::getHandle() const
{
    return m_processHandle;
}

bool ManagedProcess::getMainModuleBaseAddressAndSize(LPVOID& baseAddress, SIZE_T& mainModuleSize) const
{
    MODULEINFO moduleInfo;
    if (!GetModuleInformation(m_processHandle, m_hModules[0], &moduleInfo, sizeof(moduleInfo)))
    {
        Error(L"Could not retrieve main module info for process.");
        // Failed to get module information
        baseAddress = nullptr;
        mainModuleSize = 0;
        return false;
    }

    baseAddress = moduleInfo.lpBaseOfDll;
    mainModuleSize = moduleInfo.SizeOfImage;
    return true;
}

bool ManagedProcess::isHandleValid() const
{
    if (m_processHandle == nullptr)
    {
        Error(L"ManagedProcess handle is null.");
        return false;
    }
    const DWORD processId = GetProcessId(m_processHandle);
    if (processId == 0)
    {
        WinError(L"ManagedProcess handle is invalid.");
        return false;
    }
    return true;
}

bool ManagedProcess::isProcessTerminated() const
{
    DWORD exitCode;
    if (!GetExitCodeProcess(m_processHandle, &exitCode))
    {
        WinError(L"Failed to get process exit code.");
        return false;
    }
    if (exitCode != STILL_ACTIVE)
    {
        Info("Process has terminated.");
        return true;
    }
    return false;
}

bool ManagedProcess::isAddressValid(const LPCVOID address) const
{
    if (!isHandleValid())
        return false;

    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQueryEx(m_processHandle, address, &mbi, sizeof(mbi)) != sizeof(mbi))
    {
        WinError(L"VirtualQueryEx failed.");
        return false;
    }

    if (mbi.State != MEM_COMMIT)
    {
        Error(L"Address " + ToHexWstring(address) + L" is not committed.");
        return false;
    }

    Debug(L"Address " + ToHexWstring(address) + L" is valid.");
    return true;
}

uint8_t ManagedProcess::readByte(const LPCVOID address) const
{
    return MemoryReadWrite::ReadProcessByte(m_processHandle, address);
}
bool ManagedProcess::writeByte(const LPCVOID address, const uint8_t value) const
{
    return MemoryReadWrite::WriteProcessByte(m_processHandle, address, value);
}

int8_t ManagedProcess::readSByte(const LPCVOID address) const
{
    return MemoryReadWrite::ReadProcessSByte(m_processHandle, address);
}
bool ManagedProcess::writeSByte(const LPCVOID address, const int8_t value) const
{
    return MemoryReadWrite::WriteProcessSByte(m_processHandle, address, value);
}

uint16_t ManagedProcess::readUInt16(const LPCVOID address) const
{
    return MemoryReadWrite::ReadProcessUInt16(m_processHandle, address);
}
bool ManagedProcess::writeUInt16(const LPCVOID address, const uint16_t value) const
{
    return MemoryReadWrite::WriteProcessUInt16(m_processHandle, address, value);
}

int16_t ManagedProcess::readInt16(const LPCVOID address) const
{
    return MemoryReadWrite::ReadProcessInt16(m_processHandle, address);
}
bool ManagedProcess::writeInt16(const LPCVOID address, const int16_t value) const
{
    return MemoryReadWrite::WriteProcessInt16(m_processHandle, address, value);
}

uint32_t ManagedProcess::readUInt32(const LPCVOID address) const
{
    return MemoryReadWrite::ReadProcessUInt32(m_processHandle, address);
}
bool ManagedProcess::writeUInt32(const LPCVOID address, const uint32_t value) const
{
    return MemoryReadWrite::WriteProcessUInt32(m_processHandle, address, value);
}

int32_t ManagedProcess::readInt32(const LPCVOID address) const
{
    return MemoryReadWrite::ReadProcessInt32(m_processHandle, address);
}
bool ManagedProcess::writeInt32(const LPCVOID address, const int32_t value) const
{
    return MemoryReadWrite::WriteProcessInt32(m_processHandle, address, value);
}

uint64_t ManagedProcess::readUInt64(const LPCVOID address) const
{
    return MemoryReadWrite::ReadProcessUInt64(m_processHandle, address);
}
bool ManagedProcess::writeUInt64(const LPCVOID address, const uint64_t value) const
{
    return MemoryReadWrite::WriteProcessUInt64(m_processHandle, address, value);
}

int64_t ManagedProcess::readInt64(const LPCVOID address) const
{
    return MemoryReadWrite::ReadProcessInt64(m_processHandle, address);
}
bool ManagedProcess::writeInt64(const LPCVOID address, const int64_t value) const
{
    return MemoryReadWrite::WriteProcessInt64(m_processHandle, address, value);
}

LPCVOID ManagedProcess::readPointer(const LPCVOID address) const
{
    return MemoryReadWrite::ReadProcessPointer(m_processHandle, address);
}
bool ManagedProcess::writePointer(const LPCVOID address, const LPVOID value) const
{
    return MemoryReadWrite::WriteProcessPointer(m_processHandle, address, value);
}

float ManagedProcess::readSingle(const LPCVOID address) const
{
    return MemoryReadWrite::ReadProcessSingle(m_processHandle, address);
}
bool ManagedProcess::writeSingle(const LPCVOID address, const float value) const
{
    return MemoryReadWrite::WriteProcessSingle(m_processHandle, address, value);
}

double ManagedProcess::readDouble(const LPCVOID address) const
{
    return MemoryReadWrite::ReadProcessDouble(m_processHandle, address);
}
bool ManagedProcess::writeDouble(const LPCVOID address, const double value) const 
{
    return MemoryReadWrite::WriteProcessDouble(m_processHandle, address, value);
}

bool ManagedProcess::readBool(const LPCVOID address) const
{
    return MemoryReadWrite::ReadProcessBool(m_processHandle, address);
}
bool ManagedProcess::writeBool(const LPCVOID address, const bool value) const
{
    return MemoryReadWrite::WriteProcessBool(m_processHandle, address, value);
}

string ManagedProcess::readString(const LPCVOID address, const size_t length) const
{
    return MemoryReadWrite::ReadProcessString(m_processHandle, address, length);
}
bool ManagedProcess::writeString(const LPCVOID address, const string& value) const
{
    return MemoryReadWrite::WriteProcessString(m_processHandle, address, value);
}

wstring ManagedProcess::readUnicodeString(const LPCVOID address, const size_t length) const
{
    return MemoryReadWrite::ReadProcessUnicodeString(m_processHandle, address, length);
}
bool ManagedProcess::writeUnicodeString(const LPCVOID address, const wstring& value) const
{
    return MemoryReadWrite::WriteProcessUnicodeString(m_processHandle, address, value);
}

vector<uint8_t> ManagedProcess::readByteArray(const LPCVOID address, size_t size) const
{
    return MemoryReadWrite::ReadProcessByteArray(m_processHandle, address, size);
}
bool ManagedProcess::writeByteArray(const LPCVOID address, const vector<uint8_t>& value) const
{
    return MemoryReadWrite::WriteProcessByteArray(m_processHandle, address, value);
}

vector<int8_t> ManagedProcess::readSByteArray(const LPCVOID address, size_t size) const
{
    return MemoryReadWrite::ReadProcessSByteArray(m_processHandle, address, size);
}
bool ManagedProcess::writeSByteArray(const LPCVOID address, const vector<int8_t>& value) const
{
    return MemoryReadWrite::WriteProcessSByteArray(m_processHandle, address, value);
}

vector<uint16_t> ManagedProcess::readUInt16Array(const LPCVOID address, const size_t count) const
{
    return MemoryReadWrite::ReadProcessUInt16Array(m_processHandle, address, count);
}
bool ManagedProcess::writeUInt16Array(const LPCVOID address, const vector<uint16_t>& value) const
{
    return MemoryReadWrite::WriteProcessUInt16Array(m_processHandle, address, value);
}

vector<int16_t> ManagedProcess::readInt16Array(const LPCVOID address, const size_t count) const
{
    return MemoryReadWrite::ReadProcessInt16Array(m_processHandle, address, count);
}
bool ManagedProcess::writeInt16Array(const LPCVOID address, const vector<int16_t>& value) const
{
    return MemoryReadWrite::WriteProcessInt16Array(m_processHandle, address, value);
}

vector<uint32_t> ManagedProcess::readUInt32Array(const LPCVOID address, const size_t count) const
{
    return MemoryReadWrite::ReadProcessUInt32Array(m_processHandle, address, count);
}
bool ManagedProcess::writeUInt32Array(const LPCVOID address, const vector<uint32_t>& value) const
{
    return MemoryReadWrite::WriteProcessUInt32Array(m_processHandle, address, value);
}

vector<int32_t> ManagedProcess::readInt32Array(const LPCVOID address, const size_t count) const
{
    return MemoryReadWrite::ReadProcessInt32Array(m_processHandle, address, count);
}
bool ManagedProcess::writeInt32Array(const LPCVOID address, const vector<int32_t>& value) const
{
    return MemoryReadWrite::WriteProcessInt32Array(m_processHandle, address, value);
}

vector<uint64_t> ManagedProcess::readUInt64Array(const LPCVOID address, const size_t count) const
{
    return MemoryReadWrite::ReadProcessUInt64Array(m_processHandle, address, count);
}
bool ManagedProcess::writeUInt64Array(const LPCVOID address, const vector<uint64_t>& value) const
{
    return MemoryReadWrite::WriteProcessUInt64Array(m_processHandle, address, value);
}

vector<int64_t> ManagedProcess::readInt64Array(const LPCVOID address, const size_t count) const
{
    return MemoryReadWrite::ReadProcessInt64Array(m_processHandle, address, count);
}
bool ManagedProcess::writeInt64Array(const LPCVOID address, const vector<int64_t>& value) const
{
    return MemoryReadWrite::WriteProcessInt64Array(m_processHandle, address, value);
}

vector<float> ManagedProcess::readSingleArray(const LPCVOID address, const size_t count) const
{
    return MemoryReadWrite::ReadProcessSingleArray(m_processHandle, address, count);
}
bool ManagedProcess::writeSingleArray(const LPCVOID address, const vector<float>& value) const
{
    return MemoryReadWrite::WriteProcessSingleArray(m_processHandle, address, value);
}

vector<double> ManagedProcess::readDoubleArray(const LPCVOID address, const size_t count) const
{
    return MemoryReadWrite::ReadProcessDoubleArray(m_processHandle, address, count);
}
bool ManagedProcess::writeDoubleArray(const LPCVOID address, const vector<double>& value) const
{
    return MemoryReadWrite::WriteProcessDoubleArray(m_processHandle, address, value);
}

// Search through process memory regions with an optional wildcard mask.
// By default, we only search memory with 'EXECUTE*' protection. Otherwise, we search any readable memory (slower).
LPCVOID ManagedProcess::findPattern(const BYTE* pattern, const SIZE_T patternSize, const bool* wildcardMask, const bool pageExecuteOnly) const
{
    const HANDLE processHandle = getHandle();
    if (processHandle == nullptr)
    {
        Error("Invalid process handle. Cannot find AOB pattern in memory.");
        return nullptr;
    }

    LPVOID baseAddress;
    SIZE_T mainModuleSize;
    if (!getMainModuleBaseAddressAndSize(baseAddress, mainModuleSize))
    {
        Error("Failed to get main module base address and size. Cannot find AOB pattern in memory.");
        return nullptr;
    }

    Debug("Searching for pattern in process memory from base address " + ToHexString(baseAddress) 
        + " (main module size " + to_string(mainModuleSize) + ")");
    
    LPVOID currentAddr = baseAddress;  // start of search
    const LPBYTE endAddr = static_cast<LPBYTE>(baseAddress) + mainModuleSize;
    MEMORY_BASIC_INFORMATION mbi;
    const auto buffer = new BYTE[4096];  // reusable 4 KB buffer

    // Iterate over all memory regions in the target process
    while (currentAddr < endAddr && VirtualQueryEx(processHandle, currentAddr, &mbi, sizeof(mbi)) == sizeof(mbi))
    {
        const SIZE_T regionSize = mbi.RegionSize;
        const auto regionBase = static_cast<LPBYTE>(mbi.BaseAddress);

        if (!IsSearchable(mbi.Protect, pageExecuteOnly))
        {
            // Move to the next memory region
            currentAddr = regionBase + regionSize;
            continue;
        }

        Debug("Searching memory region at " + ToHexString(regionBase) + " with size " + to_string(regionSize));

        // Ensure we do not read beyond the module's size
        const SIZE_T readableSize = min(regionSize, static_cast<SIZE_T>(endAddr - regionBase));

        // Search the memory region in 4 KB chunks
        SIZE_T offset = 0;
        while (offset < regionSize)
        {
            // Read up to 4 KB at a time:
            const SIZE_T bytesToRead = min(static_cast<uint64_t>(4096), regionSize - offset);
            SIZE_T bytesRead = 0;

            if (ReadProcessMemory(processHandle, regionBase + offset, buffer, bytesToRead, &bytesRead) && bytesRead > 0)
            {
                const BYTE* match = SearchBuffer(buffer, bytesRead, pattern, patternSize, wildcardMask);
                if (match)
                {
                    // Calculate match address in the process memory
                    LPCVOID matchAddr = regionBase + offset + (match - buffer);
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
    return nullptr;  // No match found
}


LPCVOID ManagedProcess::findPattern(const wstring& patternString, const bool pageExecuteOnly) const
{
    vector<BYTE> pattern;
    vector<bool> wildcardMask;
    const bool valid = ParsePatternString(patternString, pattern, wildcardMask);
    if (!valid)
    {
        Error(L"Invalid hex byte pattern string: " + patternString);
        return nullptr;
    }

    const auto wildcardMaskArray = new bool[wildcardMask.size()];
    copy(wildcardMask.begin(), wildcardMask.end(), wildcardMaskArray);

    const LPCVOID result = findPattern(pattern.data(), pattern.size(), wildcardMaskArray, pageExecuteOnly);

    delete[] wildcardMaskArray;
    return result;
}

bool ManagedProcess::findProcessByName(const wstring& processName, ManagedProcess*& outProcess)
{
    outProcess = nullptr;  // Initialize the out parameter to nullptr

    // Take a snapshot of all running processes
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        Error(L"Failed to create process snapshot. Error code: " + to_wstring(GetLastError()));
        return false;  // Indicate an error in taking the snapshot
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
                const HANDLE processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, entry.th32ProcessID);
                if (processHandle == nullptr)
                {
                    Error(L"Failed to open process: " + processName + L". Error code: " + to_wstring(GetLastError()));
                    CloseHandle(snapshot);
                    return false;  // Error opening the process
                }

                // Make sure process has not terminated.
                DWORD exitCode;
                if (!GetExitCodeProcess(processHandle, &exitCode))
                {
                    WinError(L"Failed to get process exit code.");
                    continue;  // ignore
                }
                if (exitCode != STILL_ACTIVE)
                {
                    Warning("Candidate process has terminated. Ignoring.");
                    continue;  // ignore
                }

                // Process found, is not terminated, and opened successfully
                Info(L"Successfully found and opened process: " + processName);

                outProcess = new ManagedProcess(processHandle);
                CloseHandle(snapshot);  // Clean up snapshot
                return true;  // Successfully found and opened process
            }
        } while (Process32NextW(snapshot, &entry));
    }
    else
    {
        Error(L"Failed to retrieve process list. Error code: " + to_wstring(GetLastError()));
        CloseHandle(snapshot);
        return false;  // Error in retrieving process list
    }

    // Clean up snapshot if process not found
    CloseHandle(snapshot);
    return true;  // No error occurred, but process not found
}

bool ManagedProcess::findProcessByWindowTitle(const wstring& windowTitle, ManagedProcess*& outProcess)
{
    outProcess = nullptr;  // Initialize out parameter to nullptr

    // Find the window with the specified title
    const HWND hwnd = FindWindowW(nullptr, windowTitle.c_str());
    if (!hwnd)
    {
        Info(L"Could not find process with window title: " + windowTitle);
        return true;  // No error occurred, but window not found
    }

    // Get the process ID associated with this window
    DWORD processId;
    GetWindowThreadProcessId(hwnd, &processId);

    // Open the process with desired access rights
    HANDLE processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (!processHandle)
    {
        Error(L"Failed to open process with window title: " + windowTitle);
        return false;  // Error occurred when opening the process
    }

    // Successfully found the process and opened a handle
    Info(L"Successfully found process with window title: " + windowTitle);

    outProcess = new ManagedProcess(processHandle);
    return true;  // Success
}