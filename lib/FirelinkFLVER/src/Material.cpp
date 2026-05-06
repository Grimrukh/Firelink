#include <FirelinkFLVER/Material.h>

#include <FirelinkFLVER/Encodings.h>
#include <FirelinkFLVER/Mesh.h>
#include <FirelinkFLVER/Version.h>

#include <FirelinkCore/BinaryReadWrite.h>

namespace Firelink
{
    using namespace BinaryReadWrite;

    Texture Texture::ReadFLVER0(BufferReader& r, const bool unicode_encoding)
    {
        Texture tex;
        const auto path_offset = r.Read<std::uint32_t>();
        const auto type_offset = r.Read<std::uint32_t>();
        r.AssertPad(8);

        tex.path = DecodeFLVERString(r.ReadStringAt(path_offset, unicode_encoding), unicode_encoding);
        if (type_offset > 0)
        {
            tex.texture_type = DecodeFLVERString(r.ReadStringAt(type_offset, unicode_encoding), unicode_encoding);
        }
        else
            tex.texture_type = std::nullopt;
        return tex;
    }

    Texture Texture::ReadFLVER2(BufferReader& r, const bool unicode_encoding)
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

        tex.path = DecodeFLVERString(r.ReadStringAt(path_offset, unicode_encoding), unicode_encoding);
        tex.texture_type = DecodeFLVERString(r.ReadStringAt(type_offset, unicode_encoding), unicode_encoding);
        return tex;
    }

    std::size_t GXItem::GetListHash(const std::vector<GXItem>& items)
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

    std::size_t Material::GetHash() const
    {
        std::size_t h = std::hash<std::string>{}(name);
        h ^= std::hash<std::string>{}(mat_def_path) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<int>{}(flags) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<int>{}(f2_unk_x18) + 0x9e3779b9 + (h << 6) + (h >> 2);
        for (const auto& tex : textures)
        {
            h ^= std::hash<std::string>{}(tex.path) + 0x9e3779b9 + (h << 6) + (h >> 2);
            if (tex.texture_type.has_value())
                h ^= std::hash<std::string>{}(tex.texture_type.value()) + 0x9e3779b9 + (h << 6) + (h >> 2);
        }
        return h;
    }

    Material Material::ReadFLVER2(
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

        mat.name = DecodeFLVERString(r.ReadStringAt(name_offset, unicode_encoding), unicode_encoding);
        mat.mat_def_path = DecodeFLVERString(r.ReadStringAt(mat_def_offset, unicode_encoding), unicode_encoding);

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

    FLVER0MaterialRead FLVER0MaterialRead::Read(BufferReader& r, const bool unicode_encoding)
    {
        FLVER0MaterialRead result;

        const auto name_offset = r.Read<std::uint32_t>();
        const auto mat_def_path_offset = r.Read<std::uint32_t>();
        const auto textures_offset = r.Read<std::uint32_t>();
        const auto layouts_offset = r.Read<std::uint32_t>();
        const auto size = r.Read<std::uint32_t>(); (void)size;
        const auto layout_header_offset = r.Read<std::uint32_t>();
        r.AssertPad(8);

        result.mat.name = DecodeFLVERString(r.ReadStringAt(name_offset, unicode_encoding), unicode_encoding);
        result.mat.mat_def_path = DecodeFLVERString(r.ReadStringAt(mat_def_path_offset, unicode_encoding), unicode_encoding);

        // Read textures.
        {
            auto guard = r.TempOffset(textures_offset);
            const auto texture_count = r.Read<std::uint8_t>();
            r.AssertPad(3);
            r.AssertPad(12);
            for (std::uint8_t i = 0; i < texture_count; ++i)
                result.mat.textures.push_back(Texture::ReadFLVER0(r, unicode_encoding));
        }

        // Read layouts.
        if (layout_header_offset != 0)
        {
            auto guard = r.TempOffset(layout_header_offset);
            const auto layout_count = r.Read<std::int32_t>();
            const auto layout_data_offset = r.Read<std::int32_t>(); (void)layout_data_offset;
            r.AssertPad(8);

            // Read layout offset table then layouts.
            std::vector<std::uint32_t> layout_offsets;
            for (std::int32_t i = 0; i < layout_count; ++i)
                layout_offsets.push_back(r.Read<std::uint32_t>());

            for (const auto lo : layout_offsets)
            {
                auto guard2 = r.TempOffset(lo);
                result.layouts.push_back(VertexArrayLayout::ReadFLVER0(r));
            }
        }
        else
        {
            auto guard = r.TempOffset(layouts_offset);
            result.layouts.push_back(VertexArrayLayout::ReadFLVER0(r));
        }

        return result;
    }

}