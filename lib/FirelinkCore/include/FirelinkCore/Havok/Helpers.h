#pragma once

#include <FirelinkCore/BinaryReadWrite.h>
#include <FirelinkCore/Havok/Enums.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace Firelink::Havok
{

    // =========================================================================
    // Type metadata — populated from the TYPE/TBOD section of a tagfile
    // =========================================================================

    struct TemplateInfo
    {
        std::string name;   ///< e.g. "tT", "tTYPE", "vN"
        int         value = 0;  ///< type index (if isType()) or integer constant

        [[nodiscard]] bool isType() const noexcept { return !name.empty() && name[0] == 't'; }
    };

    struct MemberInfo
    {
        std::string name;
        uint32_t    flags      = 0;
        uint32_t    offset     = 0;   ///< byte offset within the owning type's DATA region
        int         typeIndex  = 0;   ///< index into the file's TypeInfo list
    };

    struct InterfaceInfo
    {
        int      typeIndex = 0;
        uint32_t flags     = 0;
    };

    /// @brief Metadata for one Havok type, built while parsing the TYPE section.
    struct TypeInfo
    {
        std::string name;

        std::vector<TemplateInfo>  templates;
        std::vector<MemberInfo>    members;
        std::vector<InterfaceInfo> interfaces;

        int      parentTypeIndex  = 0;
        uint32_t tagFormatFlags   = 0;  ///< TagFormatFlags bitmask
        uint32_t tagTypeFlags     = 0;  ///< Lower nibble = TagDataType
        uint32_t version          = 0;
        uint32_t byteSize         = 0;
        uint32_t alignment        = 0;
        uint32_t abstractValue    = 0;
        int      pointerTypeIndex = 0;  ///< For pointer/array types, index of pointed-to type
        uint32_t hsh              = 0;  ///< Hash from THSH section (0 = not set)

        [[nodiscard]] bool IsPointerShaped() const noexcept
        {
            return (tagTypeFlags & 0xFu) >= 6u;
        }
    };

    // =========================================================================
    // Item descriptor — populated from the INDX/ITEM section
    // =========================================================================

    struct TagFileItem
    {
        int    typeIndex          = 0;
        size_t absoluteDataOffset = 0;  ///< absolute byte offset in DATA section
        int    length             = 0;  ///< element count (1 for ptr items, N for array items)
        bool   isPtr              = false;

        [[nodiscard]] bool IsNull() const noexcept { return typeIndex == 0; }
    };

    // =========================================================================
    // Tagfile section helpers
    // =========================================================================

    /// @brief Describes the data region of a parsed tagfile section.
    struct SectionView
    {
        size_t start = 0;  ///< absolute reader offset of the first data byte (after 8-byte header)
        size_t end   = 0;  ///< absolute reader offset of the first byte after this section

        [[nodiscard]] size_t DataSize() const noexcept { return end > start ? end - start : 0; }
    };

    /// @brief Read the big-endian section header (4-byte size + 4-byte magic) and validate magic.
    /// Advances reader past the 8-byte header.  The returned SectionView marks the data range.
    ///
    /// @param reader  BufferReader positioned at the start of the section header.
    /// @param magic1  Expected magic string (exactly 4 chars).
    /// @param magic2  Optional second accepted magic (for dual-magic sections like TBOD/TBDY).
    /// @returns SectionView containing [start, end) of the section data.
    /// @throws std::runtime_error if the magic does not match.
    SectionView EnterSection(
        BinaryReadWrite::BufferReader& reader,
        std::string_view magic1,
        std::string_view magic2 = {});

    /// @brief Peek at the next section's magic WITHOUT advancing the reader.
    /// @returns true if the next section header is present and its magic matches.
    bool PeekSection(
        const BinaryReadWrite::BufferReader& reader,
        std::string_view magic);

    // =========================================================================
    // Variable-length integer  (Havok tagfile varint encoding)
    // =========================================================================

    /// @brief Read a Havok tagfile variable-length unsigned integer from `reader`.
    ///
    /// Encoding (from Python `unpack_var_int`):
    ///   byte_1 & 0x80 == 0  → 7-bit  (1 byte)
    ///   byte_1 == 0xC3      → special: next 2 bytes big-endian
    ///   marker = byte_1>>3:
    ///     0x10..0x17  → 14-bit (2 bytes big-endian, mask 0x3FFF)
    ///     0x18..0x1B  → 21-bit (3 bytes big-endian, mask 0x1FFFFF)
    ///     0x1C        → 27-bit (re-read byte_1 + 3 more as LE uint32, mask 0x07FFFFFF)
    ///     0x1D        → 35-bit (byte_1 + 4 more bytes big-endian, mask 0x07FFFFFFFF)
    ///     0x1E        → 59-bit (re-read byte_1 + 7 more as LE uint64, mask 0x07FFFFFFFFFFFFFF)
    uint64_t ReadVarInt(BinaryReadWrite::BufferReader& reader);

    /// @brief Write a Havok tagfile variable-length unsigned integer to `writer`.
    ///
    /// This is the write-side inverse of `ReadVarInt` (from Python `pack_var_int`):
    ///   value < 0x80       → 1 byte
    ///   value < 0x4000     → 2 bytes big-endian (| 0x8000)
    ///   value < 0x200000   → 3 bytes big-endian (high | 0xC0 + low 16-bit BE)
    ///   value < 0x8000000  → 4 bytes big-endian (| 0xE0000000)
    void WriteVarInt(BinaryReadWrite::BufferWriter& writer, uint64_t value);

} // namespace Firelink::Havok

