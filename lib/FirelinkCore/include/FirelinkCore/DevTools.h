#pragma once

#include <FirelinkCore/BinaryReadWrite.h>
#include <FirelinkCore/Export.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

/// @file DevTools.h
/// @brief Binary reverse-engineering helpers for exploring unknown file formats.
///
/// All functions that produce text return a `std::string` so callers can route
/// output through Firelink::Info/Debug or dump it to a file.  Functions that
/// search a buffer accept a raw (data, size) pair so they work with any source —
/// a `BufferReader`, a raw `std::vector<std::byte>`, or a memory-mapped region.

namespace Firelink::DevTools
{
    // =========================================================================
    // HexDump
    // =========================================================================

    /// @brief Produce a hex dump of `length` bytes starting at `offset` inside
    ///        `data`.  Output mimics `xxd`: each line shows the absolute file
    ///        offset, 16 bytes of hex, and their printable-ASCII representation.
    ///
    /// @param data        Pointer to the raw byte buffer.
    /// @param dataSize    Total size of the buffer.
    /// @param offset      First byte to include in the dump (default 0).
    /// @param length      Number of bytes to dump (default: entire buffer).
    /// @param bytesPerRow Number of bytes per row (default 16).
    FIRELINK_CORE_API std::string HexDump(
        const std::byte* data,
        std::size_t      dataSize,
        std::size_t      offset     = 0,
        std::size_t      length     = std::string::npos,     // npos → dump to end
        std::size_t      bytesPerRow = 16);

    /// @brief Convenience overload that takes a `std::vector<std::byte>`.
    FIRELINK_CORE_API std::string HexDump(
        const std::vector<std::byte>& data,
        std::size_t offset      = 0,
        std::size_t length      = std::string::npos,
        std::size_t bytesPerRow = 16);

    /// @brief Convenience overload that calls `BufferReader::RawAt(0)`.
    FIRELINK_CORE_API std::string HexDump(
        BinaryReadWrite::BufferReader& reader,
        std::size_t offset      = 0,
        std::size_t length      = std::string::npos,
        std::size_t bytesPerRow = 16);

    // =========================================================================
    // InspectAt — "what could these bytes mean?"
    // =========================================================================

    /// @brief Print every scalar interpretation of the bytes starting at
    ///        `offset`: int8, uint8, int16 (LE/BE), uint16(LE/BE),
    ///        int32(LE/BE), uint32(LE/BE), int64(LE/BE), uint64(LE/BE),
    ///        float32(LE/BE), float64(LE/BE).
    ///
    ///        Rows for sizes that exceed the available bytes are skipped.
    FIRELINK_CORE_API std::string InspectAt(
        const std::byte* data,
        std::size_t      dataSize,
        std::size_t      offset);

    FIRELINK_CORE_API std::string InspectAt(
        const std::vector<std::byte>& data,
        std::size_t offset);

    FIRELINK_CORE_API std::string InspectAt(
        BinaryReadWrite::BufferReader& reader,
        std::size_t offset);

    // =========================================================================
    // FindPattern — byte-sequence search with optional wildcard mask
    // =========================================================================

    /// @brief Search `data` for all occurrences of `pattern`.
    ///
    /// @param pattern  Bytes to search for.
    /// @param mask     Optional per-byte mask.  A `0x00` entry means "ignore
    ///                 this byte" (wildcard); `0xFF` means "must match exactly".
    ///                 If empty, all bytes must match exactly.
    /// @return Sorted list of offsets where the pattern starts.
    FIRELINK_CORE_API std::vector<std::size_t> FindPattern(
        const std::byte*              data,
        std::size_t                   dataSize,
        const std::vector<std::byte>& pattern,
        const std::vector<uint8_t>&   mask = {});

    FIRELINK_CORE_API std::vector<std::size_t> FindPattern(
        const std::vector<std::byte>& data,
        const std::vector<std::byte>& pattern,
        const std::vector<uint8_t>&   mask = {});

    /// @brief Convenience: build a `pattern` / `mask` from an IDA-style string
    ///        such as `"48 8B ? ? 48 89"`.  Tokens `?` or `??` become wildcards.
    FIRELINK_CORE_API std::vector<std::size_t> FindPatternString(
        const std::byte* data,
        std::size_t      dataSize,
        const std::string& patternStr);

    // =========================================================================
    // FindValue — search for a specific numeric constant
    // =========================================================================

    /// @brief Find all offsets where a little-endian or big-endian `uint32_t`
    ///        value matching `value` appears in `data`.
    FIRELINK_CORE_API std::vector<std::size_t> FindUInt32(
        const std::byte* data,
        std::size_t      dataSize,
        uint32_t         value,
        BinaryReadWrite::Endian endian = BinaryReadWrite::Endian::Little);

    FIRELINK_CORE_API std::vector<std::size_t> FindUInt32(
        const std::vector<std::byte>& data,
        uint32_t                      value,
        BinaryReadWrite::Endian       endian = BinaryReadWrite::Endian::Little);

    FIRELINK_CORE_API std::vector<std::size_t> FindUInt64(
        const std::byte* data,
        std::size_t      dataSize,
        uint64_t         value,
        BinaryReadWrite::Endian endian = BinaryReadWrite::Endian::Little);

    FIRELINK_CORE_API std::vector<std::size_t> FindUInt64(
        const std::vector<std::byte>& data,
        uint64_t                      value,
        BinaryReadWrite::Endian       endian = BinaryReadWrite::Endian::Little);

    FIRELINK_CORE_API std::vector<std::size_t> FindFloat(
        const std::byte* data,
        std::size_t      dataSize,
        float            value,
        BinaryReadWrite::Endian endian = BinaryReadWrite::Endian::Little);

    FIRELINK_CORE_API std::vector<std::size_t> FindFloat(
        const std::vector<std::byte>& data,
        float                         value,
        BinaryReadWrite::Endian       endian = BinaryReadWrite::Endian::Little);

    // =========================================================================
    // FindStrings — scan for printable ASCII runs
    // =========================================================================

    struct FoundString
    {
        std::size_t offset;
        std::string text;
    };

    /// @brief Return all null-terminated ASCII runs of at least `minLength`
    ///        printable characters found anywhere in `data`.
    FIRELINK_CORE_API std::vector<FoundString> FindStrings(
        const std::byte* data,
        std::size_t      dataSize,
        std::size_t      minLength = 4);

    FIRELINK_CORE_API std::vector<FoundString> FindStrings(
        const std::vector<std::byte>& data,
        std::size_t minLength = 4);

    // =========================================================================
    // Diff — byte-level comparison of two buffers
    // =========================================================================

    struct DiffEntry
    {
        std::size_t offset;
        std::byte   lhs;
        std::byte   rhs;
    };

    /// @brief Return every offset where `lhs` and `rhs` differ.
    ///        If the buffers have different sizes the shorter one is padded with
    ///        0x00 for comparison purposes.
    FIRELINK_CORE_API std::vector<DiffEntry> Diff(
        const std::byte* lhs, std::size_t lhsSize,
        const std::byte* rhs, std::size_t rhsSize);

    FIRELINK_CORE_API std::vector<DiffEntry> Diff(
        const std::vector<std::byte>& lhs,
        const std::vector<std::byte>& rhs);

    /// @brief Render a `Diff` result as a human-readable string.
    FIRELINK_CORE_API std::string FormatDiff(const std::vector<DiffEntry>& diff);

    // =========================================================================
    // ByteHistogram — frequency analysis + Shannon entropy
    // =========================================================================

    struct ByteHistogram
    {
        uint64_t counts[256]{};   ///< count[b] = number of times byte b appears
        double   entropy = 0.0;  ///< Shannon entropy in bits (0–8)
    };

    /// @brief Compute a frequency histogram and Shannon entropy for `data`.
    FIRELINK_CORE_API ByteHistogram ComputeHistogram(
        const std::byte* data,
        std::size_t      dataSize);

    FIRELINK_CORE_API ByteHistogram ComputeHistogram(
        const std::vector<std::byte>& data);

    /// @brief Render the histogram as a text table showing the top N most
    ///        frequent byte values and the overall entropy.
    FIRELINK_CORE_API std::string FormatHistogram(
        const ByteHistogram& hist,
        std::size_t          topN = 16);

    // =========================================================================
    // ScanOffsetTable — heuristic pointer / offset table finder
    // =========================================================================

    struct OffsetCandidate
    {
        std::size_t tableOffset; ///< Where in the file this potential offset/pointer lives.
        uint64_t    value;       ///< The raw value read at that location.
    };

    /// @brief Scan `data[scanStart .. scanStart+scanLength)` reading consecutive
    ///        `valueSize`-byte (4 or 8) little-endian integers.  Returns entries
    ///        whose value falls within [`minTarget`, `maxTarget`] — typically
    ///        [0, fileSize) to find in-file pointers.
    ///
    /// @param valueSize    4 (uint32) or 8 (uint64).
    /// @param alignment    Only check offsets that are multiples of this value
    ///                     (default 4).
    FIRELINK_CORE_API std::vector<OffsetCandidate> ScanOffsetTable(
        const std::byte* data,
        std::size_t      dataSize,
        std::size_t      scanStart,
        std::size_t      scanLength,
        std::size_t      valueSize  = 4,
        uint64_t         minTarget  = 0,
        uint64_t         maxTarget  = UINT64_MAX,
        std::size_t      alignment  = 4);

    FIRELINK_CORE_API std::vector<OffsetCandidate> ScanOffsetTable(
        const std::vector<std::byte>& data,
        std::size_t scanStart,
        std::size_t scanLength,
        std::size_t valueSize  = 4,
        uint64_t    minTarget  = 0,
        uint64_t    maxTarget  = UINT64_MAX,
        std::size_t alignment  = 4);

    /// @brief Render offset candidates as a text table.
    FIRELINK_CORE_API std::string FormatOffsetCandidates(
        const std::vector<OffsetCandidate>& candidates);

} // namespace Firelink::DevTools

