#pragma once

// Enum for the `CURRENT_*_SLOT` values in ChrAsm struct, plus
// a CURRENT value that can be passed to ChrAsm getters that
// tell them to check the appropriate `CURRENT_*_SLOT` value.
enum WeaponSlot
{
    CURRENT = -1,
    PRIMARY = 0,
    SECONDARY = 1,
};