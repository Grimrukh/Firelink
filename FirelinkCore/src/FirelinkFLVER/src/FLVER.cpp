// FLVER read/write + member function implementations.

#include "../include/FirelinkFLVER/FLVER.h"

#include <FirelinkFLVER/LayoutRepair.h>

#include <FirelinkCore/BinaryReadWrite.h>
#include <FirelinkCore/DCX.h>
#include <FirelinkCore/Logging.h>

#include <algorithm>
#include <functional>
#include <unordered_map>

namespace Firelink
{
    using BufferReader = BinaryReadWrite::BufferReader;
    using BufferWriter = BinaryReadWrite::BufferWriter;
    using Endian = BinaryReadWrite::Endian;

    // ---------------------------------------------------------------------------
    // Internal reader/writer helpers
    // ---------------------------------------------------------------------------

    namespace
    {
        // Read a null-terminated string at `offset` without moving the reader cursor.
        std::string read_string_at(const BufferReader& r, const std::size_t offset, const bool unicode_encoding)
        {
            if (unicode_encoding)
            {
                const auto bytes = r.ReadUTF16LEStringAt(offset);
                return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
            }
            else
            {
                const auto bytes = r.ReadCStringAt(offset);
                return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
            }
        }

        // Write a null-terminated string. If unicode, writes UTF-16 LE with null terminator.
        void write_string(BufferWriter& w, const std::string& s, bool unicode_encoding)
        {
            if (unicode_encoding)
            {
                // String is stored as raw UTF-16 LE bytes already (round-trip).
                w.WriteRaw(s.data(), s.size());
                // Null terminator: two zero bytes.
                w.WritePad(2);
            }
            else
            {
                w.WriteRaw(s.data(), s.size());
                w.WritePad(1); // single null byte terminator
            }
        }

        // Simple hash combining for material deduplication.
        std::size_t hash_material(const Material& mat)
        {
            std::size_t h = std::hash<std::string>{}(mat.name);
            h ^= std::hash<std::string>{}(mat.mat_def_path) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<int>{}(mat.flags) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<int>{}(mat.f2_unk_x18) + 0x9e3779b9 + (h << 6) + (h >> 2);
            for (const auto& tex : mat.textures)
            {
                h ^= std::hash<std::string>{}(tex.path) + 0x9e3779b9 + (h << 6) + (h >> 2);
                if (tex.texture_type.has_value())
                    h ^= std::hash<std::string>{}(tex.texture_type.value()) + 0x9e3779b9 + (h << 6) + (h >> 2);
            }
            return h;
        }

        // Hash a GXItem list for deduplication.
        std::size_t hash_gx_list(const std::vector<GXItem>& items)
        {
            std::size_t h = 0;
            for (const auto& item : items)
            {
                std::size_t ih = 0;
                for (int i = 0; i < 4; ++i)
                    ih ^= std::hash<char>{}(item.category[i]) + 0x9e3779b9 + (ih << 6) + (ih >> 2);
                ih ^= std::hash<int>{}(item.index) + 0x9e3779b9 + (ih << 6) + (ih >> 2);
                for (auto b : item.data)
                    ih ^= std::hash<int>{}(static_cast<int>(b)) + 0x9e3779b9 + (ih << 6) + (ih >> 2);
                h ^= ih + 0x9e3779b9 + (h << 6) + (h >> 2);
            }
            return h;
        }

        // Hash a VertexArrayLayout for deduplication.
        std::size_t hash_layout(const VertexArrayLayout& layout)
        {
            std::size_t h = 0;
            for (const auto& dt : layout.types)
            {
                h ^= std::hash<int>{}(static_cast<int>(dt.usage)) + 0x9e3779b9 + (h << 6) + (h >> 2);
                h ^= std::hash<int>{}(static_cast<int>(dt.format)) + 0x9e3779b9 + (h << 6) + (h >> 2);
                h ^= std::hash<int>{}(dt.instance_index) + 0x9e3779b9 + (h << 6) + (h >> 2);
                h ^= std::hash<int>{}(dt.unk_x00) + 0x9e3779b9 + (h << 6) + (h >> 2);
            }
            return h;
        }

        // Compute "write" compressed vertex size — excludes Ignored types.
        std::uint32_t WriteCompressedVertexSize(const VertexArrayLayout& layout)
        {
            std::uint32_t size = 0;
            for (const auto& dt : layout.types)
            {
                if (dt.usage == VertexUsage::Ignore) continue;
                size += FormatEnumSize(dt.format);
            }
            return size;
        }

        // Compress a vertex array (inverse of DecompressVertexArray).
        std::vector<std::byte> CompressVertexArray(
            const VertexArray& va,
            const float uv_factor)
        {
            const auto& layout = va.layout;
            const auto vertex_count = va.vertex_count;
            const auto compressed_stride = WriteCompressedVertexSize(layout);
            const auto decompressed_stride = layout.decompressed_vertex_size;

            if (vertex_count == 0 || decompressed_stride == 0)
                return {};

            std::vector<std::byte> compressed(
                static_cast<std::size_t>(vertex_count) * compressed_stride, std::byte{0});

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
                            va.decompressed_data.data() + v * decompressed_stride + sub_decomp_offset,
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


        // Write a VertexDataType struct (20 bytes).
        void WriteVertexDataType(BufferWriter& w, const VertexDataType& dt)
        {
            w.Write<std::uint32_t>(dt.unk_x00);
            w.Write<std::uint32_t>(dt.data_offset);
            w.Write<std::uint32_t>(static_cast<std::uint32_t>(dt.format));
            w.Write<std::uint32_t>(FromVertexUsage(dt.usage));
            w.Write<std::uint32_t>(dt.instance_index);
        }

        // ---------------------------------------------------------------------------
        // FLVER2 individual struct readers (existing)
        // ---------------------------------------------------------------------------

        Dummy ReadDummy(BufferReader& r, FLVERVersion /*version*/)
        {
            Dummy d;
            d.translate.x = r.Read<float>();
            d.translate.y = r.Read<float>();
            d.translate.z = r.Read<float>();
            // Color: 4 bytes (BGRA on disk for FLVER2; we store as-is)
            d.color.r = r.Read<std::uint8_t>();
            d.color.g = r.Read<std::uint8_t>();
            d.color.b = r.Read<std::uint8_t>();
            d.color.a = r.Read<std::uint8_t>();
            d.forward.x = r.Read<float>();
            d.forward.y = r.Read<float>();
            d.forward.z = r.Read<float>();
            d.reference_id = r.Read<std::int16_t>();
            d.parent_bone_index = r.Read<std::int16_t>();
            d.upward.x = r.Read<float>();
            d.upward.y = r.Read<float>();
            d.upward.z = r.Read<float>();
            d.attach_bone_index = r.Read<std::int16_t>();
            d.follows_attach_bone = r.Read<std::uint8_t>() != 0;
            d.use_upward_vector = r.Read<std::uint8_t>() != 0;
            d.unk_x30 = r.Read<std::int32_t>();
            d.unk_x34 = r.Read<std::int32_t>();
            r.AssertPad(8);
            return d;
        }

        Bone read_bone(BufferReader& r, const bool unicode_encoding)
        {
            Bone b;
            // Bone binary: translate(12), name_offset(4), rotate(12), parent(2), child(2),
            //              scale(12), next_sibling(2), prev_sibling(2), bb_min(12),
            //              usage_flags(4), bb_max(12), pad(52) = 128 bytes
            b.translate.x = r.Read<float>();
            b.translate.y = r.Read<float>();
            b.translate.z = r.Read<float>();
            const auto name_offset = r.Read<std::uint32_t>();
            b.rotate.x = r.Read<float>();
            b.rotate.y = r.Read<float>();
            b.rotate.z = r.Read<float>();
            b.parent_bone_index = r.Read<std::int16_t>();
            b.child_bone_index = r.Read<std::int16_t>();
            b.scale.x = r.Read<float>();
            b.scale.y = r.Read<float>();
            b.scale.z = r.Read<float>();
            b.next_sibling_bone_index = r.Read<std::int16_t>();
            b.previous_sibling_bone_index = r.Read<std::int16_t>();
            b.bounding_box.min.x = r.Read<float>();
            b.bounding_box.min.y = r.Read<float>();
            b.bounding_box.min.z = r.Read<float>();
            b.usage_flags = r.Read<std::int32_t>();
            b.bounding_box.max.x = r.Read<float>();
            b.bounding_box.max.y = r.Read<float>();
            b.bounding_box.max.z = r.Read<float>();
            r.AssertPad(52);

            b.name = read_string_at(r, name_offset, unicode_encoding);
            return b;
        }

        Material ReadMaterialFLVER2(
            BufferReader& r,
            const bool unicode_encoding,
            const FLVERVersion version,
            std::unordered_map<std::uint32_t, std::vector<GXItem>>& gx_item_lists_cache,
            std::uint32_t& out_texture_count,
            std::uint32_t& out_first_texture_index)
        {
            Material mat;
            const auto name_offset = r.Read<std::uint32_t>();
            const auto mat_def_offset = r.Read<std::uint32_t>();
            out_texture_count = r.Read<std::uint32_t>();
            out_first_texture_index = r.Read<std::uint32_t>();
            mat.flags = r.Read<std::int32_t>();
            const auto gx_offset = r.Read<std::uint32_t>();
            mat.f2_unk_x18 = r.Read<std::int32_t>();
            r.AssertPad(4);

            mat.name = read_string_at(r, name_offset, unicode_encoding);
            mat.mat_def_path = read_string_at(r, mat_def_offset, unicode_encoding);

            // GX items.
            if (gx_offset > 0)
            {
                if (const auto it = gx_item_lists_cache.find(gx_offset); it != gx_item_lists_cache.end())
                {
                    mat.gx_items = it->second; // deep copy
                }
                else
                {
                    // Read GX item list at offset.
                    auto guard = r.TempOffset(gx_offset);
                    if (version <= FLVERVersion::DarkSouls2)
                    {
                        // Single GX item, no terminator.
                        GXItem item;
                        r.ReadRaw(item.category.data(), 4);
                        item.index = r.Read<std::int32_t>();
                        if (const auto size = r.Read<std::int32_t>(); size < 12)
                        {
                            Error("FLVER material GX item size " + std::to_string(size) + " is too small (< 12). Setting GXItem data to null.");
                            item.data.resize(0);
                        }
                        else
                        {
                            const auto data_size = static_cast<std::size_t>(size - 12);
                            item.data.resize(size - 12);
                            if (data_size > 0)
                            {
                                r.ReadRaw(item.data.data(), item.data.size());
                            }
                        }
                        mat.gx_items.push_back(std::move(item));
                    }
                    else
                    {
                        // Read until terminator.
                        while (true)
                        {
                            GXItem item;
                            r.ReadRaw(item.category.data(), 4);
                            item.index = r.Read<std::int32_t>();
                            if (const auto size = r.Read<std::int32_t>(); size < 12)
                            {
                                Error("FLVER material GX item size " + std::to_string(size) + " is too small (< 12). Setting GXItem data to null.");
                                item.data.resize(0);
                            }
                            else
                            {
                                const auto data_size = static_cast<std::size_t>(size - 12);
                                item.data.resize(data_size);
                                if (data_size > 0)
                                {
                                    r.ReadRaw(item.data.data(), data_size);
                                }
                            }
                            const bool is_term = item.IsTerminator();
                            mat.gx_items.push_back(std::move(item));
                            if (is_term) break;
                        }
                    }
                    gx_item_lists_cache[gx_offset] = mat.gx_items; // cache for sharing
                }
            }

            return mat;
        }

        Texture ReadTextureFLVER2(BufferReader& r, const bool unicode_encoding)
        {
            Texture tex;
            const auto path_offset = r.Read<std::uint32_t>();
            const auto type_offset = r.Read<std::uint32_t>();
            tex.scale.x = r.Read<float>();
            tex.scale.y = r.Read<float>();
            tex.f2_unk_x10 = r.Read<std::uint8_t>();
            tex.f2_unk_x11 = r.Read<std::uint8_t>() != 0;
            r.AssertPad(2);
            tex.f2_unk_x14 = r.Read<float>();
            tex.f2_unk_x18 = r.Read<float>();
            tex.f2_unk_x1c = r.Read<float>();

            tex.path = read_string_at(r, path_offset, unicode_encoding);
            tex.texture_type = read_string_at(r, type_offset, unicode_encoding);
            return tex;
        }

        // Read mesh header for FLVER2 — returns a partially-constructed Mesh
        // with indices that need post-read dereferencing.
        struct MeshReadState
        {
            Mesh mesh;
            std::uint32_t material_index;
            std::vector<std::uint32_t> face_set_indices;
            std::vector<std::uint32_t> vertex_array_indices;
        };

        MeshReadState ReadMeshHeaderFLVER2(
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

        FaceSet ReadFaceSetFLVER2(
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

        // Read a FLVER2 vertex array header (does NOT read the vertex data itself).
        struct VertexArrayHeader
        {
            std::uint32_t array_index;
            std::uint32_t layout_index;
            std::uint32_t vertex_size;
            std::uint32_t vertex_count;
            std::uint32_t array_length;
            std::uint32_t array_offset;
        };

        VertexArrayHeader ReadVertexArrayHeader(BufferReader& r)
        {
            VertexArrayHeader h;
            h.array_index = r.Read<std::uint32_t>();
            h.layout_index = r.Read<std::uint32_t>();
            h.vertex_size = r.Read<std::uint32_t>();
            h.vertex_count = r.Read<std::uint32_t>();
            r.AssertPad(8);
            h.array_length = r.Read<std::uint32_t>();
            h.array_offset = r.Read<std::uint32_t>();
            return h;
        }

        // Read a FLVER2 vertex array layout.
        VertexArrayLayout ReadLayoutFLVER2(BufferReader& r)
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

        // Decompress a vertex array given its raw on-disk data and layout.
        VertexArray DecompressVertexArray(
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

        // ---------------------------------------------------------------------------
        // FLVER0-specific readers
        // ---------------------------------------------------------------------------

        // Read a FLVER0 layout (types follow immediately after header).
        VertexArrayLayout ReadLayoutFLVER0(BufferReader& r)
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

            (void)struct_size; // validated via tight_offset in Python but we skip that here
            return layout;
        }

        // Read a FLVER0 texture.
        Texture ReadTextureFLVER0(BufferReader& r, const bool unicode_encoding)
        {
            Texture tex;
            const auto path_offset = r.Read<std::uint32_t>();
            const auto type_offset = r.Read<std::uint32_t>();
            r.AssertPad(8);

            tex.path = read_string_at(r, path_offset, unicode_encoding);
            if (type_offset > 0)
                tex.texture_type = read_string_at(r, type_offset, unicode_encoding);
            else
                tex.texture_type = std::nullopt;
            return tex;
        }

        // Read a FLVER0 material header and its data (textures, layouts).
        struct FLVER0MaterialRead
        {
            Material mat;
            std::vector<VertexArrayLayout> layouts;
        };

        FLVER0MaterialRead ReadMaterialFLVER0(BufferReader& r, const bool unicode_encoding)
        {
            FLVER0MaterialRead result;

            const auto name_offset = r.Read<std::uint32_t>();
            const auto mat_def_path_offset = r.Read<std::uint32_t>();
            const auto textures_offset = r.Read<std::uint32_t>();
            const auto layouts_offset = r.Read<std::uint32_t>();
            const auto size = r.Read<std::uint32_t>(); (void)size;
            const auto layout_header_offset = r.Read<std::uint32_t>();
            r.AssertPad(8);

            result.mat.name = read_string_at(r, name_offset, unicode_encoding);
            result.mat.mat_def_path = read_string_at(r, mat_def_path_offset, unicode_encoding);

            // Read textures.
            {
                auto guard = r.TempOffset(textures_offset);
                auto texture_count = r.Read<std::uint8_t>();
                r.AssertPad(3);
                r.AssertPad(12);
                for (std::uint8_t i = 0; i < texture_count; ++i)
                    result.mat.textures.push_back(ReadTextureFLVER0(r, unicode_encoding));
            }

            // Read layouts.
            if (layout_header_offset != 0)
            {
                auto guard = r.TempOffset(layout_header_offset);
                auto layout_count = r.Read<std::int32_t>();
                auto layout_data_offset = r.Read<std::int32_t>(); (void)layout_data_offset;
                r.AssertPad(8);

                // Read layout offset table then layouts.
                std::vector<std::uint32_t> layout_offsets;
                for (std::int32_t i = 0; i < layout_count; ++i)
                    layout_offsets.push_back(r.Read<std::uint32_t>());

                for (auto lo : layout_offsets)
                {
                    auto guard2 = r.TempOffset(lo);
                    result.layouts.push_back(ReadLayoutFLVER0(r));
                }
            }
            else
            {
                auto guard = r.TempOffset(layouts_offset);
                result.layouts.push_back(ReadLayoutFLVER0(r));
            }

            return result;
        }

        // Read a FLVER0 mesh (100-byte struct).
        Mesh ReadMeshFLVER0(
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
            auto material_index = r.Read<std::uint8_t>();
            bool use_backface_culling = r.Read<std::uint8_t>() != 0;
            bool is_triangle_strip_raw = r.Read<std::uint8_t>() != 0;
            auto vertex_index_count = r.Read<std::uint32_t>();
            auto vertex_count = r.Read<std::uint32_t>();
            m.default_bone_index = r.Read<std::int16_t>();

            // 28 bone indices (shorts).
            m.bone_indices.resize(28);
            for (int i = 0; i < 28; ++i)
                m.bone_indices[i] = r.Read<std::int16_t>();

            m.f0_unk_x46 = r.Read<std::uint16_t>();
            auto vertex_indices_size = r.Read<std::uint32_t>(); (void)vertex_indices_size;
            auto vertex_indices_offset = r.Read<std::uint32_t>();
            auto vertex_array_data_size = r.Read<std::uint32_t>();
            auto vertex_array_data_offset = r.Read<std::uint32_t>();
            auto va_headers_offset_1 = r.Read<std::uint32_t>();
            auto va_headers_offset_2 = r.Read<std::uint32_t>();
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
                auto array_count = r.Read<std::int32_t>();
                auto arrays_offset = r.Read<std::int32_t>();
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
                auto array_count_2 = r.Read<std::int32_t>();
                if (array_count_2 > 0)
                    throw FLVERReadError("Unexpected vertex arrays at second header offset in FLVER0 mesh.");
            }

            // Decompress vertex array using the layout from this mesh's material.
            if (layout_index >= mat_layouts.size())
                throw FLVERReadError("FLVER0 mesh layout_index out of range for material layouts.");

            const auto& layout = mat_layouts[layout_index];
            if (array_length > 0 && layout.compressed_vertex_size > 0)
            {
                auto vc = array_length / layout.compressed_vertex_size;
                auto guard = r.TempOffset(vertex_data_offset + array_offset_val);
                const std::byte* raw = r.RawAt(vertex_data_offset + array_offset_val);
                m.vertex_arrays.push_back(DecompressVertexArray(raw, vc, layout, uv_factor));
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
    } // anonymous namespace

    // ---------------------------------------------------------------------------
    // FLVER::FromBytes
    // ---------------------------------------------------------------------------

    FLVER FLVER::FromBytes(const std::byte* data, std::size_t size)
    {
        auto dcx_type = DCXType::Null;
        const std::byte* decompressed_data = data;
        std::vector<std::byte> decompressed_storage;
        if (IsDCX(data, size))
        {
            auto result = DecompressDCX(data, size);
            dcx_type = result.type;
            decompressed_storage = std::move(result.data);
            decompressed_data = decompressed_storage.data();
            size = decompressed_storage.size();
        }

        if (size < 12)
            throw FLVERError("Buffer too small to contain a FLVER header");

        BufferReader r(decompressed_data, size, Endian::Little);

        constexpr char expected_magic[] = "FLVER";
        r.AssertBytes(expected_magic, 5, "FLVER magic");
        r.Skip(1);

        char endian_tag[2];
        r.ReadRaw(endian_tag, 2);
        Endian file_endian;
        if (endian_tag[0] == 'L' && endian_tag[1] == '\0')
            file_endian = Endian::Little;
        else if (endian_tag[0] == 'B' && endian_tag[1] == '\0')
            file_endian = Endian::Big;
        else
            throw FLVERReadError("Invalid FLVER endian tag");

        r.SetEndian(file_endian);
        auto version_int = r.Read<std::uint32_t>();
        FLVERVersion version;
        try { version = static_cast<FLVERVersion>(version_int); }
        catch (...) { throw FLVERError("Unrecognized FLVER version: " + std::to_string(version_int)); }

        if (version_int <= 0xFFFF)
        {
            // --- FLVER0 reader ---
            r.Seek(0);
            r.SetEndian(file_endian);

            FLVER flver;
            flver.version = version;
            flver.big_endian = file_endian == Endian::Big;
            flver.dcx_type = dcx_type;

            // 128-byte FLVER0 header.
            r.Skip(6); // "FLVER\0"
            r.Skip(2); // endian tag
            r.Skip(4); // version

            auto vertex_data_offset = r.Read<std::uint32_t>();
            auto vertex_data_size = r.Read<std::uint32_t>(); (void)vertex_data_size;
            auto dummy_count = r.Read<std::uint32_t>();
            auto material_count = r.Read<std::uint32_t>();
            auto bone_count = r.Read<std::uint32_t>();
            auto mesh_count = r.Read<std::uint32_t>();
            auto vertex_array_count = r.Read<std::uint32_t>(); (void)vertex_array_count;

            flver.bounding_box_min.x = r.Read<float>();
            flver.bounding_box_min.y = r.Read<float>();
            flver.bounding_box_min.z = r.Read<float>();
            flver.bounding_box_max.x = r.Read<float>();
            flver.bounding_box_max.y = r.Read<float>();
            flver.bounding_box_max.z = r.Read<float>();

            flver.true_face_count = r.Read<std::int32_t>();
            flver.total_face_count = r.Read<std::int32_t>();

            auto face_set_vertex_index_bit_size = r.Read<std::uint8_t>();
            flver.unicode = r.Read<std::uint8_t>() != 0;
            flver.f0_unk_x4a = r.Read<std::uint8_t>();
            flver.f0_unk_x4b = r.Read<std::uint8_t>();
            flver.f0_unk_x4c = r.Read<std::uint32_t>();
            r.AssertPad(12); // no face_set_count, layout_count, texture_count
            flver.f0_unk_x5c = r.Read<std::uint8_t>();
            r.AssertPad(3);
            r.AssertPad(32);
            // Header: 128 bytes total.

            bool unicode_enc = flver.unicode;
            float uv_factor = flver.big_endian ? 1024.f : 2048.f;

            // Dummies.
            for (std::uint32_t i = 0; i < dummy_count; ++i)
                flver.dummies.push_back(ReadDummy(r, version));

            // Materials (with layouts stored inside).
            std::vector<FLVER0MaterialRead> mat_reads;
            for (std::uint32_t i = 0; i < material_count; ++i)
                mat_reads.push_back(ReadMaterialFLVER0(r, unicode_enc));

            // Bones.
            for (std::uint32_t i = 0; i < bone_count; ++i)
                flver.bones.push_back(read_bone(r, unicode_enc));

            // Meshes.
            for (std::uint32_t i = 0; i < mesh_count; ++i)
            {
                flver.meshes.push_back(ReadMeshFLVER0(
                    r, face_set_vertex_index_bit_size, vertex_data_offset,
                    mat_reads, version, uv_factor, static_cast<std::int32_t>(i)));
            }

            return flver;
        }

        // --- FLVER2 reader ---
        r.Seek(0);
        r.SetEndian(file_endian);

        FLVER flver;
        flver.version = version;
        flver.big_endian = file_endian == Endian::Big;
        flver.dcx_type = dcx_type;

        // FLVER2 header: 128 bytes.
        r.Skip(6); // "FLVER\0"
        r.Skip(2); // endian tag (already read)
        r.Skip(4); // version (already read)

        auto vertex_data_offset = r.Read<std::uint32_t>();
        auto vertex_data_size = r.Read<std::uint32_t>();
        (void)vertex_data_size;

        auto dummy_count = r.Read<std::uint32_t>();
        auto material_count = r.Read<std::uint32_t>();
        auto bone_count = r.Read<std::uint32_t>();
        auto mesh_count = r.Read<std::uint32_t>();
        auto vertex_array_count = r.Read<std::uint32_t>();

        flver.bounding_box_min.x = r.Read<float>();
        flver.bounding_box_min.y = r.Read<float>();
        flver.bounding_box_min.z = r.Read<float>();
        flver.bounding_box_max.x = r.Read<float>();
        flver.bounding_box_max.y = r.Read<float>();
        flver.bounding_box_max.z = r.Read<float>();

        flver.true_face_count = r.Read<std::int32_t>();
        flver.total_face_count = r.Read<std::int32_t>();

        auto face_set_vertex_index_bit_size = r.Read<std::uint8_t>();
        flver.unicode = r.Read<std::uint8_t>() != 0;
        flver.f2_unk_x4a = r.Read<std::uint8_t>() != 0;
        r.AssertPad(1);
        flver.f2_unk_x4c = r.Read<std::int32_t>();

        auto face_set_count = r.Read<std::uint32_t>();
        auto layout_count = r.Read<std::uint32_t>();
        auto texture_count = r.Read<std::uint32_t>();

        flver.f2_unk_x5c = static_cast<std::int32_t>(r.Read<std::uint8_t>());
        flver.f2_unk_x5d = static_cast<std::int32_t>(r.Read<std::uint8_t>());
        r.AssertPad(10);
        flver.f2_unk_x68 = r.Read<std::int32_t>();
        r.AssertPad(20);

        // Header should be 128 bytes total.
        // (offset should now be 128 = 0x80)

        bool unicode_enc = flver.unicode;
        float uv_factor = version >= FLVERVersion::DarkSouls2_NT ? 2048.f : 1024.f;
        bool bounding_box_has_unknown = version == FLVERVersion::Sekiro_EldenRing;

        // --- Read dummies ---
        flver.dummies.reserve(dummy_count);
        for (std::uint32_t i = 0; i < dummy_count; ++i)
        {
            flver.dummies.push_back(ReadDummy(r, version));
        }

        // --- Read materials (with deferred texture/GX item assignment) ---
        struct MatReadInfo
        {
            Material mat;
            std::uint32_t texture_count;
            std::uint32_t first_texture_index;
        };
        std::vector<MatReadInfo> mat_infos;
        mat_infos.reserve(material_count);
        std::unordered_map<std::uint32_t, std::vector<GXItem>> gx_cache;
        for (std::uint32_t i = 0; i < material_count; ++i)
        {
            std::uint32_t tc = 0, fti = 0;
            auto mat = ReadMaterialFLVER2(r, unicode_enc, version, gx_cache, tc, fti);
            mat_infos.push_back({std::move(mat), tc, fti});
        }

        // --- Read bones ---
        flver.bones.reserve(bone_count);
        for (std::uint32_t i = 0; i < bone_count; ++i)
        {
            flver.bones.push_back(read_bone(r, unicode_enc));
        }

        // --- Read mesh headers ---
        std::vector<MeshReadState> mesh_states;
        mesh_states.reserve(mesh_count);
        for (std::uint32_t i = 0; i < mesh_count; ++i)
        {
            mesh_states.push_back(ReadMeshHeaderFLVER2(r, bounding_box_has_unknown, static_cast<std::int32_t>(i)));
        }

        // --- Read face sets ---
        std::unordered_map<std::uint32_t, FaceSet> face_sets;
        for (std::uint32_t i = 0; i < face_set_count; ++i)
        {
            face_sets[i] = ReadFaceSetFLVER2(r, face_set_vertex_index_bit_size, vertex_data_offset);
        }

        // --- Read vertex array headers ---
        std::vector<VertexArrayHeader> va_headers;
        va_headers.reserve(vertex_array_count);
        for (std::uint32_t i = 0; i < vertex_array_count; ++i)
        {
            va_headers.push_back(ReadVertexArrayHeader(r));
        }

        // --- Read layouts ---
        std::vector<VertexArrayLayout> layouts;
        layouts.reserve(layout_count);
        for (std::uint32_t i = 0; i < layout_count; ++i)
        {
            layouts.push_back(ReadLayoutFLVER2(r));
        }

        // --- Attempt DS1R layout repair ---
        {
            std::vector<VertexArrayHeaderView> va_views;
            va_views.reserve(va_headers.size());
            for (const auto& h : va_headers)
            {
                va_views.push_back({h.layout_index, h.vertex_size});
            }

            std::vector<Material> mesh_materials;
            std::vector<std::vector<std::uint32_t>> mesh_va_idx;
            mesh_materials.reserve(mesh_states.size());
            mesh_va_idx.reserve(mesh_states.size());
            for (const auto& ms : mesh_states)
            {
                mesh_materials.push_back(mat_infos[ms.material_index].mat);
                mesh_va_idx.push_back(ms.vertex_array_indices);
            }

            if (CheckDS1Layouts(layouts, va_views, mesh_materials, mesh_va_idx) > 0)
            {
                for (std::size_t j = 0; j < va_headers.size(); ++j)
                {
                    va_headers[j].layout_index = va_views[j].layout_index;
                }
            }
        }

        // --- Decompress vertex arrays ---
        std::unordered_map<std::uint32_t, VertexArray> vertex_arrays;
        for (std::uint32_t i = 0; i < vertex_array_count; ++i)
        {
            const auto& hdr = va_headers[i];
            const auto& layout = layouts[hdr.layout_index];

            // Check layout size vs header vertex_size.
            if (layout.compressed_vertex_size != hdr.vertex_size)
            {
                // DSR layout mismatch — create empty array.
                VertexArray va;
                va.layout = layout;
                va.vertex_count = 0;
                vertex_arrays[i] = std::move(va);
                continue;
            }

            auto guard = r.TempOffset(vertex_data_offset + hdr.array_offset);
            const std::byte* raw = r.RawAt(vertex_data_offset + hdr.array_offset);
            vertex_arrays[i] = DecompressVertexArray(raw, hdr.vertex_count, layout, uv_factor);
        }

        // --- Read textures ---
        std::unordered_map<std::uint32_t, Texture> textures;
        for (std::uint32_t i = 0; i < texture_count; ++i)
        {
            textures[i] = ReadTextureFLVER2(r, unicode_enc);
        }

        // --- Assign textures to materials ---
        for (auto& mi : mat_infos)
        {
            for (std::uint32_t t = mi.first_texture_index; t < mi.first_texture_index + mi.texture_count; ++t)
            {
                auto it = textures.find(t);
                if (it == textures.end())
                {
                    throw FLVERError("Texture index " + std::to_string(t) + " already assigned or doesn't exist");
                }
                mi.mat.textures.push_back(std::move(it->second));
                textures.erase(it);
            }
        }

        // --- Assign face sets and vertex arrays to meshes ---
        flver.meshes.reserve(mesh_count);
        for (auto& ms : mesh_states)
        {
            // Copy (not move) — multiple meshes may reference the same material.
            ms.mesh.material = mat_infos[ms.material_index].mat;

            for (auto idx : ms.face_set_indices)
            {
                auto it = face_sets.find(idx);
                if (it == face_sets.end())
                {
                    throw FLVERError("FaceSet index " + std::to_string(idx) + " already assigned or doesn't exist");
                }
                ms.mesh.face_sets.push_back(std::move(it->second));
                face_sets.erase(it);
            }

            for (auto idx : ms.vertex_array_indices)
            {
                auto it = vertex_arrays.find(idx);
                if (it == vertex_arrays.end())
                {
                    throw FLVERError("VertexArray index " + std::to_string(idx) + " already assigned or doesn't exist");
                }
                ms.mesh.vertex_arrays.push_back(std::move(it->second));
                vertex_arrays.erase(it);
            }

            flver.meshes.push_back(std::move(ms.mesh));
        }

        return flver;
    }

    FLVER FLVER::FromPath(const std::filesystem::path& path)
    {
        const std::vector<std::byte> data = BinaryReadWrite::ReadFileBytes(path);
        auto flver = FromBytes(data.data(), data.size());

        // Assign path. If `path` ends in '.bak', strip that first.
        if (path.has_filename() && path.filename().string().ends_with(".bak"))
            flver.path = path.parent_path() / path.stem();
        else
            flver.path = path;
        return flver;
    }

    // ---------------------------------------------------------------------------
    // FLVER::ToBytes
    // ---------------------------------------------------------------------------

    std::vector<std::byte> FLVER::ToBytes() const
    {
        if (is_flver0(version))
            return ToBytesFLVER0();
        return ToBytesFLVER2();
    }

    // ---------------------------------------------------------------------------
    // Dummy / Bone writers (shared by FLVER0 and FLVER2)
    // ---------------------------------------------------------------------------

    namespace
    {
        void WriteDummy(BufferWriter& w, const Dummy& d)
        {
            w.Write<float>(d.translate.x);
            w.Write<float>(d.translate.y);
            w.Write<float>(d.translate.z);
            w.Write<std::uint8_t>(d.color.r);
            w.Write<std::uint8_t>(d.color.g);
            w.Write<std::uint8_t>(d.color.b);
            w.Write<std::uint8_t>(d.color.a);
            w.Write<float>(d.forward.x);
            w.Write<float>(d.forward.y);
            w.Write<float>(d.forward.z);
            w.Write<std::int16_t>(d.reference_id);
            w.Write<std::int16_t>(d.parent_bone_index);
            w.Write<float>(d.upward.x);
            w.Write<float>(d.upward.y);
            w.Write<float>(d.upward.z);
            w.Write<std::int16_t>(d.attach_bone_index);
            w.Write<std::uint8_t>(d.follows_attach_bone ? 1 : 0);
            w.Write<std::uint8_t>(d.use_upward_vector ? 1 : 0);
            w.Write<std::int32_t>(d.unk_x30);
            w.Write<std::int32_t>(d.unk_x34);
            w.WritePad(8);
        }

        // Write a bone struct (128 bytes). Name offset is reserved.
        void WriteBoneStruct(BufferWriter& w, const Bone& b, const void* scope)
        {
            w.Write<float>(b.translate.x);
            w.Write<float>(b.translate.y);
            w.Write<float>(b.translate.z);
            w.Reserve<std::uint32_t>("bone_name_offset", scope);
            w.Write<float>(b.rotate.x);
            w.Write<float>(b.rotate.y);
            w.Write<float>(b.rotate.z);
            w.Write<std::int16_t>(b.parent_bone_index);
            w.Write<std::int16_t>(b.child_bone_index);
            w.Write<float>(b.scale.x);
            w.Write<float>(b.scale.y);
            w.Write<float>(b.scale.z);
            w.Write<std::int16_t>(b.next_sibling_bone_index);
            w.Write<std::int16_t>(b.previous_sibling_bone_index);
            w.Write<float>(b.bounding_box.min.x);
            w.Write<float>(b.bounding_box.min.y);
            w.Write<float>(b.bounding_box.min.z);
            w.Write<std::int32_t>(b.usage_flags);
            w.Write<float>(b.bounding_box.max.x);
            w.Write<float>(b.bounding_box.max.y);
            w.Write<float>(b.bounding_box.max.z);
            w.WritePad(52);
        }

        // Count faces for FLVER0. All face sets should be strips.
        void CountFaces0(const std::vector<Mesh>& meshes, std::int32_t& true_fc, std::int32_t& total_fc)
        {
            true_fc = 0;
            total_fc = 0;
            for (const auto& mesh : meshes)
            {
                const auto& fs = mesh.face_sets[0];
                const auto& vi = fs.vertex_indices;
                if (!fs.is_triangle_strip)
                {
                    auto n = static_cast<std::int32_t>(vi.size() / 3);
                    true_fc += n;
                    total_fc += n;
                    continue;
                }
                for (std::size_t i = 0; i + 2 < vi.size(); ++i)
                {
                    bool has_0xffff = (vi[i] == 0xFFFF || vi[i + 1] == 0xFFFF || vi[i + 2] == 0xFFFF);
                    if (has_0xffff) continue;
                    total_fc++;
                    // True if not MotionBlur and non-degenerate.
                    if (vi[i] != vi[i + 1] && vi[i] != vi[i + 2] && vi[i + 1] != vi[i + 2])
                        true_fc++;
                }
            }
        }
    } // anonymous namespace

    // ---------------------------------------------------------------------------
    // FLVER0 writer
    // ---------------------------------------------------------------------------

    std::vector<std::byte> FLVER::ToBytesFLVER0() const
    {
        Endian endian = big_endian ? Endian::Big : Endian::Little;
        BufferWriter w(endian);
        bool ue = unicode;
        float uv_factor = big_endian ? 1024.f : 2048.f;

        // Compute face counts.
        std::int32_t true_fc = 0, total_fc = 0;
        CountFaces0(meshes, true_fc, total_fc);

        // --- Deduplicate materials ---
        struct PackedMat
        {
            std::size_t flver_mat_index; // index into `materials_to_pack`
            std::size_t layout_index;    // index into that material's layout list
        };

        std::vector<const Material*> materials_to_pack;
        std::vector<std::vector<VertexArrayLayout>> material_layouts;
        std::unordered_map<std::size_t, std::size_t> hashed_mat_indices; // hash -> materials_to_pack index
        std::vector<PackedMat> mesh_pack_info; // one per mesh

        for (const auto& mesh : meshes)
        {
            auto mh = hash_material(mesh.material);
            std::size_t mat_idx;
            if (auto it = hashed_mat_indices.find(mh); it != hashed_mat_indices.end())
            {
                mat_idx = it->second;
            }
            else
            {
                mat_idx = materials_to_pack.size();
                hashed_mat_indices[mh] = mat_idx;
                materials_to_pack.push_back(&mesh.material);
                material_layouts.emplace_back();
            }

            // Deduplicate layout within this material.
            const auto& layout = mesh.vertex_arrays[0].layout;
            auto lh = hash_layout(layout);
            std::size_t layout_idx = 0;
            bool found = false;
            for (std::size_t li = 0; li < material_layouts[mat_idx].size(); ++li)
            {
                if (hash_layout(material_layouts[mat_idx][li]) == lh)
                {
                    layout_idx = li;
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                layout_idx = material_layouts[mat_idx].size();
                material_layouts[mat_idx].push_back(layout);
            }

            mesh_pack_info.push_back({mat_idx, layout_idx});
        }

        auto mat_count = static_cast<std::uint32_t>(materials_to_pack.size());

        // --- Write FLVER0 header (128 bytes) ---
        // "FLVER\0" + endian tag
        w.WriteRaw("FLVER", 5);
        w.WritePad(1); // null
        if (big_endian) { w.WriteRaw("B", 1); w.WritePad(1); }
        else { w.WriteRaw("L", 1); w.WritePad(1); }
        w.Write<std::uint32_t>(static_cast<std::uint32_t>(version));
        w.Reserve<std::uint32_t>("vertex_data_offset");
        w.Reserve<std::uint32_t>("vertex_data_size");
        w.Write<std::uint32_t>(static_cast<std::uint32_t>(dummies.size()));
        w.Write<std::uint32_t>(mat_count);
        w.Write<std::uint32_t>(static_cast<std::uint32_t>(bones.size()));
        w.Write<std::uint32_t>(static_cast<std::uint32_t>(meshes.size()));
        w.Write<std::uint32_t>(static_cast<std::uint32_t>(meshes.size())); // vertex_array_count = mesh_count
        w.Write<float>(bounding_box_min.x);
        w.Write<float>(bounding_box_min.y);
        w.Write<float>(bounding_box_min.z);
        w.Write<float>(bounding_box_max.x);
        w.Write<float>(bounding_box_max.y);
        w.Write<float>(bounding_box_max.z);
        w.Write<std::int32_t>(true_fc);
        w.Write<std::int32_t>(total_fc);
        w.Write<std::uint8_t>(16); // vertex_index_bit_size
        w.Write<std::uint8_t>(unicode ? 1 : 0);
        w.Write<std::uint8_t>(f0_unk_x4a);
        w.Write<std::uint8_t>(f0_unk_x4b);
        w.Write<std::uint32_t>(f0_unk_x4c);
        w.WritePad(12); // no face_set_count, layout_count, texture_count
        w.Write<std::uint8_t>(f0_unk_x5c);
        w.WritePad(3);
        w.WritePad(32);
        // Header: 128 bytes.

        // --- Dummies ---
        for (const auto& d : dummies)
            WriteDummy(w, d);

        // --- Material headers (32 bytes each) ---
        // Use material pointers as scope keys for reservations.
        for (std::size_t i = 0; i < mat_count; ++i)
        {
            const void* scope = materials_to_pack[i];
            w.Reserve<std::uint32_t>("mat_name_offset", scope);
            w.Reserve<std::uint32_t>("mat_def_offset", scope);
            w.Reserve<std::uint32_t>("mat_tex_offset", scope);
            w.Reserve<std::uint32_t>("mat_layouts_offset", scope);
            w.Reserve<std::uint32_t>("mat_size", scope);
            w.Reserve<std::uint32_t>("mat_layout_hdr_offset", scope);
            w.WritePad(8);
        }

        // --- Bone structs ---
        for (std::size_t i = 0; i < bones.size(); ++i)
            WriteBoneStruct(w, bones[i], &bones[i]);

        // --- Mesh headers (100 bytes each) ---
        for (std::size_t mi = 0; mi < meshes.size(); ++mi)
        {
            const auto& mesh = meshes[mi];
            const auto& fs = mesh.face_sets[0];
            const void* scope = &mesh;

            w.Write<std::uint8_t>(mesh.is_dynamic ? 1 : 0);
            w.Write<std::uint8_t>(static_cast<std::uint8_t>(mesh_pack_info[mi].flver_mat_index));
            w.Write<std::uint8_t>(fs.use_backface_culling ? 1 : 0);
            w.Write<std::uint8_t>(fs.is_triangle_strip ? 1 : 0);
            auto vi_count = static_cast<std::uint32_t>(fs.vertex_indices.size());
            w.Write<std::uint32_t>(vi_count);
            w.Write<std::uint32_t>(mesh.vertex_arrays[0].vertex_count);
            w.Write<std::int16_t>(static_cast<std::int16_t>(mesh.default_bone_index));

            // 28 bone indices.
            for (int b = 0; b < 28; ++b)
            {
                if (b < static_cast<int>(mesh.bone_indices.size()))
                    w.Write<std::int16_t>(static_cast<std::int16_t>(mesh.bone_indices[b]));
                else
                    w.Write<std::int16_t>(-1);
            }

            w.Write<std::uint16_t>(mesh.f0_unk_x46);

            auto vi_byte_size = vi_count * 2u; // 16-bit indices
            w.Write<std::uint32_t>(vi_byte_size);
            w.Reserve<std::uint32_t>("mesh_vi_offset", scope);

            // Vertex array data size/offset (used when va_headers_offset_1 == 0).
            auto write_vert_size = WriteCompressedVertexSize(mesh.vertex_arrays[0].layout);
            w.Write<std::uint32_t>(write_vert_size * mesh.vertex_arrays[0].vertex_count);
            w.Reserve<std::uint32_t>("mesh_va_data_offset", scope);
            w.Reserve<std::uint32_t>("mesh_va_hdr_offset_1", scope);
            w.Write<std::uint32_t>(0); // va_headers_offset_2 always 0
            w.WritePad(4);
        }

        // --- Material data blocks ---
        for (std::size_t i = 0; i < mat_count; ++i)
        {
            const auto* mat = materials_to_pack[i];
            const void* scope = mat;
            auto start_pos = w.Position();

            // Name.
            w.FillWithPosition<std::uint32_t>("mat_name_offset", scope);
            write_string(w, mat->name, ue);

            // Mat def path.
            w.FillWithPosition<std::uint32_t>("mat_def_offset", scope);
            write_string(w, mat->mat_def_path, ue);

            // Textures.
            w.FillWithPosition<std::uint32_t>("mat_tex_offset", scope);
            w.Write<std::uint8_t>(static_cast<std::uint8_t>(mat->textures.size()));
            w.WritePad(3);
            w.WritePad(12);

            // Texture headers.
            for (std::size_t ti = 0; ti < mat->textures.size(); ++ti)
            {
                const void* tex_scope = &mat->textures[ti];
                w.Reserve<std::uint32_t>("tex_path_offset", tex_scope);
                w.Reserve<std::uint32_t>("tex_type_offset", tex_scope);
                w.WritePad(8);
            }

            // Texture strings.
            for (const auto& tex : mat->textures)
            {
                const void* tex_scope = &tex;
                w.FillWithPosition<std::uint32_t>("tex_path_offset", tex_scope);
                write_string(w, tex.path, ue);
                if (tex.texture_type.has_value())
                {
                    w.FillWithPosition<std::uint32_t>("tex_type_offset", tex_scope);
                    write_string(w, tex.texture_type.value(), ue);
                }
                else
                {
                    w.Fill<std::uint32_t>("tex_type_offset", 0, tex_scope);
                }
            }

            // Layout header.
            const auto& layouts = material_layouts[i];
            w.FillWithPosition<std::uint32_t>("mat_layout_hdr_offset", scope);
            w.Write<std::int32_t>(static_cast<std::int32_t>(layouts.size()));
            w.Write<std::int32_t>(static_cast<std::int32_t>(w.Position() + 12)); // layout data offset
            w.WritePad(8);

            // Layout offset table.
            for (std::size_t li = 0; li < layouts.size(); ++li)
                w.Reserve<std::uint32_t>("layout_offset_" + std::to_string(li), scope);

            // Layouts offset and layouts data.
            w.FillWithPosition<std::uint32_t>("mat_layouts_offset", scope);
            for (std::size_t li = 0; li < layouts.size(); ++li)
            {
                w.FillWithPosition<std::uint32_t>("layout_offset_" + std::to_string(li), scope);
                const auto& layout = layouts[li];
                // Write VertexArrayLayout FLVER0 struct: 16-byte header + types.
                std::uint32_t write_types_count = 0;
                std::uint32_t write_struct_size = 0;
                for (const auto& dt : layout.types)
                {
                    if (dt.usage == VertexUsage::Ignore) continue;
                    write_types_count++;
                    write_struct_size += FormatEnumSize(dt.format);
                }
                w.Write<std::int16_t>(static_cast<std::int16_t>(write_types_count));
                w.Write<std::int16_t>(static_cast<std::int16_t>(write_struct_size));
                w.WritePad(12);

                std::uint32_t dt_offset = 0;
                for (const auto& dt : layout.types)
                {
                    if (dt.usage == VertexUsage::Ignore) continue;
                    w.Write<std::uint32_t>(dt.unk_x00);
                    w.Write<std::uint32_t>(dt_offset);
                    w.Write<std::uint32_t>(static_cast<std::uint32_t>(dt.format));
                    w.Write<std::uint32_t>(FromVertexUsage(dt.usage));
                    w.Write<std::uint32_t>(dt.instance_index);
                    dt_offset += FormatEnumSize(dt.format);
                }
            }

            auto end_pos = w.Position();
            w.Fill<std::uint32_t>("mat_size", static_cast<std::uint32_t>(end_pos - start_pos), scope);
        }

        // --- Bone names ---
        for (const auto& bone : bones)
        {
            w.FillWithPosition<std::uint32_t>("bone_name_offset", &bone);
            write_string(w, bone.name, ue);
        }

        // --- Mesh vertex array headers ---
        for (std::size_t mi = 0; mi < meshes.size(); ++mi)
        {
            const auto& mesh = meshes[mi];
            const void* scope = &mesh;

            w.FillWithPosition<std::uint32_t>("mesh_va_hdr_offset_1", scope);

            // Array list header: count(4), offset(4), pad(8).
            w.Write<std::int32_t>(1); // one array
            w.Write<std::int32_t>(static_cast<std::int32_t>(w.Position() + 12)); // offset to header data
            w.WritePad(8);

            // Single vertex array header (16 bytes).
            auto layout_idx = static_cast<std::uint32_t>(mesh_pack_info[mi].layout_index);
            auto write_size = WriteCompressedVertexSize(mesh.vertex_arrays[0].layout);
            auto vc = mesh.vertex_arrays[0].vertex_count;
            w.Write<std::uint32_t>(layout_idx);
            w.Write<std::uint32_t>(write_size * vc); // array_length
            w.Reserve<std::uint32_t>("va_array_offset", scope);
            w.WritePad(4);
        }

        // --- Vertex data ---
        w.PadAlign(32);
        auto vertex_data_start = w.Position();
        w.Fill<std::uint32_t>("vertex_data_offset", static_cast<std::uint32_t>(vertex_data_start));

        for (std::size_t mi = 0; mi < meshes.size(); ++mi)
        {
            const auto& mesh = meshes[mi];
            const void* scope = &mesh;

            // Face set indices.
            auto indices_offset = w.Position() - vertex_data_start;
            w.Fill<std::uint32_t>("mesh_vi_offset", static_cast<std::uint32_t>(indices_offset), scope);
            const auto& fs = mesh.face_sets[0];
            for (auto idx : fs.vertex_indices)
                w.Write<std::uint16_t>(static_cast<std::uint16_t>(idx));

            w.PadAlign(32);

            // Vertex array data.
            auto array_offset = w.Position() - vertex_data_start;
            w.Fill<std::uint32_t>("mesh_va_data_offset", static_cast<std::uint32_t>(array_offset), scope);
            w.Fill<std::uint32_t>("va_array_offset", static_cast<std::uint32_t>(array_offset), scope);

            auto compressed = CompressVertexArray(mesh.vertex_arrays[0], uv_factor);
            w.WriteRaw(compressed.data(), compressed.size());
            w.PadAlign(32);
        }

        auto vertex_data_end = w.Position();
        w.Fill<std::uint32_t>("vertex_data_size", static_cast<std::uint32_t>(vertex_data_end - vertex_data_start));

        return w.Finalize();
    }

    // ---------------------------------------------------------------------------
    // FLVER2 writer
    // ---------------------------------------------------------------------------

    std::vector<std::byte> FLVER::ToBytesFLVER2() const
    {
        Endian endian = big_endian ? Endian::Big : Endian::Little;
        BufferWriter w(endian);
        bool ue = unicode;
        float uv_factor = version >= FLVERVersion::DarkSouls2_NT ? 2048.f : 1024.f;

        // --- Pre-compute face counts and header index size ---
        std::int32_t true_fc = 0, total_fc = 0;
        std::uint8_t header_vi_bit_size = 0;
        if (version < FLVERVersion::Bloodborne_DS3_A)
        {
            header_vi_bit_size = 16;
        }

        for (const auto& mesh : meshes)
        {
            bool uses_0xffff = (mesh.vertex_arrays.size() == 1 && mesh.vertex_arrays[0].vertex_count <= 0xFFFF);
            for (const auto& fs : mesh.face_sets)
            {
                if (!fs.is_triangle_strip)
                {
                    auto n = static_cast<std::int32_t>(fs.vertex_indices.size() / 3);
                    true_fc += n;
                    total_fc += n;
                }
                else
                {
                    for (std::size_t i = 0; i + 2 < fs.vertex_indices.size(); ++i)
                    {
                        const auto& vi = fs.vertex_indices;
                        bool has_sep = uses_0xffff && (vi[i] == 0xFFFF || vi[i+1] == 0xFFFF || vi[i+2] == 0xFFFF);
                        if (has_sep) continue;
                        total_fc++;
                        if (!(fs.flags & FaceSetFlags::MotionBlur)
                            && vi[i] != vi[i+1] && vi[i] != vi[i+2] && vi[i+1] != vi[i+2])
                            true_fc++;
                    }
                }

                // Check if any face set needs 32-bit indices (pre-Bloodborne).
                if (header_vi_bit_size == 16)
                {
                    for (auto idx : fs.vertex_indices)
                    {
                        if (idx > 0xFFFF) { header_vi_bit_size = 32; break; }
                    }
                }
            }
        }

        // --- Deduplicate materials, layouts, GX lists ---
        std::vector<const Material*> mats_to_pack;
        std::vector<std::vector<std::size_t>> mesh_layout_indices; // per mesh, layout index per vertex_array
        std::vector<std::size_t> mesh_mat_indices; // per mesh, index into mats_to_pack

        std::vector<VertexArrayLayout> layouts_to_pack;
        std::unordered_map<std::size_t, std::size_t> hashed_mat_idx;
        std::unordered_map<std::size_t, std::size_t> hashed_layout_idx;

        struct GXListInfo { std::vector<const Material*> users; };
        std::vector<const std::vector<GXItem>*> gx_lists_to_pack;
        std::vector<std::vector<const Material*>> gx_list_users;
        std::unordered_map<std::size_t, std::size_t> hashed_gx_idx;

        for (const auto& mesh : meshes)
        {
            auto mh = hash_material(mesh.material);
            std::size_t mat_idx;
            if (auto it = hashed_mat_idx.find(mh); it != hashed_mat_idx.end())
            {
                mat_idx = it->second;
            }
            else
            {
                mat_idx = mats_to_pack.size();
                hashed_mat_idx[mh] = mat_idx;
                mats_to_pack.push_back(&mesh.material);

                // Check GX items.
                if (!mesh.material.gx_items.empty())
                {
                    auto gh = hash_gx_list(mesh.material.gx_items);
                    if (auto it2 = hashed_gx_idx.find(gh); it2 != hashed_gx_idx.end())
                    {
                        gx_list_users[it2->second].push_back(&mesh.material);
                    }
                    else
                    {
                        hashed_gx_idx[gh] = gx_lists_to_pack.size();
                        gx_lists_to_pack.push_back(&mesh.material.gx_items);
                        gx_list_users.push_back({&mesh.material});
                    }
                }
            }
            mesh_mat_indices.push_back(mat_idx);

            std::vector<std::size_t> arr_layout_ids;
            for (const auto& va : mesh.vertex_arrays)
            {
                auto lh = hash_layout(va.layout);
                if (auto it = hashed_layout_idx.find(lh); it != hashed_layout_idx.end())
                {
                    arr_layout_ids.push_back(it->second);
                }
                else
                {
                    auto li = layouts_to_pack.size();
                    hashed_layout_idx[lh] = li;
                    layouts_to_pack.push_back(va.layout);
                    arr_layout_ids.push_back(li);
                }
            }
            mesh_layout_indices.push_back(std::move(arr_layout_ids));
        }

        auto packed_mat_count = static_cast<std::uint32_t>(mats_to_pack.size());
        auto packed_layout_count = static_cast<std::uint32_t>(layouts_to_pack.size());
        std::uint32_t packed_texture_count = 0;
        for (const auto* mat : mats_to_pack)
            packed_texture_count += static_cast<std::uint32_t>(mat->textures.size());

        std::uint32_t total_va_count = 0;
        std::uint32_t total_fs_count = 0;
        for (const auto& mesh : meshes)
        {
            total_va_count += static_cast<std::uint32_t>(mesh.vertex_arrays.size());
            total_fs_count += static_cast<std::uint32_t>(mesh.face_sets.size());
        }

        // --- Write FLVER2 header (128 bytes) ---
        w.WriteRaw("FLVER", 5);
        w.WritePad(1);
        if (big_endian) { w.WriteRaw("B", 1); w.WritePad(1); }
        else { w.WriteRaw("L", 1); w.WritePad(1); }
        w.Write<std::uint32_t>(static_cast<std::uint32_t>(version));
        w.Reserve<std::uint32_t>("vertex_data_offset");
        w.Reserve<std::uint32_t>("vertex_data_size");
        w.Write<std::uint32_t>(static_cast<std::uint32_t>(dummies.size()));
        w.Write<std::uint32_t>(packed_mat_count);
        w.Write<std::uint32_t>(static_cast<std::uint32_t>(bones.size()));
        w.Write<std::uint32_t>(static_cast<std::uint32_t>(meshes.size()));
        w.Write<std::uint32_t>(total_va_count);
        w.Write<float>(bounding_box_min.x);
        w.Write<float>(bounding_box_min.y);
        w.Write<float>(bounding_box_min.z);
        w.Write<float>(bounding_box_max.x);
        w.Write<float>(bounding_box_max.y);
        w.Write<float>(bounding_box_max.z);
        w.Write<std::int32_t>(true_fc);
        w.Write<std::int32_t>(total_fc);
        w.Write<std::uint8_t>(header_vi_bit_size);
        w.Write<std::uint8_t>(unicode ? 1 : 0);
        w.Write<std::uint8_t>(f2_unk_x4a ? 1 : 0);
        w.WritePad(1);
        w.Write<std::int32_t>(f2_unk_x4c);
        w.Write<std::uint32_t>(total_fs_count);
        w.Write<std::uint32_t>(packed_layout_count);
        w.Write<std::uint32_t>(packed_texture_count);
        w.Write<std::uint8_t>(static_cast<std::uint8_t>(f2_unk_x5c));
        w.Write<std::uint8_t>(static_cast<std::uint8_t>(f2_unk_x5d));
        w.WritePad(10);
        w.Write<std::int32_t>(f2_unk_x68);
        w.WritePad(20);

        // --- Dummies ---
        for (const auto& d : dummies)
            WriteDummy(w, d);

        // --- Material headers (32 bytes each) ---
        for (std::size_t i = 0; i < packed_mat_count; ++i)
        {
            const void* scope = mats_to_pack[i];
            w.Reserve<std::uint32_t>("mat2_name_offset", scope);
            w.Reserve<std::uint32_t>("mat2_def_offset", scope);
            w.Write<std::uint32_t>(static_cast<std::uint32_t>(mats_to_pack[i]->textures.size()));
            w.Reserve<std::uint32_t>("mat2_first_tex_idx", scope);
            w.Write<std::int32_t>(mats_to_pack[i]->flags);
            if (!mats_to_pack[i]->gx_items.empty())
                w.Reserve<std::uint32_t>("mat2_gx_offset", scope);
            else
                w.Write<std::uint32_t>(0);
            w.Write<std::int32_t>(mats_to_pack[i]->f2_unk_x18);
            w.WritePad(4);
        }

        // --- Bones ---
        for (const auto& bone : bones)
            WriteBoneStruct(w, bone, &bone);

        // --- Mesh headers (44 bytes each) ---
        for (std::size_t mi = 0; mi < meshes.size(); ++mi)
        {
            const auto& mesh = meshes[mi];
            const void* scope = &mesh;
            w.Write<std::uint8_t>(mesh.is_dynamic ? 1 : 0);
            w.WritePad(3);
            w.Write<std::uint32_t>(static_cast<std::uint32_t>(mesh_mat_indices[mi]));
            w.WritePad(8);
            w.Write<std::int32_t>(mesh.default_bone_index);
            w.Write<std::uint32_t>(static_cast<std::uint32_t>(mesh.bone_indices.size()));
            w.Reserve<std::uint32_t>("mesh2_bbox_offset", scope);
            w.Reserve<std::uint32_t>("mesh2_bone_offset", scope);
            w.Write<std::uint32_t>(static_cast<std::uint32_t>(mesh.face_sets.size()));
            w.Reserve<std::uint32_t>("mesh2_fs_offset", scope);
            w.Write<std::uint32_t>(static_cast<std::uint32_t>(mesh.vertex_arrays.size()));
            w.Reserve<std::uint32_t>("mesh2_va_offset", scope);
        }

        // --- FaceSet headers ---
        // Compute per-face-set index sizes for later.
        std::vector<std::uint32_t> fs_index_sizes;
        for (const auto& mesh : meshes)
        {
            for (const auto& fs : mesh.face_sets)
            {
                std::uint32_t fs_bit_size;
                if (header_vi_bit_size != 0)
                {
                    fs_bit_size = header_vi_bit_size;
                }
                else
                {
                    // Per-face-set decision.
                    fs_bit_size = 16;
                    for (auto idx : fs.vertex_indices)
                    {
                        if (idx > 0xFFFF) { fs_bit_size = 32; break; }
                    }
                }
                fs_index_sizes.push_back(fs_bit_size);

                // Write header (28 bytes).
                const void* fs_scope = &fs;
                w.WritePad(3);
                w.Write<std::uint8_t>(static_cast<std::uint8_t>(fs.flags));
                w.Write<std::uint8_t>(fs.is_triangle_strip ? 1 : 0);
                w.Write<std::uint8_t>(fs.use_backface_culling ? 1 : 0);
                w.Write<std::int16_t>(fs.unk_x06);
                auto vi_count = static_cast<std::uint32_t>(fs.vertex_indices.size());
                w.Write<std::uint32_t>(vi_count);
                w.Reserve<std::uint32_t>("fs2_vi_offset", fs_scope);
                w.Write<std::uint32_t>(vi_count * fs_bit_size / 8); // vertex_indices_length
                w.WritePad(4);
                // Per-FaceSet index size (0 if header-global).
                w.Write<std::uint32_t>(header_vi_bit_size != 0 ? 0 : fs_bit_size);
                w.WritePad(4);
            }
        }

        // --- Vertex array headers ---
        std::uint32_t va_global_idx = 0;
        for (std::size_t mi = 0; mi < meshes.size(); ++mi)
        {
            for (std::size_t ai = 0; ai < meshes[mi].vertex_arrays.size(); ++ai)
            {
                const auto& va = meshes[mi].vertex_arrays[ai];
                const void* va_scope = &va;
                auto layout_idx = static_cast<std::uint32_t>(mesh_layout_indices[mi][ai]);
                auto write_size = WriteCompressedVertexSize(va.layout);
                bool write_length = (version >= static_cast<FLVERVersion>(0x20005));

                w.Write<std::uint32_t>(static_cast<std::uint32_t>(ai)); // array_index
                w.Write<std::uint32_t>(layout_idx);
                w.Write<std::uint32_t>(write_size);
                w.Write<std::uint32_t>(va.vertex_count);
                w.WritePad(8);
                w.Write<std::uint32_t>(write_length ? write_size * va.vertex_count : 0);
                w.Reserve<std::uint32_t>("va2_offset", va_scope);
                va_global_idx++;
            }
        }

        // --- Layout headers ---
        for (const auto& layout : layouts_to_pack)
        {
            const void* l_scope = &layout;
            std::uint32_t write_count = 0;
            for (const auto& dt : layout.types)
                if (dt.usage != VertexUsage::Ignore) write_count++;
            w.Write<std::uint32_t>(write_count);
            w.WritePad(8);
            w.Reserve<std::uint32_t>("layout2_types_offset", l_scope);
        }

        // --- Texture headers ---
        {
            std::uint32_t first_tex_idx = 0;
            for (std::size_t i = 0; i < packed_mat_count; ++i)
            {
                const auto* mat = mats_to_pack[i];
                const void* mat_scope = mat;
                w.Fill<std::uint32_t>("mat2_first_tex_idx", first_tex_idx, mat_scope);
                for (const auto& tex : mat->textures)
                {
                    const void* tex_scope = &tex;
                    w.Reserve<std::uint32_t>("tex2_path_offset", tex_scope);
                    w.Reserve<std::uint32_t>("tex2_type_offset", tex_scope);
                    w.Write<float>(tex.scale.x);
                    w.Write<float>(tex.scale.y);
                    w.Write<std::uint8_t>(tex.f2_unk_x10);
                    w.Write<std::uint8_t>(tex.f2_unk_x11 ? 1 : 0);
                    w.WritePad(2);
                    w.Write<float>(tex.f2_unk_x14);
                    w.Write<float>(tex.f2_unk_x18);
                    w.Write<float>(tex.f2_unk_x1c);
                }
                first_tex_idx += static_cast<std::uint32_t>(mat->textures.size());
            }
        }

        // --- Layout types data (pad 16) ---
        w.PadAlign(16);
        for (const auto& layout : layouts_to_pack)
        {
            const void* l_scope = &layout;
            w.FillWithPosition<std::uint32_t>("layout2_types_offset", l_scope);
            std::uint32_t dt_offset = 0;
            for (const auto& dt : layout.types)
            {
                if (dt.usage == VertexUsage::Ignore) continue;
                w.Write<std::uint32_t>(dt.unk_x00);
                w.Write<std::uint32_t>(dt_offset);
                w.Write<std::uint32_t>(static_cast<std::uint32_t>(dt.format));
                w.Write<std::uint32_t>(FromVertexUsage(dt.usage));
                w.Write<std::uint32_t>(dt.instance_index);
                dt_offset += FormatEnumSize(dt.format);
            }
        }

        // --- Mesh bounding boxes (pad 16) ---
        w.PadAlign(16);
        for (const auto& mesh : meshes)
        {
            const void* scope = &mesh;
            if (!mesh.uses_bounding_boxes)
            {
                w.Fill<std::uint32_t>("mesh2_bbox_offset", 0, scope);
            }
            else
            {
                w.FillWithPosition<std::uint32_t>("mesh2_bbox_offset", scope);
                w.Write<float>(mesh.bounding_box_min.x);
                w.Write<float>(mesh.bounding_box_min.y);
                w.Write<float>(mesh.bounding_box_min.z);
                w.Write<float>(mesh.bounding_box_max.x);
                w.Write<float>(mesh.bounding_box_max.y);
                w.Write<float>(mesh.bounding_box_max.z);
                if (mesh.sekiro_bbox_unk.has_value())
                {
                    w.Write<float>(mesh.sekiro_bbox_unk->x);
                    w.Write<float>(mesh.sekiro_bbox_unk->y);
                    w.Write<float>(mesh.sekiro_bbox_unk->z);
                }
            }
        }

        // --- Mesh bone indices (pad 16) ---
        w.PadAlign(16);
        auto bone_indices_start = w.Position();
        for (const auto& mesh : meshes)
        {
            const void* scope = &mesh;
            if (mesh.bone_indices.empty())
            {
                w.Fill<std::uint32_t>("mesh2_bone_offset", static_cast<std::uint32_t>(bone_indices_start), scope);
            }
            else
            {
                w.FillWithPosition<std::uint32_t>("mesh2_bone_offset", scope);
                for (auto bi : mesh.bone_indices)
                    w.Write<std::int32_t>(bi);
            }
        }

        // --- Mesh face set indices (pad 16) ---
        w.PadAlign(16);
        {
            std::uint32_t first_fs = 0;
            for (const auto& mesh : meshes)
            {
                const void* scope = &mesh;
                w.FillWithPosition<std::uint32_t>("mesh2_fs_offset", scope);
                for (std::uint32_t fi = 0; fi < mesh.face_sets.size(); ++fi)
                    w.Write<std::int32_t>(static_cast<std::int32_t>(first_fs + fi));
                first_fs += static_cast<std::uint32_t>(mesh.face_sets.size());
            }
        }

        // --- Mesh vertex array indices (pad 16) ---
        w.PadAlign(16);
        {
            std::uint32_t first_va = 0;
            for (const auto& mesh : meshes)
            {
                const void* scope = &mesh;
                w.FillWithPosition<std::uint32_t>("mesh2_va_offset", scope);
                for (std::uint32_t vi = 0; vi < mesh.vertex_arrays.size(); ++vi)
                    w.Write<std::int32_t>(static_cast<std::int32_t>(first_va + vi));
                first_va += static_cast<std::uint32_t>(mesh.vertex_arrays.size());
            }
        }

        // --- GX item lists (pad 16) ---
        w.PadAlign(16);
        for (std::size_t gi = 0; gi < gx_lists_to_pack.size(); ++gi)
        {
            const auto* gx_list = gx_lists_to_pack[gi];
            auto gx_offset = w.Position();

            for (const auto& item : *gx_list)
            {
                w.WriteRaw(item.category.data(), 4);
                w.Write<std::int32_t>(item.index);
                w.Write<std::int32_t>(static_cast<std::int32_t>(item.data.size() + 12));
                if (!item.data.empty())
                    w.WriteRaw(item.data.data(), item.data.size());
            }

            // Add terminator if missing (post-DS2).
            if (!gx_list->empty() && !gx_list->back().IsTerminator()
                && version != FLVERVersion::DarkSouls2)
            {
                constexpr char term_cat[] = {'\xff', '\xff', '\xff', '\x7f'};
                w.WriteRaw(term_cat, 4);
                w.Write<std::int32_t>(100);
                w.Write<std::int32_t>(12); // no data
            }

            // Fill GX offset for all materials using this list.
            for (const auto* mat : gx_list_users[gi])
                w.Fill<std::uint32_t>("mat2_gx_offset", static_cast<std::uint32_t>(gx_offset), mat);
        }

        // --- Material/texture strings (pad 16) ---
        w.PadAlign(16);
        for (const auto* mat : mats_to_pack)
        {
            const void* scope = mat;
            w.FillWithPosition<std::uint32_t>("mat2_name_offset", scope);
            write_string(w, mat->name, ue);
            w.FillWithPosition<std::uint32_t>("mat2_def_offset", scope);
            write_string(w, mat->mat_def_path, ue);

            for (const auto& tex : mat->textures)
            {
                const void* tex_scope = &tex;
                w.FillWithPosition<std::uint32_t>("tex2_path_offset", tex_scope);
                write_string(w, tex.path, ue);
                w.FillWithPosition<std::uint32_t>("tex2_type_offset", tex_scope);
                write_string(w, tex.texture_type.value_or(""), ue);
            }
        }

        // --- Bone names (pad 16) ---
        w.PadAlign(16);
        for (const auto& bone : bones)
        {
            w.FillWithPosition<std::uint32_t>("bone_name_offset", &bone);
            write_string(w, bone.name, ue);
        }

        // --- Version-specific alignment and vertex data ---
        auto alignment = (static_cast<std::uint32_t>(version) <= 0x2000E) ? 32u : 16u;
        w.PadAlign(alignment);
        if (version == FLVERVersion::DarkSouls2_NT || version == FLVERVersion::DarkSouls2)
            w.WritePad(32);

        auto vertex_data_start = w.Position();
        w.Fill<std::uint32_t>("vertex_data_offset", static_cast<std::uint32_t>(vertex_data_start));

        std::size_t fs_idx = 0;
        for (const auto& mesh : meshes)
        {
            // Face set index data.
            for (const auto& fs : mesh.face_sets)
            {
                const void* fs_scope = &fs;
                w.PadAlign(16);
                auto vi_offset = w.Position() - vertex_data_start;
                w.Fill<std::uint32_t>("fs2_vi_offset", static_cast<std::uint32_t>(vi_offset), fs_scope);

                auto fs_bit_size = fs_index_sizes[fs_idx++];
                if (fs_bit_size == 16)
                {
                    for (auto idx : fs.vertex_indices)
                        w.Write<std::uint16_t>(static_cast<std::uint16_t>(idx));
                }
                else
                {
                    for (auto idx : fs.vertex_indices)
                        w.Write<std::uint32_t>(idx);
                }
            }

            // Vertex array data.
            for (const auto& va : mesh.vertex_arrays)
            {
                const void* va_scope = &va;
                w.PadAlign(16);
                auto arr_offset = w.Position() - vertex_data_start;
                w.Fill<std::uint32_t>("va2_offset", static_cast<std::uint32_t>(arr_offset), va_scope);

                auto compressed = CompressVertexArray(va, uv_factor);
                w.WriteRaw(compressed.data(), compressed.size());
            }
        }

        w.PadAlign(16);
        auto vertex_data_end = w.Position();
        w.Fill<std::uint32_t>("vertex_data_size", static_cast<std::uint32_t>(vertex_data_end - vertex_data_start));

        if (version == FLVERVersion::DarkSouls2_NT || version == FLVERVersion::DarkSouls2)
            w.WritePad(32);

        return w.Finalize();
    }

} // namespace Firelink
