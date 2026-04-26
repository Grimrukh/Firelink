#include <FirelinkFLVER/Mesh.h>

#include <FirelinkFLVER/Material.h>

#include <FirelinkCore/BinaryReadWrite.h>

namespace Firelink
{
    using namespace BinaryReadWrite;

    // Write a VertexDataType struct (20 bytes).
    void VertexDataType::Write(BufferWriter& w) const
    {
        w.Write<std::uint32_t>(unk_x00);
        w.Write<std::uint32_t>(data_offset);
        w.Write<std::uint32_t>(static_cast<std::uint32_t>(format));
        w.Write<std::uint32_t>(FromVertexUsage(usage));
        w.Write<std::uint32_t>(instance_index);
    }

    MeshReadState MeshReadState::ReadFLVER2(
        BufferReader& r,
        const bool bounding_box_has_unknown,
        const std::int32_t index)
    {
        MeshReadState state;
        auto& m = state.mesh;
        m.index = index;

        m.is_dynamic = r.Read<std::uint8_t>() != 0;
        r.AssertPad(3);
        state.material_index = r.Read<std::uint32_t>();
        r.AssertPad(8);
        m.default_bone_index = r.Read<std::int32_t>();
        const auto bone_count = r.Read<std::uint32_t>();
        const auto bounding_box_offset = r.Read<std::uint32_t>();
        const auto bone_offset = r.Read<std::uint32_t>();
        const auto face_set_count = r.Read<std::uint32_t>();
        const auto face_set_offset = r.Read<std::uint32_t>();
        const auto vertex_array_count = r.Read<std::uint32_t>();
        if (vertex_array_count < 1 || vertex_array_count > 3)
        {
            throw FLVERReadError(
                "FLVER2 mesh vertex_array_count: expected 1-3, got "
                + std::to_string(vertex_array_count));
        }
        const auto vertex_array_offset = r.Read<std::uint32_t>();

        // Bone indices.
        if (bone_count > 0)
        {
            auto guard = r.TempOffset(bone_offset);
            m.bone_indices.resize(bone_count);
            for (std::uint32_t i = 0; i < bone_count; ++i)
            {
                m.bone_indices[i] = r.Read<std::int32_t>();
            }
        }

        // Face set indices.
        {
            auto guard = r.TempOffset(face_set_offset);
            state.face_set_indices.resize(face_set_count);
            for (std::uint32_t i = 0; i < face_set_count; ++i)
            {
                state.face_set_indices[i] = r.Read<std::uint32_t>();
            }
        }

        // Vertex array indices.
        {
            auto guard = r.TempOffset(vertex_array_offset);
            state.vertex_array_indices.resize(vertex_array_count);
            for (std::uint32_t i = 0; i < vertex_array_count; ++i)
            {
                state.vertex_array_indices[i] = r.Read<std::uint32_t>();
            }
        }

        // Bounding box.
        if (bounding_box_offset != 0)
        {
            m.uses_bounding_boxes = true;
            auto guard = r.TempOffset(bounding_box_offset);
            m.bounding_box_min.x = r.Read<float>();
            m.bounding_box_min.y = r.Read<float>();
            m.bounding_box_min.z = r.Read<float>();
            m.bounding_box_max.x = r.Read<float>();
            m.bounding_box_max.y = r.Read<float>();
            m.bounding_box_max.z = r.Read<float>();
            if (bounding_box_has_unknown)
            {
                Vector3 unk;
                unk.x = r.Read<float>();
                unk.y = r.Read<float>();
                unk.z = r.Read<float>();
                m.sekiro_bbox_unk = unk;
            }
        }
        else
        {
            m.uses_bounding_boxes = false;
        }

        return state;
    }

    std::uint32_t VertexArrayLayout::GetCompressedVertexSize() const
    {
        std::uint32_t size = 0;
        for (const auto& dt : types)
        {
            if (dt.usage == VertexUsage::Ignore) continue;
            size += FormatEnumSize(dt.format);
        }
        return size;
    }

    std::size_t VertexArrayLayout::GetHash() const
    {
        std::size_t h = 0;
        for (const auto& dt : types)
        {
            h ^= std::hash<int>{}(static_cast<int>(dt.usage)) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<int>{}(static_cast<int>(dt.format)) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<std::uint32_t>{}(dt.instance_index) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<std::uint32_t>{}(dt.unk_x00) + 0x9e3779b9 + (h << 6) + (h >> 2);
        }
        return h;
    }

    VertexArrayLayout VertexArrayLayout::ReadFLVER0(BufferReader& r)
    {
        VertexArrayLayout layout;
        const auto types_count = r.Read<std::int16_t>();
        const auto struct_size = r.Read<std::int16_t>();
        r.AssertPad(12); // padding

        std::uint32_t tight_offset = 0;
        for (std::int16_t i = 0; i < types_count; ++i)
        {
            VertexDataType dt;
            dt.unk_x00 = r.Read<std::uint32_t>();
            dt.data_offset = r.Read<std::uint32_t>();
            dt.format = static_cast<VertexDataFormatEnum>(r.Read<std::uint32_t>());
            const auto type_int = r.Read<std::uint32_t>();
            dt.usage = ToVertexUsage(type_int);
            dt.instance_index = r.Read<std::uint32_t>();
            layout.types.push_back(dt);
            tight_offset += FormatEnumSize(dt.format);
        }
        layout.compressed_vertex_size = tight_offset;

        // Compute decompressed size.
        std::uint32_t decomp_size = 0;
        for (const auto& dt : layout.types)
        {
            if (dt.usage == VertexUsage::Ignore) continue;
            if (const auto info = FindVertexFormatInfo(dt.usage, dt.format))
                decomp_size += static_cast<std::uint32_t>(info->GetTotalDecompressedSize());
        }
        layout.decompressed_vertex_size = decomp_size;

        (void)struct_size; // validated via tight_offset in Python, but we skip that here
        return layout;
    }

    VertexArrayLayout VertexArrayLayout::ReadFLVER2(BufferReader& r)
    {
        VertexArrayLayout layout;
        const auto types_count = r.Read<std::uint32_t>();
        r.AssertPad(8);
        const auto types_offset = r.Read<std::uint32_t>();

        auto guard = r.TempOffset(types_offset);
        std::uint32_t tight_offset = 0;
        for (std::uint32_t i = 0; i < types_count; ++i)
        {
            VertexDataType dt;
            dt.unk_x00 = r.Read<std::uint32_t>();
            dt.data_offset = r.Read<std::uint32_t>();
            dt.format = static_cast<VertexDataFormatEnum>(r.Read<std::uint32_t>());
            const auto type_int = r.Read<std::uint32_t>();
            dt.usage = ToVertexUsage(type_int);
            dt.instance_index = r.Read<std::uint32_t>();

            layout.types.push_back(dt);
            tight_offset += FormatEnumSize(dt.format);
        }
        layout.compressed_vertex_size = tight_offset;

        // Compute decompressed size.
        std::uint32_t decomp_size = 0;
        for (const auto& dt : layout.types)
        {
            if (dt.usage == VertexUsage::Ignore) continue;
            if (const auto info = FindVertexFormatInfo(dt.usage, dt.format))
            {
                decomp_size += static_cast<std::uint32_t>(info->GetTotalDecompressedSize());
            }
            else
            {
                // Unknown format — skip this field in decompressed output.
            }
        }
        layout.decompressed_vertex_size = decomp_size;

        return layout;
    }

    std::vector<std::byte> VertexArray::Compress(const float uv_factor) const
    {
        const auto compressed_stride = layout.GetCompressedVertexSize();
        const auto decompressed_stride = layout.decompressed_vertex_size;

        if (vertex_count == 0 || decompressed_stride == 0)
            return {};

        std::vector compressed(static_cast<std::size_t>(vertex_count) * compressed_stride, std::byte{0});

        std::size_t comp_offset = 0;
        std::size_t decomp_offset = 0;

        for (const auto& dt : layout.types)
        {
            const auto field_compressed_size = FormatEnumSize(dt.format);

            if (dt.usage == VertexUsage::Ignore)
            {
                continue;
            }

            const auto info = FindVertexFormatInfo(dt.usage, dt.format);
            if (!info)
            {
                comp_offset += field_compressed_size;
                continue;
            }

            std::size_t sub_comp_offset = comp_offset;
            std::size_t sub_decomp_offset = decomp_offset;
            for (std::uint8_t f = 0; f < info->field_count; ++f)
            {
                const auto& fspec = info->fields[f];
                const auto field_decomp_size = fspec.GetDecompressedSize();

                // Gather decompressed data from interleaved buffer into tightly-packed temp.
                std::vector<std::byte> tmp(vertex_count * field_decomp_size);
                for (std::uint32_t v = 0; v < vertex_count; ++v)
                {
                    std::memcpy(
                        tmp.data() + v * field_decomp_size,
                        decompressed_data.data() + v * decompressed_stride + sub_decomp_offset,
                        field_decomp_size);
                }

                // Compress into the output buffer.
                CompressField(
                    fspec,
                    tmp.data(),
                    compressed.data(),
                    vertex_count,
                    compressed_stride,
                    sub_comp_offset,
                    uv_factor);

                sub_comp_offset += fspec.GetCompressedSize();
                sub_decomp_offset += fspec.GetDecompressedSize();
            }

            comp_offset += field_compressed_size;
            decomp_offset += info->GetTotalDecompressedSize();
        }

        return compressed;
    }

    VertexArray VertexArray::FromCompressedData(
        const std::byte* raw_data,
        const std::uint32_t vertex_count,
        const VertexArrayLayout& layout,
        const float uv_factor)
    {
        VertexArray va;
        va.layout = layout;
        va.vertex_count = vertex_count;

        if (vertex_count == 0 || layout.decompressed_vertex_size == 0)
        {
            return va;
        }

        const auto compressed_stride = layout.compressed_vertex_size;
        const auto decompressed_stride = layout.decompressed_vertex_size;
        va.decompressed_data.resize(
            static_cast<std::size_t>(vertex_count) * decompressed_stride, std::byte{0});

        // Walk each layout type and decompress into the interleaved output buffer.
        std::size_t comp_offset = 0;
        std::size_t decomp_offset = 0;

        for (const auto& dt : layout.types)
        {
            const auto field_compressed_size = FormatEnumSize(dt.format);

            if (dt.usage == VertexUsage::Ignore)
            {
                comp_offset += field_compressed_size;
                continue;
            }

            const auto info = FindVertexFormatInfo(dt.usage, dt.format);
            if (!info)
            {
                // Skip unknown format.
                comp_offset += field_compressed_size;
                continue;
            }

            // Decompress each sub-field within this format entry.
            std::size_t sub_comp_offset = comp_offset;
            std::size_t sub_decomp_offset = decomp_offset;
            for (std::uint8_t f = 0; f < info->field_count; ++f)
            {
                const auto& fspec = info->fields[f];

                // We decompress into a temporary tightly-packed buffer, then
                // scatter-write into the interleaved output.
                const auto field_decomp_size = fspec.GetDecompressedSize();
                std::vector<std::byte> tmp(vertex_count * field_decomp_size);

                DecompressField(
                    fspec,
                    raw_data,
                    tmp.data(),
                    vertex_count,
                    compressed_stride,
                    sub_comp_offset,
                    uv_factor);

                // Scatter into interleaved output.
                for (std::uint32_t v = 0; v < vertex_count; ++v)
                {
                    std::memcpy(
                        va.decompressed_data.data() + v * decompressed_stride + sub_decomp_offset,
                        tmp.data() + v * field_decomp_size,
                        field_decomp_size);
                }

                sub_comp_offset += fspec.GetCompressedSize();
                sub_decomp_offset += field_decomp_size;
            }

            comp_offset += field_compressed_size;
            decomp_offset += info->GetTotalDecompressedSize();
        }

        return va;
    }

    FaceSet FaceSet::ReadFLVER2(
        BufferReader& r,
        const std::uint8_t header_vertex_index_bit_size,
        const std::uint32_t vertex_data_offset)
    {
        FaceSet fs;
        r.AssertPad(3);
        fs.flags = static_cast<std::int32_t>(r.Read<std::uint8_t>());
        fs.is_triangle_strip = r.Read<std::uint8_t>() != 0;
        fs.use_backface_culling = r.Read<std::uint8_t>() != 0;
        fs.unk_x06 = r.Read<std::int16_t>();
        const auto vertex_indices_count = r.Read<std::uint32_t>();
        const auto vertex_indices_offset = r.Read<std::uint32_t>();
        (void)r.Read<std::uint32_t>(); // vertex_indices_length (computable)
        r.AssertPad(4);
        auto vertex_index_bit_size = r.Read<std::uint32_t>();
        r.AssertPad(4);

        // vertex_index_bit_size == 0 means use header's global value.
        if (vertex_index_bit_size == 0)
        {
            vertex_index_bit_size = header_vertex_index_bit_size;
        }

        // Read indices.
        fs.vertex_indices.resize(vertex_indices_count);
        auto guard = r.TempOffset(vertex_data_offset + vertex_indices_offset);
        if (vertex_index_bit_size == 16)
        {
            for (std::uint32_t i = 0; i < vertex_indices_count; ++i)
            {
                fs.vertex_indices[i] = r.Read<std::uint16_t>();
            }
        }
        else if (vertex_index_bit_size == 32)
        {
            for (std::uint32_t i = 0; i < vertex_indices_count; ++i)
            {
                fs.vertex_indices[i] = r.Read<std::uint32_t>();
            }
        }
        else if (vertex_index_bit_size == 8)
        {
            for (std::uint32_t i = 0; i < vertex_indices_count; ++i)
            {
                fs.vertex_indices[i] = r.Read<std::uint8_t>();
            }
        }
        else
        {
            throw FLVERError("Unsupported vertex_index_bit_size: " + std::to_string(vertex_index_bit_size));
        }

        return fs;
    }

    // Read a FLVER0 mesh (100-byte struct).
    Mesh Mesh::ReadFLVER0(
        BufferReader& r,
        const std::uint8_t vertex_index_bit_size,
        const std::uint32_t vertex_data_offset,
        const std::vector<FLVER0MaterialRead>& materials,
        const FLVERVersion version,
        const float uv_factor,
        const std::int32_t mesh_index)
    {
        Mesh m;
        m.index = mesh_index;
        m.is_dynamic = r.Read<std::uint8_t>() != 0;
        const auto material_index = r.Read<std::uint8_t>();
        const bool use_backface_culling = r.Read<std::uint8_t>() != 0;
        const bool is_triangle_strip_raw = r.Read<std::uint8_t>() != 0;
        const auto vertex_index_count = r.Read<std::uint32_t>();
        auto vertex_count = r.Read<std::uint32_t>();  // not required
        m.default_bone_index = r.Read<std::int16_t>();

        // 28 bone indices (shorts).
        m.bone_indices.resize(28);
        for (int i = 0; i < 28; ++i)
            m.bone_indices[i] = r.Read<std::int16_t>();

        m.f0_unk_x46 = r.Read<std::uint16_t>();
        const auto vertex_indices_size = r.Read<std::uint32_t>(); (void)vertex_indices_size;
        const auto vertex_indices_offset = r.Read<std::uint32_t>();
        const auto vertex_array_data_size = r.Read<std::uint32_t>();
        const auto vertex_array_data_offset = r.Read<std::uint32_t>();
        const auto va_headers_offset_1 = r.Read<std::uint32_t>();
        const auto va_headers_offset_2 = r.Read<std::uint32_t>();
        r.AssertPad(4);

        // Copy material (without layouts — those go to vertex arrays).
        m.material = materials[material_index].mat;
        const auto& mat_layouts = materials[material_index].layouts;

        // Read vertex indices.
        {
            auto guard = r.TempOffset(vertex_data_offset + vertex_indices_offset);
            FaceSet fs;
            fs.flags = 0;
            fs.use_backface_culling = use_backface_culling;
            fs.unk_x06 = 0;

            // Determine triangle strip flag.
            bool is_triangle_strip = is_triangle_strip_raw;
            if (version < FLVERVersion::DemonsSouls)
                is_triangle_strip = true; // always strip in old versions

            fs.is_triangle_strip = is_triangle_strip;
            fs.vertex_indices.resize(vertex_index_count);
            if (vertex_index_bit_size == 16)
            {
                for (std::uint32_t i = 0; i < vertex_index_count; ++i)
                    fs.vertex_indices[i] = r.Read<std::uint16_t>();
            }
            else
            {
                for (std::uint32_t i = 0; i < vertex_index_count; ++i)
                    fs.vertex_indices[i] = r.Read<std::uint32_t>();
            }
            m.face_sets.push_back(std::move(fs));
        }

        // Read vertex array.
        std::uint32_t layout_index = 0;
        std::uint32_t array_length = 0;
        std::uint32_t array_offset_val = 0;

        if (va_headers_offset_1 == 0)
        {
            // Hack for old FLVER version: use inline data.
            layout_index = 0;
            array_length = vertex_array_data_size;
            array_offset_val = vertex_array_data_offset;
        }
        else
        {
            auto guard = r.TempOffset(va_headers_offset_1);
            const auto array_count = r.Read<std::int32_t>();
            const auto arrays_offset = r.Read<std::int32_t>();
            r.AssertPad(8);

            if (array_count == 0)
                throw FLVERReadError("No vertex arrays found at first array header offset in FLVER0 mesh.");

            auto guard2 = r.TempOffset(arrays_offset);
            // Read first header (16 bytes: layout_index, array_length, array_offset, pad4).
            layout_index = r.Read<std::uint32_t>();
            array_length = r.Read<std::uint32_t>();
            array_offset_val = r.Read<std::uint32_t>();
            r.AssertPad(4);
        }

        if (va_headers_offset_2 != 0)
        {
            auto guard = r.TempOffset(va_headers_offset_2);
            const auto array_count_2 = r.Read<std::int32_t>();
            if (array_count_2 > 0)
                throw FLVERReadError("Unexpected vertex arrays at second header offset in FLVER0 mesh.");
        }

        // Decompress vertex array using the layout from this mesh's material.
        if (layout_index >= mat_layouts.size())
            throw FLVERReadError("FLVER0 mesh layout_index out of range for material layouts.");

        const auto& layout = mat_layouts[layout_index];
        if (array_length > 0 && layout.compressed_vertex_size > 0)
        {
            const auto vc = array_length / layout.compressed_vertex_size;
            auto guard = r.TempOffset(vertex_data_offset + array_offset_val);
            const std::byte* raw = r.RawAt(vertex_data_offset + array_offset_val);
            m.vertex_arrays.push_back(VertexArray::FromCompressedData(raw, vc, layout, uv_factor));
        }
        else
        {
            VertexArray va;
            va.layout = layout;
            va.vertex_count = 0;
            m.vertex_arrays.push_back(std::move(va));
        }

        return m;
    }

}