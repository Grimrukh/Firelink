#pragma once

#include <string>
#include <unordered_map>

namespace Firelink::DarkSouls1R
{
    /*!
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

    inline std::unordered_map<WeaponSlot, std::string> WeaponSlotToString = {
        {WeaponSlot::CURRENT, "Current"},
        {WeaponSlot::PRIMARY, "Primary"},
        {WeaponSlot::SECONDARY, "Secondary"},
    };

    /*!
     * @brief Enum for the armor slots in the player's current equipment set.
     */
    enum class ArmorType
    {
        HEAD = 0,
        BODY = 1,
        ARMS = 2,
        LEGS = 3,
    };

    inline std::unordered_map<ArmorType, std::string> ArmorTypeToString = {
        {ArmorType::HEAD, "Head"},
        {ArmorType::BODY, "Body"},
        {ArmorType::ARMS, "Arms"},
        {ArmorType::LEGS, "Legs"},
    };
}
