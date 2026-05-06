#include "../include/FirelinkFLVER/Version.h"

namespace Firelink
{
    const char* version_name(FLVERVersion v) noexcept
    {
        switch (v)
        {
        case FLVERVersion::Null: return "Null";
        case FLVERVersion::DemonsSouls_0x0F: return "DemonsSouls_0x0F";
        case FLVERVersion::DemonsSouls_0x10: return "DemonsSouls_0x10";
        case FLVERVersion::DemonsSouls_0x14: return "DemonsSouls_0x14";
        case FLVERVersion::DemonsSouls: return "DemonsSouls";
        case FLVERVersion::DarkSouls2_Armor9320: return "DarkSouls2_Armor9320";
        case FLVERVersion::DarkSouls_PS3_o0700_o0701: return "DarkSouls_PS3_o0700_o0701";
        case FLVERVersion::DarkSouls_A: return "DarkSouls_A";
        case FLVERVersion::DarkSouls_B: return "DarkSouls_B";
        case FLVERVersion::DarkSouls2_NT: return "DarkSouls2_NT";
        case FLVERVersion::DarkSouls2: return "DarkSouls2";
        case FLVERVersion::Bloodborne_DS3_A: return "Bloodborne_DS3_A";
        case FLVERVersion::Bloodborne_DS3_B: return "Bloodborne_DS3_B";
        case FLVERVersion::Sekiro_TestChr: return "Sekiro_TestChr";
        case FLVERVersion::Sekiro_EldenRing: return "Sekiro_EldenRing";
        case FLVERVersion::ArmoredCore6: return "ArmoredCore6";
        }
        return "<unknown>";
    }
} // namespace Firelink
