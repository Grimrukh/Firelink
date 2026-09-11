#pragma once

#include <cstdint>

namespace Firelink
{
    //! @brief Game type enum for supported FromSoftware games.
    //! @details Must be kept in sync with `soulstruct` enum.
    enum class GameType : std::uint8_t
    {
        DemonsSouls = 0, // PS5 Remake will never be supported
        DarkSoulsPTDE = 1,
        DarkSoulsDSR = 2,
        DarkSouls2 = 3, // not really supported
        DarkSouls2SOTFS = 4, // not really supported
        Bloodborne = 5,
        DarkSouls3 = 6, // barely supported
        Sekiro = 7, // barely supported
        EldenRing = 8,
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
