#pragma once


namespace world_chr_man_offsets
{
    // Offset in `WorldChrMan` of pointer to player's `EnemyIns`. Accurate as of Elden Ring v1.16.
    inline constexpr int PLAYER_INS = 0x10EF8;  // void* -> EnemyIns
}


namespace game_data_man_offsets
{
    inline constexpr int NG_PLUS_LEVEL = 0x120;  // uint32_t
}