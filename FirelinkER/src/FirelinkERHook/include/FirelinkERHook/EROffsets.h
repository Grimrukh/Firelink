#pragma once

namespace FirelinkER
{
    namespace WORLD_CHR_MAN
    {
        // Offset in `WorldChrMan` of pointer to player's `PlayerIns`. Accurate as of Elden Ring v1.16.
        inline constexpr int PLAYER_INS = 0x10EF8; // void* -> PlayerIns
    } // namespace WORLD_CHR_MAN

    namespace GAME_DATA_MAN
    {
        inline constexpr int NG_PLUS_LEVEL = 0x120; // uint32_t
    }
}