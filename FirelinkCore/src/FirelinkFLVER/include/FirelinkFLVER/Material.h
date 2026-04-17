#pragma once

#include <FirelinkCore/Collections.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Firelink
{
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
    };
} // namespace Firelink
