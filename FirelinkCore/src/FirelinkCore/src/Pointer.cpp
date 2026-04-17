#include <FirelinkCore/Logging.h>
#include <FirelinkCore/MemoryUtils.h>
#include <FirelinkCore/Pointer.h>
#include <FirelinkCore/Process.h>

#include <iomanip>
#include <sstream>

using namespace Firelink;

// Resolve the base pointer.
// We start with `baseAddress`, then for each offset in `offsets`, we advance by that offset and read the pointer at that
// address. Example: if `offsets == {0}`, then `baseAddress + 0` CONTAINS the address value we want, and is not the
// address itself.
const void* BasePointer::Resolve() const
{
    if (m_baseAddress == nullptr)
        return nullptr;

    if (m_offsets.empty())
        return m_baseAddress; // no offsets to follow

    auto resolvedAddress = const_cast<void*>(m_baseAddress); // start at base address
    for (const int offset : m_offsets)
    {
        // Read new pointer at resolved address + offset
        resolvedAddress = m_process.Read<void*>(GetOffsetPointer(resolvedAddress, offset));
        if (resolvedAddress == nullptr)
        {
            // Not an error; real pointers can be null at various expected times (e.g. game is not loaded).
            Debug(
                std::format(
                    "Failed to resolve pointer '{}' at address {} after offset {}", m_name, m_baseAddress, offset));
            return nullptr;
        }
    }
    return resolvedAddress;
}

// Resolve pointer, then add a final `offset` to the resolved address.
const void* BasePointer::ResolveWithOffset(const int32_t offset) const
{
    const void* resolvedAddress = Resolve();
    if (resolvedAddress == nullptr)
    {
        // Used when attempting read/write, so we do log an error (user should check `Resolve()` or `IsNull()` first).
        Error(
            std::format(
                "Failed to resolve pointer {} at address {} prior to adding offset {}", m_name, m_baseAddress, offset));
        return nullptr;
    }
    return GetOffsetPointer(resolvedAddress, offset);
}

// Get hex string of resolved address
std::string BasePointer::GetResolvedHex() const
{
    const void* resolvedAddress = Resolve();
    return std::format("{}", resolvedAddress);
}

std::string BasePointer::ReadString(const int32_t offset, const size_t length) const
{
    const void* address = ResolveWithOffset(offset);
    if (address == nullptr)
        return "";
    return m_process.ReadString(address, length);
}
bool BasePointer::WriteString(const int32_t offset, const std::string& str) const
{
    const void* address = ResolveWithOffset(offset);
    if (address == nullptr)
        return false;
    return m_process.WriteString(address, str);
}

std::u16string BasePointer::ReadUTF16String(const int32_t offset, const size_t length) const
{
    const void* address = ResolveWithOffset(offset);
    if (address == nullptr)
        return u"";
    return m_process.ReadUTF16String(address, length);
}
bool BasePointer::WriteUTF16String(const int32_t offset, const std::u16string& str) const
{
    const void* address = ResolveWithOffset(offset);
    if (address == nullptr)
        return false;
    return m_process.WriteUTF16String(address, str);
}

void BasePointer::ReadAndLogByteArray(const int32_t offset, const SIZE_T length) const
{
    const std::vector<uint8_t> bytes = ReadArray<uint8_t>(offset, length);
    if (bytes.empty())
    {
        Error(
            std::format(
                "Cannot print bytes; failed to read byte array from pointer '{}' at offset {}.", m_name, offset));
        return;
    }
    std::ostringstream stream;
    const std::string prefix = std::format("{} bytes read from pointer '{}' at offset {}: ", length, m_name, offset);
    stream << prefix;
    for (const unsigned char byte : bytes)
    {
        // Format as two-character hex:
        stream << std::hex << std::setw(2) << std::setfill('0') << byte << " ";
    }
    Info(stream.str());
}

// ChildPointer: Resolve by following the parent pointer first, then
// applying additional offsets of child pointer. As with the base method,
// for each offset in `offsets`, we advance by that offset and read a new address.
const void* ChildPointer::Resolve() const
{
    const void* parentAddress = m_parentPointer.Resolve();
    if (parentAddress == nullptr)
        return nullptr;

    Debug(std::format("ChildPointer '{}' parent pointer address: {}", m_name, parentAddress));

    auto resolvedAddress = const_cast<void*>(parentAddress);
    for (const int offset : m_offsets)
    {
        resolvedAddress = m_process.Read<void*>(GetOffsetPointer(resolvedAddress, offset));

        if (resolvedAddress == nullptr)
        {
            // Not an error; this can occur before structures are fully loaded.
            Debug(
                std::format(
                    "Failed to read child pointer '{}' step offset of parent '{}' with address: {}",
                    m_name,
                    m_parentPointer.GetName(),
                    parentAddress));
        }
        else
        {
            Debug(
                std::format(
                    "Successfully read child pointer '{}' step offset of parent '{}' with address: {}",
                    m_name,
                    m_parentPointer.GetName(),
                    parentAddress));
        }
    }
    return resolvedAddress;
}