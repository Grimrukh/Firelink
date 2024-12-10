#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <windows.h>

namespace GrimHook::MemoryReadWrite
{
    // Helper function to read bytes from memory
    bool ReadProcessBytes(HANDLE process, LPCVOID address, LPVOID buffer, SIZE_T size);

    // Helper function to write bytes to memory
    bool WriteProcessBytes(HANDLE process, LPCVOID address, LPCVOID data, SIZE_T size);

    template<typename T>
    T ReadProcessInteger(HANDLE process, LPCVOID address);

    // Write an integer type with appropriate number of bytes
    template<typename T>
    bool WriteProcessInteger(HANDLE process, LPCVOID address, T value);

    uint8_t ReadProcessByte(HANDLE process, LPCVOID address);
    bool WriteProcessByte(HANDLE process, LPCVOID address, uint8_t value);
    int8_t ReadProcessSByte(HANDLE process, LPCVOID address);
    bool WriteProcessSByte(HANDLE process, LPCVOID address, int8_t value);
    uint16_t ReadProcessUInt16(HANDLE process, LPCVOID address);
    bool WriteProcessUInt16(HANDLE process, LPCVOID address, uint16_t value);
    int16_t ReadProcessInt16(HANDLE process, LPCVOID address);
    bool WriteProcessInt16(HANDLE process, LPCVOID address, int16_t value);
    uint32_t ReadProcessUInt32(HANDLE process, LPCVOID address);
    bool WriteProcessUInt32(HANDLE process, LPCVOID address, uint32_t value);
    int32_t ReadProcessInt32(HANDLE process, LPCVOID address);
    bool WriteProcessInt32(HANDLE process, LPCVOID address, int32_t value);
    uint64_t ReadProcessUInt64(HANDLE process, LPCVOID address);
    bool WriteProcessUInt64(HANDLE process, LPCVOID address, uint64_t value);
    int64_t ReadProcessInt64(HANDLE process, LPCVOID address);
    bool WriteProcessInt64(HANDLE process, LPCVOID address, int64_t value);

    // Read a pointer from memory
    LPCVOID ReadProcessPointer(HANDLE process, LPCVOID address);

    // Write a pointer to memory (obviously dangerous and rare!)
    bool WriteProcessPointer(HANDLE process, LPCVOID address, LPVOID value);

    // Read a single-precision floating point (float) from memory
    float ReadProcessSingle(HANDLE process, LPCVOID address);

    // Write a single-precision floating point (float) to memory
    bool WriteProcessSingle(HANDLE process, LPCVOID address, float value);

    // Read a double-precision floating point (double) from memory
    double ReadProcessDouble(HANDLE process, LPCVOID address);

    // Write a double-precision floating point (double) to memory
    bool WriteProcessDouble(HANDLE process, LPCVOID address, double value);

    // Read a one-byte boolean value from memory. Any non-zero value becomes `true`.
    bool ReadProcessBool(HANDLE process, LPCVOID address);

    // Write a boolean value to memory
    bool WriteProcessBool(HANDLE process, LPCVOID address, bool value);

    // Read a string of fixed size from memory
    std::string ReadProcessString(HANDLE process, LPCVOID address, SIZE_T size);

    // Write a string to memory. Null terminator is NOT added automatically.
    bool WriteProcessString(HANDLE process, LPCVOID address, const std::string& str);

    // Read a fixed-length UTF-16 (wide) string from memory (length in characters)
    std::wstring ReadProcessUnicodeString(HANDLE process, LPCVOID address, size_t length);

    // Write a UTF-16 (wide) string to memory without null terminator
    bool WriteProcessUnicodeString(HANDLE process, LPCVOID address, const std::wstring& wstr);

    // Read an array of integers
    template <typename T>
    std::vector<T> ReadProcessIntegerArray(HANDLE process, LPCVOID address, size_t count);

    // Write an array of unsigned integers
    template <typename T>
    bool WriteProcessIntegerArray(HANDLE process, LPCVOID address, const std::vector<T>& array);

    // Array template instantiations
    std::vector<uint8_t> ReadProcessByteArray(HANDLE process, LPCVOID address, SIZE_T size);
    bool WriteProcessByteArray(HANDLE process, LPCVOID address, const std::vector<uint8_t>& array);
    std::vector<int8_t> ReadProcessSByteArray(HANDLE process, LPCVOID address, SIZE_T size);
    bool WriteProcessSByteArray(HANDLE process, LPCVOID address, const std::vector<int8_t>& array);
    std::vector<uint16_t> ReadProcessUInt16Array(HANDLE process, LPCVOID address, SIZE_T size);
    bool WriteProcessUInt16Array(HANDLE process, LPCVOID address, const std::vector<uint16_t>& array);
    std::vector<int16_t> ReadProcessInt16Array(HANDLE process, LPCVOID address, SIZE_T size);
    bool WriteProcessInt16Array(HANDLE process, LPCVOID address, const std::vector<int16_t>& array);
    std::vector<uint32_t> ReadProcessUInt32Array(HANDLE process, LPCVOID address, SIZE_T size);
    bool WriteProcessUInt32Array(HANDLE process, LPCVOID address, const std::vector<uint32_t>& array);
    std::vector<int32_t> ReadProcessInt32Array(HANDLE process, LPCVOID address, SIZE_T size);
    bool WriteProcessInt32Array(HANDLE process, LPCVOID address, const std::vector<int32_t>& array);
    std::vector<uint64_t> ReadProcessUInt64Array(HANDLE process, LPCVOID address, SIZE_T size);
    bool WriteProcessUInt64Array(HANDLE process, LPCVOID address, const std::vector<uint64_t>& array);
    std::vector<int64_t> ReadProcessInt64Array(HANDLE process, LPCVOID address, SIZE_T size);
    bool WriteProcessInt64Array(HANDLE process, LPCVOID address, const std::vector<int64_t>& array);

    // read a single-precision floating point (float) array from memory
    std::vector<float> ReadProcessSingleArray(HANDLE process, LPCVOID address, SIZE_T size);
    // Write a single-precision floating point (float) array to memory
    bool WriteProcessSingleArray(HANDLE process, LPCVOID address, const std::vector<float>& array);

    // read a double-precision floating point (double) array from memory
    std::vector<double> ReadProcessDoubleArray(HANDLE process, LPCVOID address, SIZE_T size);
    // Write a double-precision floating point (double) array to memory
    bool WriteProcessDoubleArray(HANDLE process, LPCVOID address, const std::vector<double>& array);

    // read a structure from memory.
    // Uses an out value and returns a success bool, rather than some default structure.
    template <typename T>
    bool ReadProcessStructure(HANDLE process, LPCVOID address, T& structure);

    // Write a structure to memory
    template <typename T>
    bool WriteProcessStructure(HANDLE process, LPCVOID address, const T& structure);
}
