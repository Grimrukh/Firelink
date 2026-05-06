// flver/vertex_formats.hpp
//
// Vertex-field usage enum + per-(usage, format) codec table.
//
// The Python port encodes this as a dict-of-dicts on each `VertexDataType`
// subclass; in C++ we lift the same information into a flat lookup table.
//
// DESIGN NOTE — multi-field entries:
//
// Some (usage, format) pairs decompose into multiple sub-fields with
// independent codecs. For example, Normal format 0x10 is 3 unsigned bytes
// (normal) with a signed-to-float codec, plus 1 unsigned byte (normal_w)
// with an identity codec. The table returns a `VertexFormatInfo` that
// contains 1-4 `VertexFieldSpec` sub-fields. The vertex reader processes
// each sub-field independently.
//
// The user explicitly rejected a per-vertex `Vertex` class. Vertex data is
// stored as a single flat interleaved buffer matching the decompressed
// layout, identical to how Python's numpy structured arrays work.

#pragma once

#include <FirelinkFLVER/Export.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace Firelink
{
    // --- VertexDataFormatEnum ---------------------------------------------------
    //
    // These raw format codes appear in each VertexDataType header inside a
    // VertexArrayLayout. Each code maps to (a) a fixed on-disk byte layout and
    // (b) a compression codec that converts between the packed representation
    // and the user-facing float/int values.
    //
    // The authoritative mapping is driven by the table in vertex_formats.cpp;
    // this enum is intentionally named by numeric value to avoid inventing
    // semantics the Python source doesn't commit to. Do not reorder.
    enum class VertexDataFormatEnum : std::uint32_t
    {
        // 2/3/4-component floats
        TwoFloats = 0x01,
        ThreeFloats = 0x02,
        FourFloats = 0x03,

        // 4-component byte packings
        FourBytesA = 0x10,
        FourBytesB = 0x11,
        FourBytesC = 0x13,

        // Normal-with-W byte packing (1 byte W + 3 signed bytes)
        FourBytesD_NormalW = 0x12,

        // UV / "short-pair" formats
        TwoShorts = 0x15,
        FourShorts = 0x16,

        // 4-component short packings used for bone indices or short-to-float UVs
        FourShortsBones = 0x18,
        FourShortsToFloats = 0x1A,

        // 4-component short/byte pairs with float decompress
        FourShortsToFloatsB = 0x2E,
        FourBytesE = 0x2F,

        // Special cases
        EdgeCompressed = 0xF0, // unsupported
        Ignored = 0xFF,
    };

    constexpr bool IsSupportedFormat(VertexDataFormatEnum f) noexcept
    {
        return f != VertexDataFormatEnum::EdgeCompressed;
    }

    // What role a vertex-layout field plays.
    enum class VertexUsage : std::uint8_t
    {
        Position,
        BoneWeights,
        BoneIndices,
        Normal,
        Tangent,
        Bitangent,
        Color,
        UV,
        Ignore,
    };

    // Maps Python's type_int values to C++ VertexUsage.
    constexpr VertexUsage ToVertexUsage(const std::uint32_t type_int)
    {
        switch (type_int)
        {
        case 0: return VertexUsage::Position;
        case 1: return VertexUsage::BoneWeights;
        case 2: return VertexUsage::BoneIndices;
        case 3: return VertexUsage::Normal;
        case 5: return VertexUsage::UV;
        case 6: return VertexUsage::Tangent;
        case 7: return VertexUsage::Bitangent;
        case 10: return VertexUsage::Color;
        default: return VertexUsage::Ignore;
        }
    }

    // Reverse of ToVertexUsage — maps C++ VertexUsage back to Python's type_int for writing.
    constexpr std::uint32_t FromVertexUsage(const VertexUsage u)
    {
        switch (u)
        {
        case VertexUsage::Position:    return 0;
        case VertexUsage::BoneWeights: return 1;
        case VertexUsage::BoneIndices: return 2;
        case VertexUsage::Normal:      return 3;
        case VertexUsage::UV:          return 5;
        case VertexUsage::Tangent:     return 6;
        case VertexUsage::Bitangent:   return 7;
        case VertexUsage::Color:       return 10;
        default:                       return 0xFFFFFFFF;
        }
    }

    // Primitive scalar types for vertex fields.
    enum class VertexScalarType : std::uint8_t
    {
        U8, S8, U16, S16, U32, S32, F32,
    };

    constexpr std::size_t GetScalarSize(const VertexScalarType t) noexcept
    {
        switch (t)
        {
        case VertexScalarType::U8:
        case VertexScalarType::S8: return 1;
        case VertexScalarType::U16:
        case VertexScalarType::S16: return 2;
        case VertexScalarType::U32:
        case VertexScalarType::S32:
        case VertexScalarType::F32: return 4;
        }
        return 0;
    }

    // Codec identifier — maps to a compress/decompress operation.
    enum class VertexCodec : std::uint8_t
    {
        Identity, // no-op (same type, same values)
        IdentityWiden, // no math but wider decompressed type (byte->int32, short->int32)
        IntTo127Float, // compressed / 127.0
        IntTo255Float, // compressed / 255.0
        IntTo32767Float, // compressed / 32767.0
        SignedIntTo127Float, // (compressed - 127) / 127.0
        SignedIntTo255Float, // (compressed - 255) / 255.0
        SignedIntTo32767Float, // (compressed - 32767) / 32767.0
        UvFactor, // compressed / uv_factor (1024 or 2048)
    };

    // One sub-field within a vertex format entry. A single VertexDataType can
    // decompose into 1-4 sub-fields (e.g. Normal = normal(3) + normal_w(1)).
    struct VertexFieldSpec
    {
        const char* name = ""; // e.g. "normal", "normal_w", "uv"
        VertexScalarType compressed_scalar = VertexScalarType::U8;
        std::uint8_t compressed_count = 0;
        VertexScalarType decompressed_scalar = VertexScalarType::U8;
        std::uint8_t decompressed_count = 0;
        VertexCodec codec = VertexCodec::Identity;

        [[nodiscard]] constexpr std::size_t GetCompressedSize() const noexcept
        {
            return GetScalarSize(compressed_scalar) * compressed_count;
        }

        [[nodiscard]] constexpr std::size_t GetDecompressedSize() const noexcept
        {
            return GetScalarSize(decompressed_scalar) * decompressed_count;
        }
    };

    // Full description of a (usage, format) pair, possibly with multiple sub-fields.
    struct VertexFormatInfo
    {
        std::uint8_t field_count = 0;
        std::array<VertexFieldSpec, 4> fields{}; // at most 4 sub-fields

        [[nodiscard]] constexpr std::size_t GetTotalCompressedSize() const noexcept
        {
            std::size_t s = 0;
            for (std::uint8_t i = 0; i < field_count; ++i) s += fields[i].GetCompressedSize();
            return s;
        }

        [[nodiscard]] constexpr std::size_t GetTotalDecompressedSize() const noexcept
        {
            std::size_t s = 0;
            for (std::uint8_t i = 0; i < field_count; ++i) s += fields[i].GetDecompressedSize();
            return s;
        }
    };

    // Look up the format info for a given (usage, format) pair. Returns nullopt
    // if not recognised.
    FIRELINK_FLVER_API std::optional<VertexFormatInfo>
    FindVertexFormatInfo(VertexUsage usage, VertexDataFormatEnum format) noexcept;

    // Get the on-disk size for a VertexDataFormatEnum. Used when the full
    // format info is not needed (e.g. for VertexIgnore entries).
    FIRELINK_FLVER_API std::uint32_t FormatEnumSize(VertexDataFormatEnum format) noexcept;

    // Decompress a single sub-field across all vertices.
    // `src` points to the start of the compressed vertex buffer;
    // `dst` is the tightly-packed decompressed output for this sub-field.
    // `compressed_stride` is the full compressed vertex stride.
    // `compressed_offset` is the byte offset of this sub-field within one compressed vertex.
    FIRELINK_FLVER_API void DecompressField(
        const VertexFieldSpec& spec,
        const std::byte* src,
        std::byte* dst,
        std::size_t vertex_count,
        std::size_t compressed_stride,
        std::size_t compressed_offset,
        float uv_factor
    );

    // Inverse of decompress_field — used by the writer.
    FIRELINK_FLVER_API void CompressField(
        const VertexFieldSpec& spec,
        const std::byte* src,
        std::byte* dst,
        std::size_t vertex_count,
        std::size_t compressed_stride,
        std::size_t compressed_offset,
        float uv_factor
    );
} // namespace Firelink
