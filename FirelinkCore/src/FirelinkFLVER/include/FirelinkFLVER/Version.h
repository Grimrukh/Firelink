// flver/version.hpp
//
// FLVER format version constants — mirrors src/soulstruct/flver/version.py.
// Values MUST match the Python enum; tests assert that on load.

#pragma once

#include "Exceptions.h"

#include <FirelinkFLVER/Export.h>

#include <cstdint>

namespace Firelink
{
    enum class FLVERVersion : std::uint32_t
    {
        Null = 0x00000,

        // FLVER0 (Demon's Souls era).
        DemonsSouls_0x0F = 0x0000F, // e.g. o9993
        DemonsSouls_0x10 = 0x00010, // e.g. c1200
        DemonsSouls_0x14 = 0x00014, // e.g. c7080, m07 map pieces
        DemonsSouls = 0x00015, // "standard" Demon's Souls

        // NOTE: no FLVER1 versions (gap in [0x10000, 0x1FFFF]).

        // FLVER2.
        DarkSouls2_Armor9320 = 0x20009,
        DarkSouls_PS3_o0700_o0701 = 0x2000B,
        DarkSouls_A = 0x2000C,
        DarkSouls_B = 0x2000D,
        DarkSouls2_NT = 0x2000F,
        DarkSouls2 = 0x20010, // includes SOTFS
        Bloodborne_DS3_A = 0x20013,
        Bloodborne_DS3_B = 0x20014,
        Sekiro_TestChr = 0x20016,
        Sekiro_EldenRing = 0x2001A,
        ArmoredCore6 = 0x2001B,
    };

    // --- Version predicates (match FLVERVersion methods in Python) --------------

    constexpr std::uint32_t version_value(FLVERVersion v) noexcept
    {
        return static_cast<std::uint32_t>(v);
    }

    constexpr bool is_flver0(FLVERVersion v) noexcept
    {
        return version_value(v) <= 0x0FFFF;
    }

    constexpr bool is_flver2(FLVERVersion v) noexcept
    {
        return version_value(v) >= 0x20000;
    }

    // Bloodborne+: map piece vertices pack a single rigged-bone index in the 4th
    // byte of the normal_w field instead of having a full bone_indices array.
    constexpr bool map_pieces_use_normal_w_bones(FLVERVersion v) noexcept
    {
        return version_value(v) >= version_value(FLVERVersion::Bloodborne_DS3_A);
    }

    // Sekiro+: Mesh has an additional "unknown" Vector3 after the bounding
    // box max.
    constexpr bool mesh_has_sekiro_bbox_unk(FLVERVersion v) noexcept
    {
        return version_value(v) >= version_value(FLVERVersion::Sekiro_TestChr);
    }

    // UV compression factor used by vertex-array codecs. Big-endian FLVER0 uses
    // 1024 (PS3-era), while DS2+ always uses 2048. The Python source picks
    // between them at layout decompress time.
    constexpr float default_uv_factor(FLVERVersion v, bool big_endian) noexcept
    {
        if (is_flver0(v) && big_endian) return 1024.f;
        return 2048.f;
    }

    // Returns a human-readable name for logging/tests.
    FIRELINK_FLVER_API const char* version_name(FLVERVersion v) noexcept;
} // namespace Firelink
