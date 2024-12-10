#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <windows.h>

#include "Process.h"

namespace GrimHook {

    // EXPORT: Manages a pointer to a memory address in a process, with read/write functions to use at that address
    class __declspec(dllexport) BasePointer
    {
    protected:
        ManagedProcess* m_process;  // Managed process containing pointer
        std::wstring m_name;  // Name of the pointer for debugging
        LPCVOID m_baseAddress;  // Holds the base address of the pointer
        std::vector<SIZE_T> m_offsets;  // Relative offsets to follow

    public:
        // RULE OF FIVE (all defaults)
        virtual ~BasePointer() = default;
        BasePointer(const BasePointer&) = default;
        BasePointer& operator=(const BasePointer&) = default;
        BasePointer(BasePointer&&) = default;
        BasePointer& operator=(BasePointer&&) = default;

        // Constructor for absolute address pointer
        BasePointer(ManagedProcess* process, std::wstring name, const LPCVOID address, const std::vector<SIZE_T>& offsets = {})
            : m_process(process), m_name(std::move(name)), m_baseAddress(address), m_offsets(offsets) {}

        // --- STATIC CONSTRUCTORS ---

        // Pointer found by reading a 32-bit jump instruction `jumpRelativeOffset` bytes after `baseAddress`
        // and making that jump after that instruction.
        static BasePointer* withJumpInstruction(
            ManagedProcess* process, const std::wstring& name, LPCVOID baseAddress, SIZE_T jumpRelativeOffset, const std::vector<SIZE_T>& offsets = {});

        // Pointer to the start of found AOB pattern.
        static BasePointer* fromAobPattern(
            ManagedProcess* process, const std::wstring& name, const std::wstring& aobPattern, const std::vector<SIZE_T>& offsets = {});

        // Pointer found by first search for an AOB pattern, then reading a 32-bit jump instruction
        // `jumpRelativeOffset` bytes after that pattern, and making that jump after that instruction (as above).
        static BasePointer* fromAobWithJumpInstruction(
            ManagedProcess* process, const std::wstring& name, const std::wstring& aobPattern, SIZE_T jumpRelativeOffset, const std::vector<SIZE_T>& offsets = {});

        // --- METHODS ---

        LPCVOID getBaseAddress() const
        { return m_baseAddress; }

        std::wstring getName() const
        { return m_name; }

        // Get the process for child pointers to inherit
        ManagedProcess* getProcess() const
        { return m_process; }

        // Check if the pointer resolves to null (i.e. invalid pointer)
        bool isNull() { return resolve() == nullptr; }

        // Resolve the pointer by following the address and its offsets
        virtual LPCVOID resolve();

        // Resolve and add `offset` to the resolved address
        LPCVOID resolveWithOffset(int32_t offset);

        // Get hex wide string of resolved address
        std::wstring getResolvedHex();

        // Wrapper functions for reading and writing memory types

        uint8_t readByte(int32_t offset);
        bool writeByte(int32_t offset, uint8_t value);

        int8_t readSByte(int32_t offset);
        bool writeSByte(int32_t offset, int8_t value);

        uint16_t readUInt16(int32_t offset);
        bool writeUInt16(int32_t offset, uint16_t value);

        int16_t readInt16(int32_t offset);
        bool writeInt16(int32_t offset, int16_t value);

        uint32_t readUInt32(int32_t offset);
        bool writeUInt32(int32_t offset, uint32_t value);

        int32_t readInt32(int32_t offset);
        bool writeInt32(int32_t offset, int32_t value);

        uint64_t readUInt64(int32_t offset);
        bool writeUInt64(int32_t offset, uint64_t value);

        int64_t readInt64(int32_t offset);
        bool writeInt64(int32_t offset, int64_t value);

        LPCVOID readPointer(int32_t offset);
        bool writePointer(int32_t offset, LPVOID value);

        float readSingle(int32_t offset);
        bool writeSingle(int32_t offset, float value);

        double readDouble(int32_t offset);
        bool writeDouble(int32_t offset, double value);

        bool readBool(int32_t offset);
        bool writeBool(int32_t offset, bool value);

        std::string readString(int32_t offset, SIZE_T length);
        bool writeString(int32_t offset, const std::string& str);

        std::wstring readUnicodeString(int32_t offset, SIZE_T length);
        bool writeUnicodeString(int32_t offset, const std::wstring& str);

        std::vector<uint8_t> readByteArray(int32_t offset, SIZE_T size);
        bool writeByteArray(int32_t offset, const std::vector<uint8_t>& bytes);

        std::vector<int8_t> readSByteArray(int32_t offset, SIZE_T size);
        bool writeSByteArray(int32_t offset, const std::vector<int8_t>& bytes);

        std::vector<uint16_t> readUInt16Array(int32_t offset, SIZE_T size);
        bool writeUInt16Array(int32_t offset, const std::vector<uint16_t>& array);

        std::vector<int16_t> readInt16Array(int32_t offset, SIZE_T size);
        bool writeInt16Array(int32_t offset, const std::vector<int16_t>& array);

        std::vector<uint32_t> readUInt32Array(int32_t offset, SIZE_T size);
        bool writeUInt32Array(int32_t offset, const std::vector<uint32_t>& array);

        std::vector<int32_t> readInt32Array(int32_t offset, SIZE_T size);
        bool writeInt32Array(int32_t offset, const std::vector<int32_t>& array);

        std::vector<uint64_t> readUInt64Array(int32_t offset, SIZE_T size);
        bool writeUInt64Array(int32_t offset, const std::vector<uint64_t>& array);

        std::vector<int64_t> readInt64Array(int32_t offset, SIZE_T size);
        bool writeInt64Array(int32_t offset, const std::vector<int64_t>& array);

        std::vector<float> readSingleArray(int32_t offset, SIZE_T size);
        bool writeSingleArray(int32_t offset, const std::vector<float>& array);

        std::vector<double> readDoubleArray(int32_t offset, SIZE_T size);
        bool writeDoubleArray(int32_t offset, const std::vector<double>& array);

        void readAndLogByteArray(int32_t offset, SIZE_T size);
    };

    // EXPORT: Child pointer class to chain from an existing pointer
    class __declspec(dllexport) ChildPointer final : public BasePointer
    {
        BasePointer* m_parentPointer;  // Parent pointer to follow

    public:
        // Constructor taking a parent pointer and additional offsets
        ChildPointer(const std::wstring& name, BasePointer* parent, const std::vector<SIZE_T>& offsets)
            : BasePointer(parent->getProcess(), name, nullptr, offsets), m_parentPointer(parent) {}

        // Resolve by following the parent pointer first
        LPCVOID resolve() override;
    };

}


