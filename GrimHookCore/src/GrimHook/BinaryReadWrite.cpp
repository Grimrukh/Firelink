
#include <fstream>
#include <functional>
#include <vector>

#include "GrimHook/BinaryReadWrite.h"

using namespace std;
using namespace GrimHook;


void BinaryReadWrite::AssertReadPadBytes(ifstream& stream, const int& padSize, const string& fieldName, const uint8_t& padValue)
{
    if (!stream)
    {
        throw runtime_error("Failed to read value from stream.");
    }
    for (size_t i = 0; i < padSize; ++i)
    {
        uint8_t value = 0;
        stream.read(reinterpret_cast<char*>(&value), sizeof(uint8_t));
        if (value != padValue)
        {
            throw runtime_error(
                fieldName + ": Read value " + to_string(value) + " does not match expected pad value "
                + to_string(padValue));
        }
    }
}

void BinaryReadWrite::WritePadBytes(ofstream& stream, const int& padSize, const uint8_t& padValue)
{
    for (int i = 0; i < padSize; ++i)
    {
        stream.write(reinterpret_cast<const char*>(&padValue), sizeof(uint8_t));
    }
}

string BinaryReadWrite::ReadString(ifstream& stream)
{
    string str;
    char ch;
    while (stream.read(&ch, sizeof(char)) && ch != '\0')
        str.push_back(ch);
    return str;
}

string BinaryReadWrite::ReadString(ifstream& stream, const streampos& offset)
{
    const streampos currentOffset = stream.tellg();
    stream.seekg(offset);
    string result = ReadString(stream);
    stream.seekg(currentOffset);
    return result;
}

void BinaryReadWrite::WriteString(ofstream& stream, const string& str)
{
    stream.write(str.c_str(), static_cast<streamsize>(str.size()));
    constexpr char nullTerminator = '\0';
    stream.write(&nullTerminator, sizeof(char));
}

void BinaryReadWrite::WriteString(ofstream& stream, const streampos& offset, const string& str)
{
    const streampos currentOffset = stream.tellp();
    stream.seekp(offset);
    WriteString(stream, str);
    stream.seekp(currentOffset);
}

// Reads a null-terminated UTF-16 string from the stream
u16string BinaryReadWrite::ReadUTF16String(ifstream& stream)
{
    u16string u16str;
    char16_t ch;
    while (stream.read(reinterpret_cast<char*>(&ch), sizeof(char16_t)) && ch != u'\0')
        u16str.push_back(ch);
    return u16str;
}

u16string BinaryReadWrite::ReadUTF16String(ifstream& stream, const streampos& offset)
{
    const streampos currentOffset = stream.tellg();
    stream.seekg(offset);
    u16string result = ReadUTF16String(stream);
    stream.seekg(currentOffset);
    return result;
}

// Writes a null-terminated UTF-16 string to the stream
void BinaryReadWrite::WriteUTF16String(ofstream& stream, const u16string& str)
{
    for (char16_t ch : str)
        stream.write(reinterpret_cast<const char*>(&ch), sizeof(char16_t));
    constexpr char16_t nullTerminator = u'\0';
    stream.write(reinterpret_cast<const char*>(&nullTerminator), sizeof(char16_t));
}

void BinaryReadWrite::WriteUTF16String(ofstream& stream, const streampos& offset, const u16string& str)
{
    const streampos currentOffset = stream.tellp();
    stream.seekp(offset);
    WriteUTF16String(stream, str);
    stream.seekp(currentOffset);
}

void BinaryReadWrite::WriteUTF16String(ofstream& stream, const string& str)
{
    for (auto u16str = u16string(str.begin(), str.end()); char16_t ch : u16str)
        stream.write(reinterpret_cast<const char*>(&ch), sizeof(char16_t));
    constexpr char16_t nullTerminator = u'\0';
    stream.write(reinterpret_cast<const char*>(&nullTerminator), sizeof(char16_t));
}

void BinaryReadWrite::WriteUTF16String(ofstream& stream, const streampos& offset, const string& str)
{
    const streampos currentOffset = stream.tellp();
    stream.seekp(offset);
    WriteUTF16String(stream, str);
    stream.seekp(currentOffset);
}

// Aligns the input stream to the specified boundary
void BinaryReadWrite::AlignStream(ifstream& stream, const streamsize& alignment)
{
    const streampos currentPosition = stream.tellg();
    const streamsize padding = (alignment - (currentPosition % alignment)) % alignment;
    stream.ignore(padding);
}

// Aligns the output stream to the specified boundary
void BinaryReadWrite::AlignStream(ofstream& stream, const streamsize& alignment)
{
    const streampos currentPosition = stream.tellp();
    const streamsize padding = (alignment - (currentPosition % alignment)) % alignment;
    const vector<char> paddingBytes(padding, 0);
    stream.write(paddingBytes.data(), padding);
}


/**
 * Temporarily seek to a specific position in the stream, execute the provided function,
 * and then restore the stream's original position.
 *
 * @param stream The input/output stream to manipulate.
 * @param position The temporary position to seek to.
 * @param func A function or lambda to execute while at the specified position.
 */
void BinaryReadWrite::WithStreamPosition(iostream& stream, const streampos position, const function<void()>& func)
{
    // Save the current position
    const streampos originalPosition = stream.tellg();

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
void BinaryReadWrite::Reserver::ReserveOffset(const string& label)
{
    if (finished)
        throw runtime_error("Cannot reserve offset after calling Finish().");

    const streampos pos = stream.tellp();
    if (reservedOffsetOffsets.contains(label))
        throw runtime_error("Offset label already reserved: " + label);

    reservedOffsetOffsets[label] = pos;

    // Write a zero.
    if (is64Bit)
        WriteValue<uint64_t>(stream, 0);
    else
        WriteValue<uint32_t>(stream, 0);
}


void BinaryReadWrite::Reserver::ReserveValidatedStruct(const string& label, const streamsize& size)
{
    if (finished)
        throw runtime_error("Cannot reserve struct after calling Finish().");

    streampos pos = stream.tellp();
    if (reservedStructOffsetsSizes.contains(label))
        throw runtime_error("Struct label already reserved: " + label);

    reservedStructOffsetsSizes[label] = pair(pos, size);

    // Write `size` zeroes.
    const vector<uint8_t> zeroes(size, 0);
    stream.write(reinterpret_cast<const char*>(zeroes.data()), size);
}


void BinaryReadWrite::Reserver::FillOffset(const string& label, const int64_t offset)
{
    if (finished)
        throw runtime_error("Cannot fill offset after calling Finish().");

    if (!is64Bit)
        throw runtime_error("Cannot fill 64-bit offset in 32-bit reserve mode.");

    if (!reservedOffsetOffsets.contains(label))
        throw runtime_error("Offset label not reserved: " + label);

    const streampos currentPos = stream.tellp();
    stream.seekp(reservedOffsetOffsets[label]);
    WriteValue<int64_t>(stream, offset);
    stream.seekp(currentPos);
    reservedOffsetOffsets.erase(label);
}


void BinaryReadWrite::Reserver::FillOffset(const string& label, const int32_t offset)
{
    if (finished)
        throw runtime_error("Cannot fill offset after calling Finish().");

    if (is64Bit)
        throw runtime_error("Cannot fill 32-bit offset in 64-bit reserve mode.");

    if (!reservedOffsetOffsets.contains(label))
        throw runtime_error("Offset label not reserved: " + label);

    const streampos currentPos = stream.tellp();
    stream.seekp(reservedOffsetOffsets[label]);
    WriteValue<int32_t>(stream, offset);
    stream.seekp(currentPos);
    reservedOffsetOffsets.erase(label);
}


void BinaryReadWrite::Reserver::FillOffsetWithPosition(const string& label)
{
    return is64Bit
    ? FillOffset(label, stream.tellp())
    : FillOffset(label, static_cast<int32_t>(stream.tellp()));
}

void BinaryReadWrite::Reserver::FillOffsetWithRelativePosition(const string& label)
{
    if (relativePositionStart == 0)
        throw runtime_error("Cannot fill relative offset without setting relative position start.");

    const streampos relativePosition = stream.tellp() - relativePositionStart;
    return is64Bit
    ? FillOffset(label, relativePosition)
    : FillOffset(label, static_cast<int32_t>(relativePosition));
}


void BinaryReadWrite::Reserver::Finish()
{
    if (finished)
        throw runtime_error("Cannot call Finish() twice on Reserver.");

    if (reservedOffsetOffsets.empty())
    {
        // No offsets reserved, so we're done.
        finished = true;
        return;
    }

    string unfilledOffsets;
    for (const auto& [label, pos] : reservedOffsetOffsets)
    {
        unfilledOffsets += label + ", ";
    }

    throw runtime_error("Unfilled Reserver offsets: " + unfilledOffsets);
}
