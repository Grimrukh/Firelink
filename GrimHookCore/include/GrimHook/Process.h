#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <windows.h>

namespace GrimHook {

    // EXPORT: Wraps a process HANDLE and passes it to memory read/write methods.
    class __declspec(dllexport) ManagedProcess
    {
        HANDLE m_processHandle;
        HMODULE m_hModules[1024] = { nullptr };

    public:
        explicit ManagedProcess(HANDLE processHandle);

        // RULE OF FIVE
        ~ManagedProcess();
        ManagedProcess(const ManagedProcess&) = delete;
        ManagedProcess& operator=(const ManagedProcess&) = delete;
        ManagedProcess(ManagedProcess&&) noexcept;
        ManagedProcess& operator=(ManagedProcess&&) noexcept;

        // Get the process handle.
        [[nodiscard]] HANDLE getHandle() const;

        // Get the main module base address.
        bool getMainModuleBaseAddressAndSize(LPVOID& baseAddress, SIZE_T& mainModuleSize) const;

        [[nodiscard]] bool isHandleValid() const;

        [[nodiscard]] bool isProcessTerminated() const;

        bool isAddressValid(LPCVOID address) const;

        uint8_t readByte(LPCVOID address) const;
        bool writeByte(LPCVOID address, uint8_t value) const;
        int8_t readSByte(LPCVOID address) const;
        bool writeSByte(LPCVOID address, int8_t value) const;
        uint16_t readUInt16(LPCVOID address) const;
        bool writeUInt16(LPCVOID address, uint16_t value) const;
        int16_t readInt16(LPCVOID address) const;
        bool writeInt16(LPCVOID address, int16_t value) const;
        uint32_t readUInt32(LPCVOID address) const;
        bool writeUInt32(LPCVOID address, uint32_t value) const;
        int32_t readInt32(LPCVOID address) const;
        bool writeInt32(LPCVOID address, int32_t value) const;
        uint64_t readUInt64(LPCVOID address) const;
        bool writeUInt64(LPCVOID address, uint64_t value) const;
        int64_t readInt64(LPCVOID address) const;
        bool writeInt64(LPCVOID address, int64_t value) const;

        LPCVOID readPointer(LPCVOID address) const;
        bool writePointer(LPCVOID address, LPVOID value) const;

        float readSingle(LPCVOID address) const;
        bool writeSingle(LPCVOID address, float value) const;

        double readDouble(LPCVOID address) const;
        bool writeDouble(LPCVOID address, double value) const;

        bool readBool(LPCVOID address) const;
        bool writeBool(LPCVOID address, bool value) const;

        std::string readString(LPCVOID address, size_t length) const;
        bool writeString(LPCVOID address, const std::string& value) const;

        std::wstring readUnicodeString(LPCVOID address, size_t length) const;
        bool writeUnicodeString(LPCVOID address, const std::wstring& value) const;

        std::vector<uint8_t> readByteArray(LPCVOID address, size_t size) const;
        bool writeByteArray(LPCVOID address, const std::vector<uint8_t>& value) const;
        std::vector<int8_t> readSByteArray(LPCVOID address, size_t size) const;
        bool writeSByteArray(LPCVOID address, const std::vector<int8_t>& value) const;
        std::vector<uint16_t> readUInt16Array(LPCVOID address, size_t count) const;
        bool writeUInt16Array(LPCVOID address, const std::vector<uint16_t>& value) const;
        std::vector<int16_t> readInt16Array(LPCVOID address, size_t count) const;
        bool writeInt16Array(LPCVOID address, const std::vector<int16_t>& value) const;
        std::vector<uint32_t> readUInt32Array(LPCVOID address, size_t count) const;
        bool writeUInt32Array(LPCVOID address, const std::vector<uint32_t>& value) const;
        std::vector<int32_t> readInt32Array(LPCVOID address, size_t count) const;
        bool writeInt32Array(LPCVOID address, const std::vector<int32_t>& value) const;
        std::vector<uint64_t> readUInt64Array(LPCVOID address, size_t count) const;
        bool writeUInt64Array(LPCVOID address, const std::vector<uint64_t>& value) const;
        std::vector<int64_t> readInt64Array(LPCVOID address, size_t count) const;
        bool writeInt64Array(LPCVOID address, const std::vector<int64_t>& value) const;

        std::vector<float> readSingleArray(LPCVOID address, size_t count) const;
        bool writeSingleArray(LPCVOID address, const std::vector<float>& value) const;

        std::vector<double> readDoubleArray(LPCVOID address, size_t count) const;
        bool writeDoubleArray(LPCVOID address, const std::vector<double>& value) const;

        // --- PATTERN SEARCH ---
        [[nodiscard]] LPCVOID findPattern(const BYTE* pattern, SIZE_T patternSize, const bool* wildcardMask = nullptr, bool pageExecuteOnly = true) const;
        [[nodiscard]] LPCVOID findPattern(const std::wstring& patternString, bool pageExecuteOnly = true) const;

        // Static function to find and open a process by name.
        // Returns `true` if the process search was valid, even if no `outProcess` was found.
        static bool findProcessByName(const std::wstring& processName, ManagedProcess*& outProcess);

        // Static function to find and open a process by window title.
        // Returns `true` if the process search was valid, even if no `outProcess` was found.
        static bool findProcessByWindowTitle(const std::wstring& windowTitle, ManagedProcess*& outProcess);
    };
}
