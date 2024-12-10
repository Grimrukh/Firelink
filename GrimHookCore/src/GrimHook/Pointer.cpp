#include "GrimHook/Pointer.h"

#include <iomanip>
#include <sstream>

#include "GrimHook/Logging.h"
#include "GrimHook/Process.h"
#include "GrimHook/MemoryUtils.h"

using namespace std;
using namespace GrimHook;

// Relative pointer constructor with relative jump offset
BasePointer* BasePointer::withJumpInstruction(
    ManagedProcess* process, const wstring& name, const LPCVOID baseAddress, const SIZE_T jumpRelativeOffset, const vector<SIZE_T>& offsets)
{
    Debug(L"AOB pattern for BasePointer " + name + L" found at: " + ToHexWstring(baseAddress));

    // Resolve relative jump by reading 32-bit offset
    const LPCVOID jumpLocation = GetOffsetPointer(baseAddress, jumpRelativeOffset);
    const SIZE_T relativeJump = process->readInt32(jumpLocation);
    const LPCVOID jumpedAddress = GetOffsetPointer(jumpLocation, 4 + relativeJump);  // jump from end of relative offset
    DebugPtr(L"Resolved pointer " + name + L" relative address to:", jumpedAddress);

    return new BasePointer(process, name, jumpedAddress, offsets);
}

// AOB search constructor (Absolute AOB)
BasePointer* BasePointer::fromAobPattern(ManagedProcess* process, const wstring& name, const wstring& aobPattern, const vector<SIZE_T>& offsets)
{
    const LPCVOID baseAddress = process->findPattern(aobPattern);
    if (baseAddress == nullptr)
    {
        Error(L"Failed to find AOB pattern for BasePointer " + name);
    }
    else
    {
        DebugPtr(L"AOB pattern for BasePointer " + name + L" found at:", baseAddress);
    }

    return new BasePointer(process, name, baseAddress, offsets);
}

// AOB search constructor (with relative jump offset)
BasePointer* BasePointer::fromAobWithJumpInstruction(ManagedProcess* process, const wstring& name, const wstring& aobPattern, const SIZE_T jumpRelativeOffset, const vector<SIZE_T>& offsets)
{
    const LPCVOID aobAddress = process->findPattern(aobPattern);
    if (aobAddress == nullptr)
    {
        Error(L"Failed to find AOB pattern for relative jump BasePointer " + name);
        return new BasePointer(process, name, nullptr, offsets);
    }
    Debug(L"AOB pattern for relative jump BasePointer " + name + L" found at: " + ToHexWstring(aobAddress));
    return withJumpInstruction(process, name, aobAddress, jumpRelativeOffset, offsets);
}

// Resolve the base pointer.
// We start with `baseAddress`, then for each offset in `offsets`, we advance by that offset and read the pointer at that address.
// Example: if `offsets == {0}`, then `baseAddress + 0` CONTAINS the address value we want, and is not the address itself.
LPCVOID BasePointer::resolve()
{
    if (m_baseAddress == nullptr)
        return nullptr;

    if (m_offsets.empty())
        return m_baseAddress;  // no offsets to follow

    auto resolvedAddress = const_cast<LPVOID>(m_baseAddress);  // start at base address
    for (const SIZE_T offset : m_offsets)
    {
        // Read new pointer at resolved address + offset
        resolvedAddress = const_cast<LPVOID>(m_process->readPointer(GetOffsetPointer(resolvedAddress, offset)));
        if (resolvedAddress == nullptr)
        {
            // Not an error; real pointers can be null at various expected times (e.g. game is not loaded).
            Debug(
                L"Failed to resolve pointer " + m_name + L" at address " + ToHexWstring(m_baseAddress)
                + L" after offset " + to_wstring(offset));
            return nullptr;
        }
    }
    return resolvedAddress;
}

// Resolve pointer, then add a final `offset` to the resolved address.
LPCVOID BasePointer::resolveWithOffset(const int32_t offset)
{
    const LPCVOID resolvedAddress = resolve();
    if (resolvedAddress == nullptr)
    {
        // Used when attempting read/write, so we do log an error (user should check `Resolve()` or `IsNull()` first).
        Error(
            L"Failed to resolve pointer " + m_name + L" at address " + ToHexWstring(m_baseAddress)
            + L" prior to adding offset " + to_wstring(offset));
        return nullptr;
    }
    return GetOffsetPointer(resolvedAddress, offset);
}

// Get hex string of resolved address
wstring BasePointer::getResolvedHex()
{
    const LPCVOID resolvedAddress = resolve();
    return ToHexWstring(resolvedAddress);
}

// Wrapper functions for reading and writing memory types:

uint8_t BasePointer::readByte(const int32_t offset)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return 0;
    return m_process->readByte(address);
}
bool BasePointer::writeByte(const int32_t offset, uint8_t value)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return false;
    return m_process->writeByte(address, value);
}

int8_t BasePointer::readSByte(const int32_t offset)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return 0;
    return m_process->readSByte(address);
}
bool BasePointer::writeSByte(const int32_t offset, int8_t value)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return false;
    return m_process->writeSByte(address, value);
}

uint16_t BasePointer::readUInt16(const int32_t offset)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return 0;
    return m_process->readUInt16(address);
}
bool BasePointer::writeUInt16(const int32_t offset, uint16_t value)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return false;
    return m_process->writeUInt16(address, value);
}

int16_t BasePointer::readInt16(const int32_t offset)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return 0;
    return m_process->readInt16(address);
}
bool BasePointer::writeInt16(const int32_t offset, int16_t value)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return false;
    return m_process->writeInt16(address, value);
}

uint32_t BasePointer::readUInt32(const int32_t offset)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return 0;
    return m_process->readUInt32(address);
}
bool BasePointer::writeUInt32(const int32_t offset, uint32_t value)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return false;
    return m_process->writeUInt32(address, value);
}

int32_t BasePointer::readInt32(const int32_t offset)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return 0;
    return m_process->readInt32(address);
}
bool BasePointer::writeInt32(const int32_t offset, int32_t value)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return false;
    return m_process->writeInt32(address, value);
}

uint64_t BasePointer::readUInt64(const int32_t offset)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return 0;
    return m_process->readUInt64(address);
}
bool BasePointer::writeUInt64(const int32_t offset, uint64_t value)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return false;
    return m_process->writeUInt64(address, value);
}

int64_t BasePointer::readInt64(const int32_t offset)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return 0;
    return m_process->readInt64(address);
}
bool BasePointer::writeInt64(const int32_t offset, int64_t value)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return false;
    return m_process->writeInt64(address, value);
}

LPCVOID BasePointer::readPointer(const int32_t offset)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return nullptr;
    return m_process->readPointer(address);
}
bool BasePointer::writePointer(const int32_t offset, LPVOID value)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return false;
    return m_process->writePointer(address, value);
}

float BasePointer::readSingle(const int32_t offset)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return 0;
    return m_process->readSingle(address);
}
bool BasePointer::writeSingle(const int32_t offset, float value)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return false;
    return m_process->writeSingle(address, value);
}

double BasePointer::readDouble(const int32_t offset)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return 0;
    return m_process->readDouble(address);
}
bool BasePointer::writeDouble(const int32_t offset, double value)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return false;
    return m_process->writeDouble(address, value);
}

bool BasePointer::readBool(const int32_t offset)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return false;
    return m_process->readBool(address);
}
bool BasePointer::writeBool(const int32_t offset, bool value)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return false;
    return m_process->writeBool(address, value);
}

string BasePointer::readString(const int32_t offset, SIZE_T length)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return "";
    return m_process->readString(address, length);
}
bool BasePointer::writeString(const int32_t offset, const string& str)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return false;
    return m_process->writeString(address, str);
}

wstring BasePointer::readUnicodeString(const int32_t offset, SIZE_T length)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return L"";
    return m_process->readUnicodeString(address, length);
}
bool BasePointer::writeUnicodeString(const int32_t offset, const wstring& str)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return false;
    return m_process->writeUnicodeString(address, str);
}

vector<uint8_t> BasePointer::readByteArray(const int32_t offset, SIZE_T size)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return {};
    return m_process->readByteArray(address, size);
}
bool BasePointer::writeByteArray(const int32_t offset, const vector<uint8_t>& bytes)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return false;
    return m_process->writeByteArray(address, bytes);
}

vector<int8_t> BasePointer::readSByteArray(const int32_t offset, SIZE_T size)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return {};
    return m_process->readSByteArray(address, size);
}
bool BasePointer::writeSByteArray(const int32_t offset, const vector<int8_t>& bytes)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return false;
    return m_process->writeSByteArray(address, bytes);
}

vector<uint16_t> BasePointer::readUInt16Array(const int32_t offset, SIZE_T size)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return {};
    return m_process->readUInt16Array(address, size);
}
bool BasePointer::writeUInt16Array(const int32_t offset, const vector<uint16_t>& array)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return false;
    return m_process->writeUInt16Array(address, array);
}

vector<int16_t> BasePointer::readInt16Array(const int32_t offset, SIZE_T size)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return {};
    return m_process->readInt16Array(address, size);
}
bool BasePointer::writeInt16Array(const int32_t offset, const vector<int16_t>& array)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return false;
    return m_process->writeInt16Array(address, array);
}

vector<uint32_t> BasePointer::readUInt32Array(const int32_t offset, SIZE_T size)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return {};
    return m_process->readUInt32Array(address, size);
}
bool BasePointer::writeUInt32Array(const int32_t offset, const vector<uint32_t>& array)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return false;
    return m_process->writeUInt32Array(address, array);
}

vector<int32_t> BasePointer::readInt32Array(const int32_t offset, SIZE_T size)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return {};
    return m_process->readInt32Array(address, size);
}
bool BasePointer::writeInt32Array(const int32_t offset, const vector<int32_t>& array)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return false;
    return m_process->writeInt32Array(address, array);
}

vector<uint64_t> BasePointer::readUInt64Array(const int32_t offset, SIZE_T size)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return {};
    return m_process->readUInt64Array(address, size);
}
bool BasePointer::writeUInt64Array(const int32_t offset, const vector<uint64_t>& array)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return false;
    return m_process->writeUInt64Array(address, array);
}

vector<int64_t> BasePointer::readInt64Array(const int32_t offset, SIZE_T size)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return {};
    return m_process->readInt64Array(address, size);
}
bool BasePointer::writeInt64Array(const int32_t offset, const vector<int64_t>& array)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return false;
    return m_process->writeInt64Array(address, array);
}

vector<float> BasePointer::readSingleArray(const int32_t offset, SIZE_T size)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return {};
    return m_process->readSingleArray(address, size);
}
bool BasePointer::writeSingleArray(const int32_t offset, const vector<float>& array)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return false;
    return m_process->writeSingleArray(address, array);
}

vector<double> BasePointer::readDoubleArray(const int32_t offset, SIZE_T size)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return {};
    return m_process->readDoubleArray(address, size);
}
bool BasePointer::writeDoubleArray(const int32_t offset, const vector<double>& array)
{
    const LPCVOID address = resolveWithOffset(offset);
    if (address == nullptr) return false;
    return m_process->writeDoubleArray(address, array);
}

void BasePointer::readAndLogByteArray(const int32_t offset, const SIZE_T size)
{
    const vector<uint8_t> bytes = readByteArray(offset, size);
    if (bytes.empty())
    {
        Error(L"Cannot print bytes; failed to read byte array from " + m_name + L" at offset " + to_wstring(offset));
        return;
    }
    wostringstream stream;
    stream << size << L" bytes read from " << m_name << L" at offset " << offset << L": ";
    for (const unsigned char byte : bytes)
    {
        // Format as two-character hex:
        stream << hex << setw(2) << setfill(L'0') << byte << " ";
    }
    Info(stream.str());
}

// ChildPointer: Resolve by following the parent pointer first, then
// applying additional offsets of child pointer. As with the base method,
// for each offset in `offsets`, we advance by that offset and read a new address.
LPCVOID ChildPointer::resolve()
{
    const LPCVOID parentAddress = m_parentPointer->resolve();
    if (parentAddress == nullptr)
        return nullptr;

    DebugPtr(L"ChildPointer " + m_name + L" parent pointer address:", parentAddress);

    auto resolvedAddress = const_cast<LPVOID>(parentAddress);
    for (const SIZE_T offset : m_offsets)
    {
        resolvedAddress = const_cast<LPVOID>(m_process->readPointer(GetOffsetPointer(resolvedAddress, offset)));

        if (resolvedAddress == nullptr)
        {
            // Not an error; this can occur before structures are fully loaded.
            DebugPtr(
                L"Failed to read child pointer step offset of parent '" + m_parentPointer->getName() + L"' with address:", parentAddress);
        }
        else
        {
            DebugPtr(
                L"Successfully read child pointer step offset of parent '" + m_parentPointer->getName() + L"' with address:", parentAddress);
        }        
    }
    return resolvedAddress;
}