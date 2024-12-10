#include "../pch.h"

#include "MemoryReadWrite.h"

#include "GrimHook/Logging.h"


using namespace std;
using namespace GrimHook;


// Helper function to read bytes from memory
bool MemoryReadWrite::ReadProcessBytes(const HANDLE process, const LPCVOID address, LPVOID buffer, const SIZE_T size)
{
    SIZE_T bytesRead;
    if (!ReadProcessMemory(process, address, buffer, size, &bytesRead))
    {
        WinErrorPtr(L"Failed to read ANY bytes from address:", address);
        return false;
    }
    if (bytesRead != size)
    {
        WinErrorPtr(L"Could only read " + to_wstring(bytesRead) + L" out of " + to_wstring(size) + L" bytes from address:", address);
        return false;
    }
    
    DebugPtr(L"Successfully read " + to_wstring(size) + L" bytes from address:", address);
    return true;
}

// Helper function to write bytes to memory
bool MemoryReadWrite::WriteProcessBytes(const HANDLE process, const LPCVOID address, LPCVOID data, const SIZE_T size)
{
    SIZE_T bytesWritten;
    DWORD oldProtect;
    auto lpAddress = const_cast<LPVOID>(address);  // Windows API takes non-const `LPVOID lpAddress` (but does not actually modify it)

    if (VirtualProtectEx(process, lpAddress, size, PAGE_EXECUTE_READWRITE, &oldProtect))
    {
        if (WriteProcessMemory(process, lpAddress, data, size, &bytesWritten) && bytesWritten == size)
        {
            VirtualProtectEx(process, lpAddress, size, oldProtect, &oldProtect);
            DebugPtr(L"Successfully wrote " + to_wstring(size) + L" bytes to address:", address);
            return true;
        }
    
        WinErrorPtr(L"Failed to write " + to_wstring(size) + L" bytes to address ", address);
        return false;
    }
    
    WinErrorPtr(L"Failed to change memory protection for address:", address);
    return false;
}

template<typename T>
T MemoryReadWrite::ReadProcessInteger(const HANDLE process, const LPCVOID address)
{
    T value = 0;
    if (ReadProcessBytes(process, address, &value, sizeof(T)))
    {
        Debug(L"Read integer value: " + to_wstring(value));
        return value;
    }

    ErrorPtr(L"Failed to read integer from address:", address);
    return 0;
}

template<typename T>
bool MemoryReadWrite::WriteProcessInteger(const HANDLE process, const LPCVOID address, T value)
{
    Debug(L"Writing integer value: " + to_wstring(value));
    return WriteProcessBytes(process, address, &value, sizeof(T));
}

uint8_t MemoryReadWrite::ReadProcessByte(const HANDLE process, const LPCVOID address) { return ReadProcessInteger<uint8_t>(process, address); }
bool MemoryReadWrite::WriteProcessByte(const HANDLE process, const LPCVOID address, uint8_t value) { return WriteProcessInteger<uint8_t>(process, address, value); }
int8_t MemoryReadWrite::ReadProcessSByte(const HANDLE process, const LPCVOID address) { return ReadProcessInteger<int8_t>(process, address); }
bool MemoryReadWrite::WriteProcessSByte(const HANDLE process, const LPCVOID address, int8_t value) { return WriteProcessInteger<int8_t>(process, address, value); }
uint16_t MemoryReadWrite::ReadProcessUInt16(const HANDLE process, const LPCVOID address) { return ReadProcessInteger<uint16_t>(process, address); }
bool MemoryReadWrite::WriteProcessUInt16(const HANDLE process, const LPCVOID address, uint16_t value) { return WriteProcessInteger<uint16_t>(process, address, value); }
int16_t MemoryReadWrite::ReadProcessInt16(const HANDLE process, const LPCVOID address) { return ReadProcessInteger<int16_t>(process, address); }
bool MemoryReadWrite::WriteProcessInt16(const HANDLE process, const LPCVOID address, int16_t value) { return WriteProcessInteger<int16_t>(process, address, value); }
uint32_t MemoryReadWrite::ReadProcessUInt32(const HANDLE process, const LPCVOID address) { return ReadProcessInteger<uint32_t>(process, address); }
bool MemoryReadWrite::WriteProcessUInt32(const HANDLE process, const LPCVOID address, uint32_t value) { return WriteProcessInteger<uint32_t>(process, address, value); }
int32_t MemoryReadWrite::ReadProcessInt32(const HANDLE process, const LPCVOID address) { return ReadProcessInteger<int32_t>(process, address); }
bool MemoryReadWrite::WriteProcessInt32(const HANDLE process, const LPCVOID address, int32_t value) { return WriteProcessInteger<int32_t>(process, address, value); }
uint64_t MemoryReadWrite::ReadProcessUInt64(const HANDLE process, const LPCVOID address) { return ReadProcessInteger<uint64_t>(process, address); }
bool MemoryReadWrite::WriteProcessUInt64(const HANDLE process, const LPCVOID address, uint64_t value) { return WriteProcessInteger<uint64_t>(process, address, value); }
int64_t MemoryReadWrite::ReadProcessInt64(const HANDLE process, const LPCVOID address) { return ReadProcessInteger<int64_t>(process, address); }
bool MemoryReadWrite::WriteProcessInt64(const HANDLE process, const LPCVOID address, int64_t value) { return WriteProcessInteger<int64_t>(process, address, value); }

LPCVOID MemoryReadWrite::ReadProcessPointer(const HANDLE process, const LPCVOID address)
{
    LPVOID value = nullptr;
    if (ReadProcessBytes(process, address, &value, sizeof(LPVOID)))
    {
        DebugPtr(L"Read pointer value:", value);
        return value;
    }
    ErrorPtr(L"Failed to read pointer from address:", address);
    return nullptr;
}

bool MemoryReadWrite::WriteProcessPointer(const HANDLE process, const LPCVOID address, LPVOID value)
{
    DebugPtr(L"Writing pointer value:", value);
    return WriteProcessBytes(process, address, &value, sizeof(LPVOID));
}

// Read a single-precision floating point (float) from memory
float MemoryReadWrite::ReadProcessSingle(const HANDLE process, const LPCVOID address)
{
    float value = 0.;
    if (ReadProcessBytes(process, address, &value, sizeof(float)))
    {
        Debug(L"Read float value: " + to_wstring(value));
        return value;
    }
    ErrorPtr(L"Failed to read float from address:", address);
    return 0.0f;
}

// Write a single-precision floating point (float) to memory
bool MemoryReadWrite::WriteProcessSingle(const HANDLE process, const LPCVOID address, float value)
{
    Debug(L"Writing float value: " + to_wstring(value));
    return WriteProcessBytes(process, address, &value, sizeof(float));
}

// Read a double-precision floating point (double) from memory
double MemoryReadWrite::ReadProcessDouble(const HANDLE process, const LPCVOID address)
{
    double value = 0.;
    if (ReadProcessBytes(process, address, &value, sizeof(double)))
    {
        Debug(L"Read double value: " + to_wstring(value));
        return value;
    }
    ErrorPtr(L"Failed to read double from address:", address);
    return 0.0;
}

// Write a double-precision floating point (double) to memory
bool MemoryReadWrite::WriteProcessDouble(const HANDLE process, const LPCVOID address, double value)
{
    Debug(L"Writing double value: " + to_wstring(value));
    return WriteProcessBytes(process, address, &value, sizeof(double));
}

// Read a one-byte boolean value from memory. Any non-zero value becomes `true`.
bool MemoryReadWrite::ReadProcessBool(const HANDLE process, const LPCVOID address)
{
    uint8_t value = 0;
    if (ReadProcessBytes(process, address, &value, sizeof(uint8_t)))
    {
        Debug(L"Read bool value: " + to_wstring(value != 0));
        return value != 0;  // Treat any non-zero value as true
    }
    ErrorPtr(L"Failed to read bool from address:", address);
    return false;
}

// Write a boolean value to memory
bool MemoryReadWrite::WriteProcessBool(const HANDLE process, const LPCVOID address, bool value)
{
    uint8_t boolValue = value ? 1 : 0;  // Represent bool as 1 or 0
    Debug(L"Writing bool value: " + to_wstring(boolValue));
    return WriteProcessBytes(process, address, &boolValue, sizeof(uint8_t));
}

// Read a string of fixed size from memory
string MemoryReadWrite::ReadProcessString(const HANDLE process, const LPCVOID address, const SIZE_T size)
{
    vector<char> buffer(size);  // Allocate a buffer of the given size
    if (ReadProcessBytes(process, address, buffer.data(), size))
    {
        string result(buffer.begin(), buffer.end());
        Debug(L"Read string of size " + to_wstring(size) + L": " + wstring(result.begin(), result.end()));
        return result;
    }
    ErrorPtr(L"Failed to read string from address:", address);
    return "";
}

// Write a string to memory. Null terminator is NOT added automatically.
bool MemoryReadWrite::WriteProcessString(const HANDLE process, const LPCVOID address, const string& str)
{
    const SIZE_T byteSize = str.size();  // Number of bytes to write (without null terminator)
    Debug(L"Writing string: " + wstring(str.begin(), str.end()));
    return WriteProcessBytes(process, address, str.data(), byteSize);  // Use data() and size() to avoid null terminator
}

// Read a fixed-length UTF-16 (wide) string from memory (length in characters)
wstring MemoryReadWrite::ReadProcessUnicodeString(const HANDLE process, const LPCVOID address, size_t length)
{
    vector<wchar_t> buffer(length);  // Allocate buffer for 'length' characters
    const SIZE_T byteSize = length * sizeof(wchar_t);  // Size in bytes (2 * length)

    if (ReadProcessBytes(process, address, buffer.data(), byteSize))
    {
        wstring result(buffer.begin(), buffer.end());
        Debug(L"Read wide string of length " + to_wstring(length) + L": " + result);
        return result;
    }
    ErrorPtr(L"Failed to read wide string from address:", address);
    return L"";
}

// Write a UTF-16 (wide) string to memory without null terminator
bool MemoryReadWrite::WriteProcessUnicodeString(const HANDLE process, const LPCVOID address, const wstring& wstr)
{
    const SIZE_T byteSize = wstr.size() * sizeof(wchar_t);  // Calculate the number of bytes to write (2 * number of characters)
    Debug(L"Writing wide string (without null terminator): " + wstr);
    return WriteProcessBytes(process, address, wstr.data(), byteSize);  // Use data() and size() to write exactly the content without null terminator
}

// Read an array of integers
template <typename T>
vector<T> MemoryReadWrite::ReadProcessIntegerArray(const HANDLE process, const LPCVOID address, size_t count)
{
    vector<T> buffer(count);
    const SIZE_T byteSize = count * sizeof(T);  // Calculate the number of bytes to read (count * sizeof(T))

    if (ReadProcessBytes(process, address, buffer.data(), byteSize))
    {
        Debug(L"Read integer array of size " + to_wstring(count));
        return buffer;
    }
    ErrorPtr(L"Failed to read integer array from address:", address);
    return {};  // Return an empty vector on failure
}

// Write an array of unsigned integers
template <typename T>
bool MemoryReadWrite::WriteProcessIntegerArray(const HANDLE process, const LPCVOID address, const vector<T>& array)
{
    const SIZE_T byteSize = array.size() * sizeof(T);  // Calculate the number of bytes to write (array size * sizeof(T))

    Debug(L"Writing integer array of size " + to_wstring(array.size()));
    return WriteProcessBytes(process, address, array.data(), byteSize);  // Write the array data to memory
}

vector<uint8_t> MemoryReadWrite::ReadProcessByteArray(const HANDLE process, const LPCVOID address, const SIZE_T size)
{ return ReadProcessIntegerArray<uint8_t>(process, address, size); }

bool MemoryReadWrite::WriteProcessByteArray(const HANDLE process, const LPCVOID address, const vector<uint8_t>& array)
{ return WriteProcessIntegerArray<uint8_t>(process, address, array); }

vector<int8_t> MemoryReadWrite::ReadProcessSByteArray(const HANDLE process, const LPCVOID address, const SIZE_T size)
{ return ReadProcessIntegerArray<int8_t>(process, address, size); }

bool MemoryReadWrite::WriteProcessSByteArray(const HANDLE process, const LPCVOID address, const vector<int8_t>& array)
{ return WriteProcessIntegerArray<int8_t>(process, address, array); }

vector<uint16_t> MemoryReadWrite::ReadProcessUInt16Array(const HANDLE process, const LPCVOID address, const SIZE_T size)
{ return ReadProcessIntegerArray<uint16_t>(process, address, size); }

bool MemoryReadWrite::WriteProcessUInt16Array(const HANDLE process, const LPCVOID address, const vector<uint16_t>& array)
{ return WriteProcessIntegerArray<uint16_t>(process, address, array); }

vector<int16_t> MemoryReadWrite::ReadProcessInt16Array(const HANDLE process, const LPCVOID address, const SIZE_T size)
{ return ReadProcessIntegerArray<int16_t>(process, address, size); }

bool MemoryReadWrite::WriteProcessInt16Array(const HANDLE process, const LPCVOID address, const vector<int16_t>& array)
{ return WriteProcessIntegerArray<int16_t>(process, address, array); }

vector<uint32_t> MemoryReadWrite::ReadProcessUInt32Array(const HANDLE process, const LPCVOID address, const SIZE_T size)
{ return ReadProcessIntegerArray<uint32_t>(process, address, size); }

bool MemoryReadWrite::WriteProcessUInt32Array(const HANDLE process, const LPCVOID address, const vector<uint32_t>& array)
{ return WriteProcessIntegerArray<uint32_t>(process, address, array); }

vector<int32_t> MemoryReadWrite::ReadProcessInt32Array(const HANDLE process, const LPCVOID address, const SIZE_T size)
{ return ReadProcessIntegerArray<int32_t>(process, address, size); }

bool MemoryReadWrite::WriteProcessInt32Array(const HANDLE process, const LPCVOID address, const vector<int32_t>& array)
{ return WriteProcessIntegerArray<int32_t>(process, address, array); }

vector<uint64_t> MemoryReadWrite::ReadProcessUInt64Array(const HANDLE process, const LPCVOID address, const SIZE_T size)
{ return ReadProcessIntegerArray<uint64_t>(process, address, size); }

bool MemoryReadWrite::WriteProcessUInt64Array(const HANDLE process, const LPCVOID address, const vector<uint64_t>& array)
{ return WriteProcessIntegerArray<uint64_t>(process, address, array); }

vector<int64_t> MemoryReadWrite::ReadProcessInt64Array(const HANDLE process, const LPCVOID address, const SIZE_T size)
{ return ReadProcessIntegerArray<int64_t>(process, address, size); }

bool MemoryReadWrite::WriteProcessInt64Array(const HANDLE process, const LPCVOID address, const vector<int64_t>& array)
{ return WriteProcessIntegerArray<int64_t>(process, address, array); }


vector<float> MemoryReadWrite::ReadProcessSingleArray(const HANDLE process, const LPCVOID address, const SIZE_T size)
{
    return ReadProcessIntegerArray<float>(process, address, size);
}
bool MemoryReadWrite::WriteProcessSingleArray(const HANDLE process, const LPCVOID address, const vector<float>& array)
{
    return WriteProcessIntegerArray<float>(process, address, array);
}

vector<double> MemoryReadWrite::ReadProcessDoubleArray(const HANDLE process, const LPCVOID address, const SIZE_T size)
{
    return ReadProcessIntegerArray<double>(process, address, size);
}
bool MemoryReadWrite::WriteProcessDoubleArray(const HANDLE process, const LPCVOID address, const vector<double>& array)
{
    return WriteProcessIntegerArray<double>(process, address, array);
}

// Read a structure from memory.
// Uses an out value and returns a success bool, rather than some default structure.
template <typename T>
bool MemoryReadWrite::ReadProcessStructure(const HANDLE process, const LPCVOID address, T& structure)
{
    const SIZE_T byteSize = sizeof(T);  // Calculate the size of the structure in bytes
    if (ReadProcessBytes(process, address, &structure, byteSize))
    {
        Debug(L"Read structure of size " + to_wstring(byteSize));
        return false;
    }

    ErrorPtr(L"Failed to read structure from address:", address);
    return true;
}

// Write a structure to memory
template <typename T>
bool MemoryReadWrite::WriteProcessStructure(const HANDLE process, const LPCVOID address, const T& structure)
{
    const SIZE_T byteSize = sizeof(T);  // Calculate the size of the structure in bytes
    Debug(L"Writing structure of size " + to_wstring(byteSize));
    return WriteProcessBytes(process, address, &structure, byteSize);
}