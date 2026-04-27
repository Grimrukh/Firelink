#include <FirelinkFLVER/FLVER.h>

#include <FirelinkFLVER/LayoutRepair.h>
#include <FirelinkFLVER/MergedMesh.h>

#include <algorithm>
#include <functional>
#include <unordered_map>

namespace Firelink
{
    using BinaryReadWrite::BufferReader;
    using BinaryReadWrite::BufferWriter;
    using BinaryReadWrite::Endian;

    namespace
    {
        // Read a FLVER2 vertex array header (does NOT read the vertex data itself).
        struct VertexArrayHeader
        {
            std::uint32_t array_index = 0;
            std::uint32_t layout_index = 0;
            std::uint32_t vertex_size = 0;
            std::uint32_t vertex_count = 0;
            std::uint32_t array_length = 0;
            std::uint32_t array_offset = 0;
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
                    const auto n = static_cast<std::int32_t>(vi.size() / 3);
                    true_fc += n;
                    total_fc += n;
                    continue;
                }
                for (std::size_t i = 0; i + 2 < vi.size(); ++i)
                {
                    const bool has_0xffff = (vi[i] == 0xFFFF || vi[i + 1] == 0xFFFF || vi[i + 2] == 0xFFFF);
                    if (has_0xffff) continue;
                    total_fc++;
                    // True if not MotionBlur and non-degenerate.
                    if (vi[i] != vi[i + 1] && vi[i] != vi[i + 2] && vi[i + 1] != vi[i + 2])
                        true_fc++;
                }
            }
        }

    } // anonymous namespace


    Endian FLVER::GetEndian() const noexcept
    {
        return m_isBigEndian ? Endian::Big : Endian::Little;
    }

    void FLVER::Deserialize(BufferReader& r)
    {
        if (r.size() < 12)
            throw FLVERError("Buffer too small to contain a FLVER header");

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
        m_isBigEndian = file_endian == Endian::Big;

        auto version_int = r.Read<std::uint32_t>();
        try
        {
            m_version = static_cast<FLVERVersion>(version_int);
        }
        catch (...)
        {
            throw FLVERError("Unrecognized FLVER version: " + std::to_string(version_int));
        }

        // FLVER0 and FLVER2 diverge after magic, endian tag, and version.

        if (version_int < 0x20000)
        {
            DeserializeFLVER0(r);
            return;
        }
    }

    void FLVER::Serialize(BufferWriter& w) const
    {
        if (is_flver0(m_version))
        {
            SerializeFLVER0(w);
            return;
        }
        return SerializeFLVER2(w);
    }

    MergedMesh FLVER::GetCachedMergedMesh(
        const std::vector<std::uint32_t>& meshMaterialIndices,
        const std::vector<std::vector<std::string>>& materialUVLayerNames,
        bool merge_vertices)
    {
        if (!m_cachedMergedMesh)
            m_cachedMergedMesh = std::make_unique<MergedMesh>(
                *this, meshMaterialIndices, materialUVLayerNames, merge_vertices);
        return *m_cachedMergedMesh;
    }

    void FLVER::ClearCachedMergedMesh()
    {
        m_cachedMergedMesh.reset();
    }

    void FLVER::DeserializeFLVER0(BufferReader& r)
    {
        // FLVER header should have already been read.
        const auto vertex_data_offset = r.Read<std::uint32_t>();
        const auto vertex_data_size = r.Read<std::uint32_t>(); (void)vertex_data_size;
        const auto dummy_count = r.Read<std::uint32_t>();
        const auto material_count = r.Read<std::uint32_t>();
        const auto bone_count = r.Read<std::uint32_t>();
        const auto mesh_count = r.Read<std::uint32_t>();
        const auto vertex_array_count = r.Read<std::uint32_t>(); (void)vertex_array_count;

        m_boundingBox.min.x = r.Read<float>();
        m_boundingBox.min.y = r.Read<float>();
        m_boundingBox.min.z = r.Read<float>();
        m_boundingBox.max.x = r.Read<float>();
        m_boundingBox.max.y = r.Read<float>();
        m_boundingBox.max.z = r.Read<float>();

        m_trueFaceCount = r.Read<std::int32_t>();
        m_totalFaceCount = r.Read<std::int32_t>();

        const auto face_set_vertex_index_bit_size = r.Read<std::uint8_t>();
        m_isUnicode = r.Read<std::uint8_t>() != 0;
        m_f0Unk4a = r.Read<std::uint8_t>();
        m_f0Unk4b = r.Read<std::uint8_t>();
        m_f0Unk4c = r.Read<std::uint32_t>();
        r.AssertPad(12); // no face_set_count, layout_count, texture_count
        m_f0Unk5c = r.Read<std::uint8_t>();
        r.AssertPad(3);
        r.AssertPad(32);
        // Header: 128 bytes total.

        const float uv_factor = m_isBigEndian ? 1024.f : 2048.f;

        // Dummies.
        for (std::uint32_t i = 0; i < dummy_count; ++i)
            m_dummies.push_back(Dummy::Read(r, m_version));

        // Materials (with layouts stored inside).
        std::vector<FLVER0MaterialRead> mat_reads;
        for (std::uint32_t i = 0; i < material_count; ++i)
            mat_reads.push_back(FLVER0MaterialRead::Read(r, m_isUnicode));

        // Bones.
        for (std::uint32_t i = 0; i < bone_count; ++i)
            m_bones.push_back(Bone::Read(r, m_isUnicode));

        // Meshes.
        for (std::uint32_t i = 0; i < mesh_count; ++i)
        {
            m_meshes.push_back(Mesh::ReadFLVER0(
                r, face_set_vertex_index_bit_size, vertex_data_offset,
                mat_reads, m_version, uv_factor, static_cast<std::int32_t>(i)));
        }
    }

    void FLVER::DeserializeFLVER2(BufferReader& r)
    {
        // FLVER header should have already been read.
        auto vertex_data_offset = r.Read<std::uint32_t>();
        auto vertex_data_size = r.Read<std::uint32_t>();
        (void)vertex_data_size;

        const auto dummy_count = r.Read<std::uint32_t>();
        const auto material_count = r.Read<std::uint32_t>();
        const auto bone_count = r.Read<std::uint32_t>();
        const auto mesh_count = r.Read<std::uint32_t>();
        const auto vertex_array_count = r.Read<std::uint32_t>();

        m_boundingBox.min.x = r.Read<float>();
        m_boundingBox.min.y = r.Read<float>();
        m_boundingBox.min.z = r.Read<float>();
        m_boundingBox.max.x = r.Read<float>();
        m_boundingBox.max.y = r.Read<float>();
        m_boundingBox.max.z = r.Read<float>();

        m_trueFaceCount = r.Read<std::int32_t>();
        m_totalFaceCount = r.Read<std::int32_t>();

        const auto face_set_vertex_index_bit_size = r.Read<std::uint8_t>();
        m_isUnicode = r.Read<std::uint8_t>() != 0;
        m_f2Unk4a = r.Read<std::uint8_t>() != 0;
        r.AssertPad(1);
        m_f2Unk4c = r.Read<std::int32_t>();

        auto face_set_count = r.Read<std::uint32_t>();
        auto layout_count = r.Read<std::uint32_t>();
        auto texture_count = r.Read<std::uint32_t>();

        m_f2Unk5c = static_cast<std::int32_t>(r.Read<std::uint8_t>());
        m_f2Unk5d = static_cast<std::int32_t>(r.Read<std::uint8_t>());
        r.AssertPad(10);
        m_f2Unk68 = r.Read<std::int32_t>();
        r.AssertPad(20);

        // Header should be 128 bytes total.
        // (offset should now be 128 = 0x80)

        float uv_factor = m_version >= FLVERVersion::DarkSouls2_NT ? 2048.f : 1024.f;
        bool bounding_box_has_unknown = m_version == FLVERVersion::Sekiro_EldenRing;

        // --- Read dummies ---
        m_dummies.reserve(dummy_count);
        for (std::uint32_t i = 0; i < dummy_count; ++i)
        {
            m_dummies.push_back(Dummy::Read(r, m_version));
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
            auto mat = Material::ReadFLVER2(r, m_isUnicode, m_version, gx_cache, tc, fti);
            mat_infos.push_back({std::move(mat), tc, fti});
        }

        // --- Read bones ---
        m_bones.reserve(bone_count);
        for (std::uint32_t i = 0; i < bone_count; ++i)
        {
            m_bones.push_back(Bone::Read(r, m_isUnicode));
        }

        // --- Read mesh headers ---
        std::vector<MeshReadState> mesh_states;
        mesh_states.reserve(mesh_count);
        for (std::uint32_t i = 0; i < mesh_count; ++i)
        {
            mesh_states.push_back(MeshReadState::ReadFLVER2(r, bounding_box_has_unknown, static_cast<std::int32_t>(i)));
        }

        // --- Read face sets ---
        std::unordered_map<std::uint32_t, FaceSet> face_sets;
        for (std::uint32_t i = 0; i < face_set_count; ++i)
        {
            face_sets[i] = FaceSet::ReadFLVER2(r, face_set_vertex_index_bit_size, vertex_data_offset);
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
            layouts.push_back(VertexArrayLayout::ReadFLVER2(r));
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
            vertex_arrays[i] = VertexArray::FromCompressedData(raw, hdr.vertex_count, layout, uv_factor);
        }

        // --- Read textures ---
        std::unordered_map<std::uint32_t, Texture> textures;
        for (std::uint32_t i = 0; i < texture_count; ++i)
        {
            textures[i] = Texture::ReadFLVER2(r, m_isUnicode);
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
        m_meshes.reserve(mesh_count);
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

            m_meshes.push_back(std::move(ms.mesh));
        }
    }

    void FLVER::SerializeFLVER0(BufferWriter& w) const
    {
        bool ue = m_isUnicode;
        float uv_factor = m_isBigEndian ? 1024.f : 2048.f;

        // Compute face counts.
        std::int32_t true_fc = 0, total_fc = 0;
        CountFaces0(m_meshes, true_fc, total_fc);

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

        for (const auto& mesh : m_meshes)
        {
            auto mh = mesh.material.GetHash();
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
            auto lh = layout.GetHash();
            std::size_t layout_idx = 0;
            bool found = false;
            for (std::size_t li = 0; li < material_layouts[mat_idx].size(); ++li)
            {
                if (material_layouts[mat_idx][li].GetHash() == lh)
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
        if (m_isBigEndian) { w.WriteRaw("B", 1); w.WritePad(1); }
        else { w.WriteRaw("L", 1); w.WritePad(1); }
        w.Write<std::uint32_t>(static_cast<std::uint32_t>(m_version));
        w.Reserve<std::uint32_t>("vertex_data_offset");
        w.Reserve<std::uint32_t>("vertex_data_size");
        w.Write<std::uint32_t>(static_cast<std::uint32_t>(m_dummies.size()));
        w.Write<std::uint32_t>(mat_count);
        w.Write<std::uint32_t>(static_cast<std::uint32_t>(m_bones.size()));
        w.Write<std::uint32_t>(static_cast<std::uint32_t>(m_meshes.size()));
        w.Write<std::uint32_t>(static_cast<std::uint32_t>(m_meshes.size())); // vertex_array_count = mesh_count
        w.Write<float>(m_boundingBox.min.x);
        w.Write<float>(m_boundingBox.min.y);
        w.Write<float>(m_boundingBox.min.z);
        w.Write<float>(m_boundingBox.max.x);
        w.Write<float>(m_boundingBox.max.y);
        w.Write<float>(m_boundingBox.max.z);
        w.Write<std::int32_t>(true_fc);
        w.Write<std::int32_t>(total_fc);
        w.Write<std::uint8_t>(16); // vertex_index_bit_size
        w.Write<std::uint8_t>(m_isUnicode ? 1 : 0);
        w.Write<std::uint8_t>(m_f0Unk4a);
        w.Write<std::uint8_t>(m_f0Unk4b);
        w.Write<std::uint32_t>(m_f0Unk4c);
        w.WritePad(12); // no face_set_count, layout_count, texture_count
        w.Write<std::uint8_t>(m_f0Unk5c);
        w.WritePad(3);
        w.WritePad(32);
        // Header: 128 bytes.

        // --- Dummies ---
        for (const auto& d : m_dummies)
            d.Write(w, m_version);

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
        for (const auto& bone : m_bones)
            bone.Write(w, &bone);

        // --- Mesh headers (100 bytes each) ---
        for (std::size_t mi = 0; mi < m_meshes.size(); ++mi)
        {
            const auto& mesh = m_meshes[mi];
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
            auto write_vert_size = mesh.vertex_arrays[0].layout.GetCompressedVertexSize();
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
            w.WriteString(mat->name, ue);

            // Mat def path.
            w.FillWithPosition<std::uint32_t>("mat_def_offset", scope);
            w.WriteString(mat->mat_def_path, ue);

            // Textures.
            w.FillWithPosition<std::uint32_t>("mat_tex_offset", scope);
            w.Write<std::uint8_t>(static_cast<std::uint8_t>(mat->textures.size()));
            w.WritePad(3);
            w.WritePad(12);

            // Texture headers.
            for (const auto& texture : mat->textures)
            {
                const void* tex_scope = &texture;
                w.Reserve<std::uint32_t>("tex_path_offset", tex_scope);
                w.Reserve<std::uint32_t>("tex_type_offset", tex_scope);
                w.WritePad(8);
            }

            // Texture strings.
            for (const auto& tex : mat->textures)
            {
                const void* tex_scope = &tex;
                w.FillWithPosition<std::uint32_t>("tex_path_offset", tex_scope);
                w.WriteString(tex.path, ue);
                if (tex.texture_type.has_value())
                {
                    w.FillWithPosition<std::uint32_t>("tex_type_offset", tex_scope);
                    w.WriteString(tex.texture_type.value(), ue);
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
        for (const auto& bone : m_bones)
        {
            w.FillWithPosition<std::uint32_t>("bone_name_offset", &bone);
            w.WriteString(bone.name, ue);
        }

        // --- Mesh vertex array headers ---
        for (std::size_t mi = 0; mi < m_meshes.size(); ++mi)
        {
            const auto& mesh = m_meshes[mi];
            const void* scope = &mesh;

            w.FillWithPosition<std::uint32_t>("mesh_va_hdr_offset_1", scope);

            // Array list header: count(4), offset(4), pad(8).
            w.Write<std::int32_t>(1); // one array
            w.Write<std::int32_t>(static_cast<std::int32_t>(w.Position() + 12)); // offset to header data
            w.WritePad(8);

            // Single vertex array header (16 bytes).
            auto layout_idx = static_cast<std::uint32_t>(mesh_pack_info[mi].layout_index);
            auto write_size = mesh.vertex_arrays[0].layout.GetCompressedVertexSize();
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

        for (const auto& mesh : m_meshes)
        {
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

            auto compressed = mesh.vertex_arrays[0].Compress(uv_factor);
            w.WriteRaw(compressed.data(), compressed.size());
            w.PadAlign(32);
        }

        auto vertex_data_end = w.Position();
        w.Fill<std::uint32_t>("vertex_data_size", static_cast<std::uint32_t>(vertex_data_end - vertex_data_start));
    }

    // ---------------------------------------------------------------------------
    // FLVER2 writer
    // ---------------------------------------------------------------------------

    void FLVER::SerializeFLVER2(BufferWriter& w) const
    {
        bool ue = m_isUnicode;
        float uv_factor = m_version >= FLVERVersion::DarkSouls2_NT ? 2048.f : 1024.f;

        // --- Pre-compute face counts and header index size ---
        std::int32_t true_fc = 0, total_fc = 0;
        std::uint8_t header_vi_bit_size = 0;
        if (m_version < FLVERVersion::Bloodborne_DS3_A)
        {
            header_vi_bit_size = 16;
        }

        for (const auto& mesh : m_meshes)
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

        for (const auto& mesh : m_meshes)
        {
            auto mh = mesh.material.GetHash();
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
                    auto gh = GXItem::GetListHash(mesh.material.gx_items);
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
                auto lh = va.layout.GetHash();
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
        for (const auto& mesh : m_meshes)
        {
            total_va_count += static_cast<std::uint32_t>(mesh.vertex_arrays.size());
            total_fs_count += static_cast<std::uint32_t>(mesh.face_sets.size());
        }

        // --- Write FLVER2 header (128 bytes) ---
        w.WriteRaw("FLVER", 5);
        w.WritePad(1);
        if (m_isBigEndian) { w.WriteRaw("B", 1); w.WritePad(1); }
        else { w.WriteRaw("L", 1); w.WritePad(1); }
        w.Write<std::uint32_t>(static_cast<std::uint32_t>(m_version));
        w.Reserve<std::uint32_t>("vertex_data_offset");
        w.Reserve<std::uint32_t>("vertex_data_size");
        w.Write<std::uint32_t>(static_cast<std::uint32_t>(m_dummies.size()));
        w.Write<std::uint32_t>(packed_mat_count);
        w.Write<std::uint32_t>(static_cast<std::uint32_t>(m_bones.size()));
        w.Write<std::uint32_t>(static_cast<std::uint32_t>(m_meshes.size()));
        w.Write<std::uint32_t>(total_va_count);
        w.Write<float>(m_boundingBox.min.x);
        w.Write<float>(m_boundingBox.min.y);
        w.Write<float>(m_boundingBox.min.z);
        w.Write<float>(m_boundingBox.max.x);
        w.Write<float>(m_boundingBox.max.y);
        w.Write<float>(m_boundingBox.max.z);
        w.Write<std::int32_t>(true_fc);
        w.Write<std::int32_t>(total_fc);
        w.Write<std::uint8_t>(header_vi_bit_size);
        w.Write<std::uint8_t>(m_isUnicode ? 1 : 0);
        w.Write<std::uint8_t>(m_f2Unk4a ? 1 : 0);
        w.WritePad(1);
        w.Write<std::int32_t>(m_f2Unk4c);
        w.Write<std::uint32_t>(total_fs_count);
        w.Write<std::uint32_t>(packed_layout_count);
        w.Write<std::uint32_t>(packed_texture_count);
        w.Write<std::uint8_t>(static_cast<std::uint8_t>(m_f2Unk5c));
        w.Write<std::uint8_t>(static_cast<std::uint8_t>(m_f2Unk5d));
        w.WritePad(10);
        w.Write<std::int32_t>(m_f2Unk68);
        w.WritePad(20);

        // --- Dummies ---
        for (const auto& d : m_dummies)
            d.Write(w, m_version);

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
        for (const auto& bone : m_bones)
            bone.Write(w, &bone);

        // --- Mesh headers (44 bytes each) ---
        for (std::size_t mi = 0; mi < m_meshes.size(); ++mi)
        {
            const auto& mesh = m_meshes[mi];
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
        for (const auto& mesh : m_meshes)
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
        for (std::size_t mi = 0; mi < m_meshes.size(); ++mi)
        {
            for (std::size_t ai = 0; ai < m_meshes[mi].vertex_arrays.size(); ++ai)
            {
                const auto& va = m_meshes[mi].vertex_arrays[ai];
                const void* va_scope = &va;
                auto layout_idx = static_cast<std::uint32_t>(mesh_layout_indices[mi][ai]);
                auto write_size = va.layout.GetCompressedVertexSize();
                bool write_length = (m_version >= static_cast<FLVERVersion>(0x20005));

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
        for (const auto& mesh : m_meshes)
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
        for (const auto& mesh : m_meshes)
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
            for (const auto& mesh : m_meshes)
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
            for (const auto& mesh : m_meshes)
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
                && m_version != FLVERVersion::DarkSouls2)
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
            w.WriteString(mat->name, ue);
            w.FillWithPosition<std::uint32_t>("mat2_def_offset", scope);
            w.WriteString(mat->mat_def_path, ue);

            for (const auto& tex : mat->textures)
            {
                const void* tex_scope = &tex;
                w.FillWithPosition<std::uint32_t>("tex2_path_offset", tex_scope);
                w.WriteString(tex.path, ue);
                w.FillWithPosition<std::uint32_t>("tex2_type_offset", tex_scope);
                w.WriteString(tex.texture_type.value_or(""), ue);
            }
        }

        // --- Bone names (pad 16) ---
        w.PadAlign(16);
        for (const auto& bone : m_bones)
        {
            w.FillWithPosition<std::uint32_t>("bone_name_offset", &bone);
            w.WriteString(bone.name, ue);
        }

        // --- Version-specific alignment and vertex data ---
        auto alignment = (static_cast<std::uint32_t>(m_version) <= 0x2000E) ? 32u : 16u;
        w.PadAlign(alignment);
        if (m_version == FLVERVersion::DarkSouls2_NT || m_version == FLVERVersion::DarkSouls2)
            w.WritePad(32);

        auto vertex_data_start = w.Position();
        w.Fill<std::uint32_t>("vertex_data_offset", static_cast<std::uint32_t>(vertex_data_start));

        std::size_t fs_idx = 0;
        for (const auto& mesh : m_meshes)
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

                auto compressed = va.Compress(uv_factor);
                w.WriteRaw(compressed.data(), compressed.size());
            }
        }

        w.PadAlign(16);
        auto vertex_data_end = w.Position();
        w.Fill<std::uint32_t>("vertex_data_size", static_cast<std::uint32_t>(vertex_data_end - vertex_data_start));

        if (m_version == FLVERVersion::DarkSouls2_NT || m_version == FLVERVersion::DarkSouls2)
            w.WritePad(32);
    }

} // namespace Firelink
