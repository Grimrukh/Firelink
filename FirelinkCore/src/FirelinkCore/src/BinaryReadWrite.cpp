
#include <FirelinkCore/BinaryReadWrite.h>

#include <fstream>
#include <functional>
#include <ranges>
#include <vector>

using namespace Firelink;

std::vector<std::byte> BinaryReadWrite::ReadFileBytes(const std::string& path)
{
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open())
        throw BinaryReadError("Cannot open file: " + path);

    const auto file_size = f.tellg();
    if (file_size < 0)
        throw BinaryReadError("Cannot determine file size: " + path);

    f.seekg(0, std::ios::beg);
    std::vector<std::byte> buf(file_size);
    f.read(reinterpret_cast<char*>(buf.data()), file_size);
    if (!f)
        throw BinaryReadError("Error reading file: " + path);

    return buf;
}

std::vector<std::byte> BinaryReadWrite::ReadFileBytes(const std::filesystem::path& path)
{
    return ReadFileBytes(path.string());
}

void BinaryReadWrite::AssertReadPadBytes(
    std::ifstream& stream, const int& padSize, const std::string& fieldName, const uint8_t& padValue)
{
    if (!stream)
    {
        throw std::runtime_error("Failed to read value from stream.");
    }
    for (size_t i = 0; i < padSize; ++i)
    {
        uint8_t value = 0;
        stream.read(reinterpret_cast<char*>(&value), sizeof(uint8_t));
        if (value != padValue)
        {
            throw std::runtime_error(
                fieldName + ": Read value " + std::to_string(value) + " does not match expected pad value "
                + std::to_string(padValue));
        }
    }
}

void BinaryReadWrite::WritePadBytes(std::ofstream& stream, const int& padSize, const uint8_t& padValue)
{
    for (int i = 0; i < padSize; ++i)
    {
        stream.write(reinterpret_cast<const char*>(&padValue), sizeof(uint8_t));
    }
}

std::string BinaryReadWrite::ReadString(std::ifstream& stream)
{
    std::string str;
    char ch;
    while (stream.read(&ch, sizeof(char)) && ch != '\0')
        str.push_back(ch);
    return str;
}

std::string BinaryReadWrite::ReadString(std::ifstream& stream, const std::streampos& offset)
{
    const std::streampos currentOffset = stream.tellg();
    stream.seekg(offset);
    std::string result = ReadString(stream);
    stream.seekg(currentOffset);
    return result;
}

void BinaryReadWrite::WriteString(std::ofstream& stream, const std::string& str)
{
    stream.write(str.c_str(), static_cast<std::streamsize>(str.size()));
    constexpr char nullTerminator = '\0';
    stream.write(&nullTerminator, sizeof(char));
}

void BinaryReadWrite::WriteString(std::ofstream& stream, const std::streampos& offset, const std::string& str)
{
    const std::streampos currentOffset = stream.tellp();
    stream.seekp(offset);
    WriteString(stream, str);
    stream.seekp(currentOffset);
}

// Reads a null-terminated UTF-16 std::string from the stream
std::u16string BinaryReadWrite::ReadUTF16String(std::ifstream& stream)
{
    std::u16string u16str;
    char16_t ch;
    while (stream.read(reinterpret_cast<char*>(&ch), sizeof(char16_t)) && ch != u'\0')
        u16str.push_back(ch);
    return u16str;
}

std::u16string BinaryReadWrite::ReadUTF16String(std::ifstream& stream, const std::streampos& offset)
{
    const std::streampos currentOffset = stream.tellg();
    stream.seekg(offset);
    std::u16string result = ReadUTF16String(stream);
    stream.seekg(currentOffset);
    return result;
}

// Writes a null-terminated UTF-16 std::string to the stream
void BinaryReadWrite::WriteUTF16String(std::ofstream& stream, const std::u16string& str)
{
    for (char16_t ch : str)
        stream.write(reinterpret_cast<const char*>(&ch), sizeof(char16_t));
    constexpr char16_t nullTerminator = u'\0';
    stream.write(reinterpret_cast<const char*>(&nullTerminator), sizeof(char16_t));
}

void BinaryReadWrite::WriteUTF16String(std::ofstream& stream, const std::streampos& offset, const std::u16string& str)
{
    const std::streampos currentOffset = stream.tellp();
    stream.seekp(offset);
    WriteUTF16String(stream, str);
    stream.seekp(currentOffset);
}

void BinaryReadWrite::WriteUTF16String(std::ofstream& stream, const std::string& str)
{
    for (const auto u16str = std::u16string(str.begin(), str.end()); char16_t ch : u16str)
        stream.write(reinterpret_cast<const char*>(&ch), sizeof(char16_t));
    constexpr char16_t nullTerminator = u'\0';
    stream.write(reinterpret_cast<const char*>(&nullTerminator), sizeof(char16_t));
}

void BinaryReadWrite::WriteUTF16String(std::ofstream& stream, const std::streampos& offset, const std::string& str)
{
    const std::streampos currentOffset = stream.tellp();
    stream.seekp(offset);
    WriteUTF16String(stream, str);
    stream.seekp(currentOffset);
}

// Aligns the input stream to the specified boundary
void BinaryReadWrite::AlignStream(std::ifstream& stream, const std::streamsize& alignment)
{
    const std::streampos currentPosition = stream.tellg();
    const std::streamsize padding = (alignment - (currentPosition % alignment)) % alignment;
    stream.ignore(padding);
}

// Aligns the output stream to the specified boundary
void BinaryReadWrite::AlignStream(std::ofstream& stream, const std::streamsize& alignment)
{
    const std::streampos currentPosition = stream.tellp();
    const std::streamsize padding = (alignment - (currentPosition % alignment)) % alignment;
    const std::vector<char> paddingBytes(padding, 0);
    stream.write(paddingBytes.data(), padding);
}

/*!
 * Temporarily seek to a specific position in the stream, execute the provided function,
 * and then restore the stream's original position.
 *
 * @param stream The input/output stream to manipulate.
 * @param position The temporary position to seek to.
 * @param func A function or lambda to execute while at the specified position.
 */
void BinaryReadWrite::WithStreamPosition(
    std::iostream& stream, const std::streampos& position, const std::function<void()>& func)
{
    // Save the current position
    const std::streampos originalPosition = stream.tellg();

    // Seek to the desired position
    stream.seekg(position);
    stream.seekp(position);

    // Execute the provided function
    func();

    // Restore the original position
    stream.seekg(originalPosition);
    stream.seekp(originalPosition);
}

// Stores reserved offsets, under labels, for later filling. Call `Finish()` to prevent further reserving and
// validate that no offsets remain unfilled.
void BinaryReadWrite::Reserver::ReserveOffset(const std::string& label)
{
    if (m_finished)
        throw std::runtime_error("Cannot reserve offset after calling Finish().");

    const std::streampos pos = m_stream.tellp();
    if (m_reservedOffsetOffsets.contains(label))
        throw std::runtime_error("Offset label already reserved: " + label);

    m_reservedOffsetOffsets[label] = pos;

    // Write a zero.
    if (m_is64Bit)
        WriteValue<uint64_t>(m_stream, 0);
    else
        WriteValue<uint32_t>(m_stream, 0);
}

void BinaryReadWrite::Reserver::ReserveInt32(const std::string& label)
{
    if (m_finished)
        throw std::runtime_error("Cannot reserve offset after calling Finish().");

    const std::streampos pos = m_stream.tellp();
    if (m_reservedInt32Offsets.contains(label))
        throw std::runtime_error("`int32_t` label already reserved: " + label);

    m_reservedInt32Offsets[label] = pos;

    // Write a zero.
    WriteValue<int32_t>(m_stream, 0);
}

void BinaryReadWrite::Reserver::ReserveValidatedStruct(const std::string& label, const std::streamsize& size)
{
    if (m_finished)
        throw std::runtime_error("Cannot reserve struct after calling Finish().");

    std::streampos pos = m_stream.tellp();
    if (m_reservedStructOffsetsSizes.contains(label))
        throw std::runtime_error("Struct label already reserved: " + label);

    m_reservedStructOffsetsSizes[label] = std::pair(pos, size);

    // Write `size` zeroes.
    const std::vector<uint8_t> zeroes(size, 0);
    m_stream.write(reinterpret_cast<const char*>(zeroes.data()), size);
}

void BinaryReadWrite::Reserver::FillOffset(const std::string& label, const int64_t offset)
{
    if (m_finished)
        throw std::runtime_error("Cannot fill offset after calling Finish().");

    if (!m_is64Bit)
        throw std::runtime_error("Cannot fill 64-bit offset in 32-bit reserve mode.");

    if (!m_reservedOffsetOffsets.contains(label))
        throw std::runtime_error("Offset label not reserved: " + label);

    const std::streampos currentPos = m_stream.tellp();
    m_stream.seekp(m_reservedOffsetOffsets[label]);
    WriteValue<int64_t>(m_stream, offset);
    m_stream.seekp(currentPos);
    m_reservedOffsetOffsets.erase(label);
}

void BinaryReadWrite::Reserver::FillOffset(const std::string& label, const int32_t offset)
{
    if (m_finished)
        throw std::runtime_error("Cannot fill offset after calling Finish().");

    if (m_is64Bit)
        throw std::runtime_error("Cannot fill 32-bit offset in 64-bit reserve mode.");

    if (!m_reservedOffsetOffsets.contains(label))
        throw std::runtime_error("Offset label not reserved: " + label);

    const std::streampos currentPos = m_stream.tellp();
    m_stream.seekp(m_reservedOffsetOffsets[label]);
    WriteValue<int32_t>(m_stream, offset);
    m_stream.seekp(currentPos);
    m_reservedOffsetOffsets.erase(label);
}

void BinaryReadWrite::Reserver::FillOffsetWithPosition(const std::string& label)
{
    return m_is64Bit ? FillOffset(label, m_stream.tellp()) : FillOffset(label, static_cast<int32_t>(m_stream.tellp()));
}

void BinaryReadWrite::Reserver::FillOffsetWithRelativePosition(const std::string& label)
{
    if (m_relativePositionStart == 0)
        throw std::runtime_error("Cannot fill relative offset without setting relative position start.");

    const std::streampos relativePosition = m_stream.tellp() - m_relativePositionStart;
    return m_is64Bit ? FillOffset(label, relativePosition) : FillOffset(label, static_cast<int32_t>(relativePosition));
}

void BinaryReadWrite::Reserver::FillInt32(const std::string& label, const int32_t value)
{
    if (m_finished)
        throw std::runtime_error("Cannot fill `int32_t` after calling Finish().");

    if (!m_reservedInt32Offsets.contains(label))
        throw std::runtime_error("`int32_t` label not reserved: " + label);

    const std::streampos currentPos = m_stream.tellp();
    m_stream.seekp(m_reservedInt32Offsets[label]);
    WriteValue<int32_t>(m_stream, value);
    m_stream.seekp(currentPos);
    m_reservedInt32Offsets.erase(label);
}

void BinaryReadWrite::Reserver::Finish()
{
    if (m_finished)
        throw std::runtime_error("Cannot call Finish() twice on Reserver.");

    if (!m_reservedOffsetOffsets.empty())
    {
        std::string unfilledOffsets;
        for (const auto& label : std::views::keys(m_reservedOffsetOffsets))
            unfilledOffsets += label + ", ";

        throw std::runtime_error("Unfilled Reserver offsets: " + unfilledOffsets);
    }

    if (!m_reservedInt32Offsets.empty())
    {
        std::string unfilledInt32s;
        for (const auto& label : std::views::keys(m_reservedInt32Offsets))
            unfilledInt32s += label + ", ";

        throw std::runtime_error("Unfilled Reserver `int32_t` values: " + unfilledInt32s);
    }

    if (!m_reservedStructOffsetsSizes.empty())
    {
        std::string unfilledStructs;
        for (const auto& label : std::views::keys(m_reservedStructOffsetsSizes))
            unfilledStructs += label + ", ";

        throw std::runtime_error("Unfilled Reserver structs: " + unfilledStructs);
    }

    // No offsets still reserved, so we're done.
    m_finished = true;
}
