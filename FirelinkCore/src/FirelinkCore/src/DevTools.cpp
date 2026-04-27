#include <FirelinkCore/DevTools.h>

#include <algorithm>
#include <bit>
#include <cassert>
#include <cctype>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>

using namespace Firelink::DevTools;
using Firelink::BinaryReadWrite::BufferReader;
using Firelink::BinaryReadWrite::Endian;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace
{
    // Format a single byte as two hex digits.
    void WriteHex2(std::ostringstream& ss, uint8_t b)
    {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(b);
    }

    // Byte-swap helpers (little-endian host assumed — same as BufferReader).
    template <typename T>
    T LERead(const std::byte* p)
    {
        T v;
        std::memcpy(&v, p, sizeof(T));
        return v;
    }

    template <typename T>
    T BERead(const std::byte* p)
    {
        T v;
        std::memcpy(&v, p, sizeof(T));
        // Reverse bytes for big-endian interpretation on a LE host.
        if constexpr (sizeof(T) > 1)
        {
            auto* b = reinterpret_cast<uint8_t*>(&v);
            std::reverse(b, b + sizeof(T));
        }
        return v;
    }
} // anonymous namespace

// ---------------------------------------------------------------------------
// HexDump
// ---------------------------------------------------------------------------

std::string Firelink::DevTools::HexDump(
    const std::byte* data,
    const std::size_t dataSize,
    const std::size_t offset,
    std::size_t length,
    const std::size_t bytesPerRow)
{
    if (offset >= dataSize)
        return {};

    if (length == std::string::npos || offset + length > dataSize)
        length = dataSize - offset;

    std::ostringstream ss;
    ss << std::dec; // reset to decimal for counts below

    for (std::size_t row = 0; row < length; row += bytesPerRow)
    {
        // Offset column.
        const std::size_t rowStart = offset + row;
        ss << std::hex << std::setw(8) << std::setfill('0') << rowStart << "  ";

        const std::size_t rowEnd = std::min(row + bytesPerRow, length);

        // Hex columns.
        for (std::size_t i = row; i < rowEnd; ++i)
        {
            WriteHex2(ss, static_cast<uint8_t>(data[offset + i]));
            ss << ' ';
            if (i - row == 7)
                ss << ' '; // extra space between 8-byte halves
        }

        // Pad if the last row is short.
        const std::size_t missing = bytesPerRow - (rowEnd - row);
        for (std::size_t i = 0; i < missing; ++i)
        {
            ss << "   ";
            if (i + (rowEnd - row) == 7)
                ss << ' ';
        }

        // ASCII column.
        ss << " |";
        for (std::size_t i = row; i < rowEnd; ++i)
        {
            const auto ch = static_cast<unsigned char>(data[offset + i]);
            ss << (std::isprint(ch) ? static_cast<char>(ch) : '.');
        }
        ss << "|\n";
    }
    return ss.str();
}

std::string Firelink::DevTools::HexDump(
    const std::vector<std::byte>& data,
    const std::size_t offset,
    const std::size_t length,
    const std::size_t bytesPerRow)
{
    return HexDump(data.data(), data.size(), offset, length, bytesPerRow);
}

std::string Firelink::DevTools::HexDump(
    BufferReader& reader,
    const std::size_t offset,
    const std::size_t length,
    const std::size_t bytesPerRow)
{
    return HexDump(reader.RawAt(0), reader.Size(), offset, length, bytesPerRow);
}

// ---------------------------------------------------------------------------
// InspectAt
// ---------------------------------------------------------------------------

std::string Firelink::DevTools::InspectAt(
    const std::byte* data,
    const std::size_t dataSize,
    const std::size_t offset)
{
    if (offset >= dataSize)
        return "(InspectAt: offset past end of buffer)\n";

    const std::size_t avail = dataSize - offset;
    const std::byte* p = data + offset;
    std::ostringstream ss;

    ss << "InspectAt 0x" << std::hex << std::setw(8) << std::setfill('0') << offset
       << "  (" << std::dec << avail << " bytes remaining)\n";

    // Raw hex preview (up to 8 bytes).
    ss << "  raw:";
    for (std::size_t i = 0; i < std::min(avail, std::size_t{8}); ++i)
    {
        ss << ' ';
        WriteHex2(ss, static_cast<uint8_t>(p[i]));
    }
    ss << '\n';

    // 1-byte types.
    if (avail >= 1)
    {
        ss << "  int8   = " << std::dec << static_cast<int>(static_cast<int8_t>(p[0]))       << '\n';
        ss << "  uint8  = " << std::dec << static_cast<int>(static_cast<uint8_t>(p[0]))      << '\n';
    }

    // 2-byte types.
    if (avail >= 2)
    {
        ss << "  int16  LE = " << std::dec << LERead<int16_t>(p)  << "  |  BE = " << BERead<int16_t>(p)  << '\n';
        ss << "  uint16 LE = " << std::dec << LERead<uint16_t>(p) << "  |  BE = " << BERead<uint16_t>(p) << '\n';
    }

    // 4-byte types.
    if (avail >= 4)
    {
        ss << "  int32  LE = " << std::dec << LERead<int32_t>(p)  << "  |  BE = " << BERead<int32_t>(p)  << '\n';
        ss << "  uint32 LE = " << std::dec << LERead<uint32_t>(p) << "  |  BE = " << BERead<uint32_t>(p) << '\n';
        float f_le = LERead<float>(p), f_be = BERead<float>(p);
        ss << "  float  LE = " << f_le << "  |  BE = " << f_be << '\n';
        ss << "  hex32  LE = 0x" << std::hex << std::setw(8) << std::setfill('0') << LERead<uint32_t>(p) << '\n';
    }

    // 8-byte types.
    if (avail >= 8)
    {
        ss << "  int64  LE = " << std::dec << LERead<int64_t>(p)  << "  |  BE = " << BERead<int64_t>(p)  << '\n';
        ss << "  uint64 LE = " << std::dec << LERead<uint64_t>(p) << "  |  BE = " << BERead<uint64_t>(p) << '\n';
        double d_le = LERead<double>(p), d_be = BERead<double>(p);
        ss << "  double LE = " << d_le << "  |  BE = " << d_be << '\n';
        ss << "  hex64  LE = 0x" << std::hex << std::setw(16) << std::setfill('0') << LERead<uint64_t>(p) << '\n';
    }

    return ss.str();
}

std::string Firelink::DevTools::InspectAt(
    const std::vector<std::byte>& data,
    const std::size_t offset)
{
    return InspectAt(data.data(), data.size(), offset);
}

std::string Firelink::DevTools::InspectAt(
    BufferReader& reader,
    const std::size_t offset)
{
    return InspectAt(reader.RawAt(0), reader.Size(), offset);
}

// ---------------------------------------------------------------------------
// FindPattern
// ---------------------------------------------------------------------------

std::vector<std::size_t> Firelink::DevTools::FindPattern(
    const std::byte* data,
    const std::size_t dataSize,
    const std::vector<std::byte>& pattern,
    const std::vector<uint8_t>& mask)
{
    std::vector<std::size_t> hits;
    if (pattern.empty() || dataSize < pattern.size())
        return hits;

    const std::size_t patLen = pattern.size();
    const bool hasMask = !mask.empty();

    for (std::size_t i = 0; i <= dataSize - patLen; ++i)
    {
        bool match = true;
        for (std::size_t j = 0; j < patLen; ++j)
        {
            if (hasMask && mask[j] == 0x00)
                continue; // wildcard
            if (data[i + j] != pattern[j])
            {
                match = false;
                break;
            }
        }
        if (match)
            hits.push_back(i);
    }
    return hits;
}

std::vector<std::size_t> Firelink::DevTools::FindPattern(
    const std::vector<std::byte>& data,
    const std::vector<std::byte>& pattern,
    const std::vector<uint8_t>& mask)
{
    return FindPattern(data.data(), data.size(), pattern, mask);
}

std::vector<std::size_t> Firelink::DevTools::FindPatternString(
    const std::byte* data,
    const std::size_t dataSize,
    const std::string& patternStr)
{
    std::vector<std::byte> pat;
    std::vector<uint8_t> mask;

    std::istringstream iss(patternStr);
    std::string token;
    while (iss >> token)
    {
        if (token == "?" || token == "??")
        {
            pat.push_back(std::byte{0});
            mask.push_back(0x00); // wildcard
        }
        else
        {
            // Parse as hex byte.
            const auto val = static_cast<uint8_t>(std::stoul(token, nullptr, 16));
            pat.push_back(static_cast<std::byte>(val));
            mask.push_back(0xFF); // must match
        }
    }
    return FindPattern(data, dataSize, pat, mask);
}

// ---------------------------------------------------------------------------
// FindValue
// ---------------------------------------------------------------------------

namespace
{
    template <typename T>
    std::vector<std::size_t> FindValueImpl(
        const std::byte* data,
        const std::size_t dataSize,
        const T value,
        const Endian endian)
    {
        std::vector<std::size_t> hits;
        if (dataSize < sizeof(T))
            return hits;

        // Encode the target as a byte pattern.
        T target = value;
        if (endian == Endian::Big)
        {
            auto* b = reinterpret_cast<uint8_t*>(&target);
            std::reverse(b, b + sizeof(T));
        }

        std::byte pat[sizeof(T)];
        std::memcpy(pat, &target, sizeof(T));

        for (std::size_t i = 0; i <= dataSize - sizeof(T); ++i)
        {
            if (std::memcmp(data + i, pat, sizeof(T)) == 0)
                hits.push_back(i);
        }
        return hits;
    }
}

std::vector<std::size_t> Firelink::DevTools::FindUInt32(
    const std::byte* data, const std::size_t dataSize, const uint32_t value, const Endian endian)
{
    return FindValueImpl<uint32_t>(data, dataSize, value, endian);
}

std::vector<std::size_t> Firelink::DevTools::FindUInt32(
    const std::vector<std::byte>& data, const uint32_t value, const Endian endian)
{
    return FindUInt32(data.data(), data.size(), value, endian);
}

std::vector<std::size_t> Firelink::DevTools::FindUInt64(
    const std::byte* data, const std::size_t dataSize, const uint64_t value, const Endian endian)
{
    return FindValueImpl<uint64_t>(data, dataSize, value, endian);
}

std::vector<std::size_t> Firelink::DevTools::FindUInt64(
    const std::vector<std::byte>& data, const uint64_t value, const Endian endian)
{
    return FindUInt64(data.data(), data.size(), value, endian);
}

std::vector<std::size_t> Firelink::DevTools::FindFloat(
    const std::byte* data, const std::size_t dataSize, const float value, const Endian endian)
{
    return FindValueImpl<float>(data, dataSize, value, endian);
}

std::vector<std::size_t> Firelink::DevTools::FindFloat(
    const std::vector<std::byte>& data, const float value, const Endian endian)
{
    return FindFloat(data.data(), data.size(), value, endian);
}

// ---------------------------------------------------------------------------
// FindStrings
// ---------------------------------------------------------------------------

std::vector<FoundString> Firelink::DevTools::FindStrings(
    const std::byte* data,
    const std::size_t dataSize,
    const std::size_t minLength)
{
    std::vector<FoundString> result;
    std::size_t i = 0;

    while (i < dataSize)
    {
        if (std::isprint(static_cast<unsigned char>(data[i])))
        {
            const std::size_t start = i;
            std::string run;
            while (i < dataSize && std::isprint(static_cast<unsigned char>(data[i])))
            {
                run += static_cast<char>(data[i]);
                ++i;
            }
            if (run.size() >= minLength)
                result.push_back({start, std::move(run)});
        }
        else
        {
            ++i;
        }
    }
    return result;
}

std::vector<FoundString> Firelink::DevTools::FindStrings(
    const std::vector<std::byte>& data,
    const std::size_t minLength)
{
    return FindStrings(data.data(), data.size(), minLength);
}

// ---------------------------------------------------------------------------
// Diff
// ---------------------------------------------------------------------------

std::vector<DiffEntry> Firelink::DevTools::Diff(
    const std::byte* lhs, const std::size_t lhsSize,
    const std::byte* rhs, const std::size_t rhsSize)
{
    std::vector<DiffEntry> result;
    const std::size_t maxSize = std::max(lhsSize, rhsSize);

    for (std::size_t i = 0; i < maxSize; ++i)
    {
        const std::byte l = i < lhsSize ? lhs[i] : std::byte{0};
        const std::byte r = i < rhsSize ? rhs[i] : std::byte{0};
        if (l != r)
            result.push_back({i, l, r});
    }
    return result;
}

std::vector<DiffEntry> Firelink::DevTools::Diff(
    const std::vector<std::byte>& lhs,
    const std::vector<std::byte>& rhs)
{
    return Diff(lhs.data(), lhs.size(), rhs.data(), rhs.size());
}

std::string Firelink::DevTools::FormatDiff(const std::vector<DiffEntry>& diff)
{
    if (diff.empty())
        return "(no differences)\n";

    std::ostringstream ss;
    ss << std::dec << diff.size() << " difference(s):\n";
    ss << "  Offset      LHS   RHS\n";
    ss << "  ----------  ----  ----\n";

    // Group consecutive offsets into runs for compact display.
    std::size_t i = 0;
    while (i < diff.size())
    {
        const std::size_t runStart = diff[i].offset;
        std::size_t j = i;
        while (j + 1 < diff.size() && diff[j + 1].offset == diff[j].offset + 1)
            ++j;
        const std::size_t runEnd = diff[j].offset;

        if (runStart == runEnd)
        {
            ss << "  0x";
            ss << std::hex << std::setw(8) << std::setfill('0') << runStart;
            ss << "    ";
            WriteHex2(ss, static_cast<uint8_t>(diff[i].lhs));
            ss << "    ";
            WriteHex2(ss, static_cast<uint8_t>(diff[i].rhs));
            ss << '\n';
        }
        else
        {
            ss << "  0x" << std::hex << std::setw(8) << std::setfill('0') << runStart
               << "–0x" << std::hex << std::setw(8) << std::setfill('0') << runEnd
               << "  (" << std::dec << (runEnd - runStart + 1) << " bytes)\n";
        }
        i = j + 1;
    }
    return ss.str();
}

// ---------------------------------------------------------------------------
// ByteHistogram
// ---------------------------------------------------------------------------

ByteHistogram Firelink::DevTools::ComputeHistogram(
    const std::byte* data,
    const std::size_t dataSize)
{
    ByteHistogram hist{};
    for (std::size_t i = 0; i < dataSize; ++i)
        ++hist.counts[static_cast<uint8_t>(data[i])];

    if (dataSize == 0)
        return hist;

    // Shannon entropy H = -sum p*log2(p).
    double entropy = 0.0;
    for (int b = 0; b < 256; ++b)
    {
        if (hist.counts[b] == 0)
            continue;
        const double p = static_cast<double>(hist.counts[b]) / static_cast<double>(dataSize);
        entropy -= p * std::log2(p);
    }
    hist.entropy = entropy;
    return hist;
}

ByteHistogram Firelink::DevTools::ComputeHistogram(
    const std::vector<std::byte>& data)
{
    return ComputeHistogram(data.data(), data.size());
}

std::string Firelink::DevTools::FormatHistogram(
    const ByteHistogram& hist,
    const std::size_t topN)
{
    // Sort indices by frequency, descending.
    std::array<int, 256> order{};
    for (int i = 0; i < 256; ++i) order[i] = i;
    std::stable_sort(order.begin(), order.end(),
        [&hist](const int a, const int b) { return hist.counts[a] > hist.counts[b]; });

    const uint64_t total = [&]
    {
        uint64_t s = 0;
        for (const auto c : hist.counts) s += c;
        return s;
    }();

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(3);
    ss << "Shannon entropy: " << hist.entropy << " bits/byte  (";
    ss << std::setprecision(1) << (hist.entropy / 8.0 * 100.0) << "% of maximum)\n\n";
    ss << "Top " << topN << " byte values:\n";
    ss << "  Value   Count       Freq\n";
    ss << "  ------  ----------  --------\n";

    for (std::size_t i = 0; i < std::min(topN, std::size_t{256}); ++i)
    {
        if (hist.counts[order[i]] == 0)
            break;
        const double freq = total > 0
            ? static_cast<double>(hist.counts[order[i]]) / static_cast<double>(total) * 100.0
            : 0.0;
        ss << "  0x";
        ss << std::hex << std::setw(2) << std::setfill('0') << order[i];
        ss << "    ";
        ss << std::dec << std::setw(10) << hist.counts[order[i]];
        ss << "  ";
        ss << std::fixed << std::setprecision(2) << std::setw(7) << freq << "%\n";
    }
    return ss.str();
}

// ---------------------------------------------------------------------------
// ScanOffsetTable
// ---------------------------------------------------------------------------

std::vector<OffsetCandidate> Firelink::DevTools::ScanOffsetTable(
    const std::byte* data,
    const std::size_t dataSize,
    const std::size_t scanStart,
    const std::size_t scanLength,
    const std::size_t valueSize,
    const uint64_t minTarget,
    const uint64_t maxTarget,
    const std::size_t alignment)
{
    if (valueSize != 4 && valueSize != 8)
        throw std::invalid_argument("ScanOffsetTable: valueSize must be 4 or 8");
    if (alignment == 0)
        throw std::invalid_argument("ScanOffsetTable: alignment must be > 0");

    std::vector<OffsetCandidate> result;

    const std::size_t scanEnd = std::min(scanStart + scanLength, dataSize);

    for (std::size_t off = scanStart; off + valueSize <= scanEnd; off += alignment)
    {
        uint64_t v = 0;
        if (valueSize == 4)
            v = static_cast<uint64_t>(LERead<uint32_t>(data + off));
        else
            v = LERead<uint64_t>(data + off);

        if (v >= minTarget && v <= maxTarget)
            result.push_back({off, v});
    }
    return result;
}

std::vector<OffsetCandidate> Firelink::DevTools::ScanOffsetTable(
    const std::vector<std::byte>& data,
    const std::size_t scanStart,
    const std::size_t scanLength,
    const std::size_t valueSize,
    const uint64_t minTarget,
    const uint64_t maxTarget,
    const std::size_t alignment)
{
    return ScanOffsetTable(
        data.data(), data.size(),
        scanStart, scanLength,
        valueSize, minTarget, maxTarget, alignment);
}

std::string Firelink::DevTools::FormatOffsetCandidates(
    const std::vector<OffsetCandidate>& candidates)
{
    if (candidates.empty())
        return "(no offset candidates found)\n";

    std::ostringstream ss;
    ss << candidates.size() << " candidate(s):\n";
    ss << "  TableOffset  Value\n";
    ss << "  -----------  ------------------\n";
    for (const auto& c : candidates)
    {
        ss << "  0x" << std::hex << std::setw(8) << std::setfill('0') << c.tableOffset;
        ss << "   0x" << std::hex << std::setw(16) << std::setfill('0') << c.value;
        ss << '\n';
    }
    return ss.str();
}

