#pragma once

#include <cstdint>

namespace Firelink::Havok
{
    /// @brief Bit flags stored per-type in the TBOD section, controlling which optional fields are present.
    /// Bit positions confirmed by cross-referencing known __tag_format_flags values in Python type definitions.
    enum class TagFormatFlags : uint32_t
    {
        None          = 0x00,
        SubType       = 0x01,  ///< tag_type_flags varint follows
        Pointer       = 0x02,  ///< pointer_type_index varint follows (only written when SubType lower nibble >= 6)
        Version       = 0x04,  ///< version varint follows
        ByteSize      = 0x08,  ///< byte_size + alignment varints follow
        AbstractValue = 0x10,  ///< abstract_value varint follows
        Members       = 0x20,  ///< member list follows
        Interfaces    = 0x40,  ///< interface list follows
        Unknown       = 0x80,  ///< unsupported flag; raising an error if encountered
    };

    inline constexpr TagFormatFlags operator|(TagFormatFlags a, TagFormatFlags b) noexcept
    {
        return static_cast<TagFormatFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    inline constexpr bool HasFlag(TagFormatFlags flags, TagFormatFlags bit) noexcept
    {
        return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(bit)) != 0;
    }

    /// @brief Lower-nibble type codes used in tag_type_flags. Values >= 6 indicate pointer-shaped types.
    enum class TagDataType : uint32_t
    {
        Void            = 0,
        Opaque          = 1,
        Bool            = 2,
        String          = 3,   ///< Null-terminated char*
        Int             = 4,
        Float           = 5,
        Pointer         = 6,   ///< Ptr_, hkRefPtr_, hkViewPtr_  (tag_type_flags = 6)
        SparseArray     = 7,
        Array           = 8,   ///< hkArray_                      (tag_type_flags = 8)
        InplaceArray    = 9,
        Enum            = 10,
        Struct          = 11,  ///< Fixed-length inline array / tuple (hkStruct_)
        SimpleArray     = 12,
        HomogenousArray = 13,
        Variant         = 14,
        CharArray       = 15,  ///< String item (hkStringPtr)
        Class           = 64,  ///< 0x40 — concrete hk class objects
    };

    /// @brief Access and serialization flags stored per member in TBOD.
    /// All member flags have a base value >= 32.
    enum class MemberFlags : uint32_t
    {
        Default   = 32,   ///< Public / no special access (0x20 base)
        Private   = 34,   ///< Private member  (32 | 0x02)
        Protected = 36,   ///< Protected member (32 | 0x04)
        NotOwned  = 32,   ///< Alias: public, storage not owned by container
    };
}