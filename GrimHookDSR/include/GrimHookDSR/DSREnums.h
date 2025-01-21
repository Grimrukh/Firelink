#pragma once

/**
 * @brief Enum for the weapon slots in the player's current equipment set.
 *
 * `CURRENT` is not a real game value, but is used by hook methods to detect the current active slot from the game.
 */
enum class WeaponSlot
{
    CURRENT = -1,
    PRIMARY = 0,
    SECONDARY = 1,
};