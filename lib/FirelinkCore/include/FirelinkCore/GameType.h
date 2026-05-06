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
}
