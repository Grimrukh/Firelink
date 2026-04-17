#pragma once

#include <FirelinkFLVER/Material.h>
#include <FirelinkFLVER/VertexFormats.h>

#include <FirelinkCore/Collections.h>

#include <cstdint>
#include <vector>

namespace Firelink
{
    // A single field in a vertex layout. `usage` identifies what the field is
    // (Position, Normal, etc.) and `format` names the on-disk byte shape.
    // `decompressed_offset` and `decompressed_size` are computed on load.
    struct VertexDataType
    {
        VertexUsage usage = VertexUsage::Ignore;
        VertexDataFormatEnum format = VertexDataFormatEnum::Ignored;
        std::uint32_t instance_index = 0; // for multi-instance usages (uv_0, uv_1, color_0, ...)

        // On-disk preserved fields (round-trip fidelity).
        std::uint32_t unk_x00 = 0;
        std::uint32_t data_offset = 0; // byte offset of this field within the compressed vertex

        bool operator==(const VertexDataType&) const = default;

        [[nodiscard]] std::uint32_t CompressedSize() const noexcept // size on disk
        {
            return FormatEnumSize(format);
        }
    };

    struct VertexArrayLayout
    {
        std::vector<VertexDataType> types;

        // Cached totals. Filled in on read/build.
        std::uint32_t compressed_vertex_size = 0; // excludes Ignored fields on write; includes on read
        std::uint32_t decompressed_vertex_size = 0; // stride of the decompressed interleaved buffer

        bool operator==(const VertexArrayLayout&) const = default;
    };

    // Holds the decompressed vertex data for one Mesh as a single flat
    // interleaved byte buffer. Matches the numpy structured-array memory layout
    // in Python. Field views can be computed via `layout.types[i].data_offset`
    // (compressed) or tracked separately (decompressed).
    struct VertexArray
    {
        VertexArrayLayout layout;
        std::uint32_t vertex_count = 0;

        // Size: vertex_count * layout.decompressed_vertex_size.
        std::vector<std::byte> decompressed_data;

        bool operator==(const VertexArray&) const = default;
    };

    // --- FaceSetFlags -----------------------------------------------------------
    // Bit flags on FaceSet.flags (FLVER2 only).
    namespace FaceSetFlags
    {
        constexpr std::uint8_t LodLevel1 = 0b0000'0001;
        constexpr std::uint8_t LodLevel2 = 0b0000'0010;
        constexpr std::uint8_t EdgeCompressed = 0b0100'0000; // NOT supported for round-trip
        constexpr std::uint8_t MotionBlur = 0b1000'0000;
    }

    struct FaceSet
    {
        std::int32_t flags = 0;
        bool is_triangle_strip = false;
        bool use_backface_culling = true;
        std::int16_t unk_x06 = 0;

        // Flat vertex-index buffer. For non-strip face sets this is conceptually
        // a (triangle_count, 3) array; for strips it's a flat index sequence
        // (sentinels included). Always uint32 internally regardless of on-disk
        // bit size.
        std::vector<std::uint32_t> vertex_indices;

        bool operator==(const FaceSet&) const = default;
    };

    struct Mesh
    {
        bool is_dynamic = false;
        std::int32_t default_bone_index = 0;
        std::vector<std::int32_t> bone_indices; // may be empty in FLVER2

        // Denormalised: each mesh owns its material/vertex_arrays/face_sets.
        // Deduplication into global tables happens at write time.
        Material material;
        std::vector<VertexArray> vertex_arrays; // in practice always exactly one
        std::vector<FaceSet> face_sets;

        // FLVER2 bounding box. If `uses_bounding_boxes` is false, no bounding box
        // is written and the offset field is 0 on disk. `sekiro_bbox_unk` is
        // only present in Sekiro+ and encodes an unknown third vector.
        bool uses_bounding_boxes = true;
        Vector3 bounding_box_min = Vector3::SingleMax();
        Vector3 bounding_box_max = Vector3::SingleMin();
        std::optional<Vector3> sekiro_bbox_unk;

        // FLVER0-specific.
        std::uint16_t f0_unk_x46 = 0;

        // Convenience index into FLVER::meshes, set on load.
        std::int32_t index = -1;
        bool invalid_layout = false;

        bool operator==(const Mesh&) const = default;
    };
} // namespace Firelink
