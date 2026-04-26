#pragma once

#include <FirelinkFLVER/Version.h>

#include <FirelinkCore/Collections.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Firelink
{
    class VertexArrayLayout;

    namespace BinaryReadWrite
    {
        class BufferReader;
    }

    struct Texture
    {
        std::string path;
        // Optional in FLVER0 (some textures have no type string); always present in FLVER2.
        std::optional<std::string> texture_type;
        Vector2 scale{1.f, 1.f};

        // FLVER2-only extras. Defaulted so FLVER0 instances don't care.
        std::uint8_t f2_unk_x10 = 0;
        bool f2_unk_x11 = false;
        float f2_unk_x14 = 0.f;
        float f2_unk_x18 = 0.f;
        float f2_unk_x1c = 0.f;

        bool operator==(const Texture&) const = default;

        /// @brief Read a FLVER0 texture.
        static Texture ReadFLVER0(BinaryReadWrite::BufferReader& r, bool unicode_encoding);

        /// @brief Read a FLVER2 texture.
        static Texture ReadFLVER2(BinaryReadWrite::BufferReader& r, bool unicode_encoding);
    };

    struct GXItem
    {
        // FromSoftware uses a 4-byte ASCII tag for the category.
        std::array<char, 4> category{};
        std::int32_t index = 0;
        std::vector<std::byte> data; // variable-length payload (size excludes the 12-byte header)

        // A GXItem is a 'terminator' if its category is one of the known sentinels.
        // The final list element in FLVER2 (post-DS2) is always a terminator.
        [[nodiscard]] bool IsTerminator() const noexcept
        {
            constexpr std::array term1 = {'\xff', '\xff', '\xff', '\x7f'};
            constexpr std::array term2 = {'\xff', '\xff', '\xff', '\xff'};
            return category == term1 || category == term2;
        }

        bool operator==(const GXItem&) const = default;

        /// @brief Hash a GXItem list for deduplication.
        static std::size_t GetListHash(const std::vector<GXItem>& items);
    };

    struct Material
    {
        std::string name;
        std::string mat_def_path; // .mtd / .matbin path
        std::int32_t flags = 0;
        std::int32_t f2_unk_x18 = 0;

        // Textures owned by the material (denormalised from FLVER2's global texture list).
        std::vector<Texture> textures;

        // GXItems owned by the material. Empty means "no GX items" (pre-DS2).
        // Note: lists may be shared between materials at read time via
        // shallow-copy semantics in Python; we deep-copy here. Deduplication on
        // write is the writer's responsibility.
        std::vector<GXItem> gx_items;

        bool operator==(const Material&) const = default;

        /// @brief Simple hash combining for material deduplication.
        [[nodiscard]] std::size_t GetHash() const;

        static Material ReadFLVER2(
            BinaryReadWrite::BufferReader& r,
            bool unicode_encoding,
            FLVERVersion version,
            std::unordered_map<std::uint32_t, std::vector<GXItem>>& gx_item_lists_cache,
            std::uint32_t& out_texture_count,
            std::uint32_t& out_first_texture_index);
    };

    /// @brief Temporary struct used while handing FLVER0 materials to meshes.
    struct FLVER0MaterialRead
    {
        Material mat;
        std::vector<VertexArrayLayout> layouts;

        /// @brief Read a FLVER0 material header and its data (textures, layouts).
        static FLVER0MaterialRead Read(BinaryReadWrite::BufferReader& r, bool unicode_encoding);
    };

} // namespace Firelink
