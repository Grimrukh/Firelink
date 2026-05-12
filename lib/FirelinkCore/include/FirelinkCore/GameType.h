#pragma once

#include <cstdint>

namespace Firelink
{
    enum class GameType : std::uint8_t
    {
        DemonsSouls = 0,
        DarkSoulsPTDE = 1,
        DarkSoulsDSR = 2,
        Bloodborne = 3,
        DarkSouls3 = 4,
        Sekiro = 5,
        EldenRing = 6,
    };

    ///! @brief Returns true if `game` generally uses DCX for Binder compression.
    inline bool UsesBinderDcx(const GameType game)
    {
        switch (game)
        {
            case GameType::DemonsSouls:
            case GameType::DarkSoulsPTDE:
                return false;
            default:
                return true;
        }
    }

    ///! @brief Returns true if `game` uses DCX for MSB compression.
    inline bool UsesMsbDcx(const GameType game)
    {
        switch (game)
        {
            case GameType::EldenRing:
                return true;
            default:
                return false;
        }
    }
}
