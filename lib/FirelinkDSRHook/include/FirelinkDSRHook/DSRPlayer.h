#pragma once

#include <FirelinkCore/Pointer.h>
#include <FirelinkDSRHook/DSREnums.h>
#include <FirelinkDSRHook/DSRHook.h>
#include <FirelinkDSRHook/DSROffsets.h>
#include <FirelinkDSRHook/Export.h>

#include <utility>

namespace Firelink::DarkSouls1R
{
    /// @brief Wrapper for `PlayerIns` struct, which could be host or client player.
    class FIRELINK_DSR_HOOK_API DSRPlayer
    {
    public:
        explicit DSRPlayer(DSRHook* hook, const Firelink::BasePointer& playerInsPtr)
        : hook(hook)
        , playerInsPtr(playerInsPtr)
        {
        }

        [[nodiscard]] Firelink::BasePointer GetPlayerGameData() const
        {
            return playerInsPtr.ReadPointer("PlayerGameData", PLAYER_INS::PLAYER_GAME_DATA_PTR);
        }

        /// @brief Get the player model name from the game. (Should always be "c0000".)
        [[nodiscard]] std::u16string GetPlayerName() const;

        /// @brief Get the player's current and maximum HP in-game.
        [[nodiscard]] std::pair<int, int> GetPlayerHp() const;

        /// @brief Get a vector of the player's active SpEffects in-game.
        [[nodiscard]] std::vector<int> GetPlayerActiveSpEffects() const;

        /// @brief Check if player currently has a specific SpEffect active.
        [[nodiscard]] bool HasSpEffect(int spEffectId) const;

        // --- EQUIPPED WEAPONS ---

        /// @brief Get the current equipped weapon slot type (PRIMARY or SECONDARY) for the given hand.
        [[nodiscard]] WeaponSlot GetWeaponSlot(bool isLeftHand = false) const;

        /// @brief Get the player's weapon in the given `slot` and given hand.
        [[nodiscard]] int GetWeapon(WeaponSlot slot, bool isLeftHand = false) const;

        /// @brief Set the player's weapon in the given `slot` and given hand.
        [[nodiscard]] bool SetWeapon(WeaponSlot slot, int weaponId, bool isLeftHand = false) const;

        /// @brief Convenient method for getting the player's right-hand weapon in the given `slot`.
        [[nodiscard]] int GetRightHandWeapon(const WeaponSlot slot) const { return GetWeapon(slot, false); }
        /// @brief Convenient method for setting the player's right-hand weapon in the given `slot`.
        [[nodiscard]] bool SetRightHandWeapon(const WeaponSlot slot, const int weaponId) const
        {
            return SetWeapon(slot, weaponId, false);
        }

        /// @brief Convenient method for getting the player's left-hand weapon in the given `slot`.
        [[nodiscard]] int GetLeftHandWeapon(const WeaponSlot slot) const { return GetWeapon(slot, true); }
        /// @brief Convenient method for setting the player's left-hand weapon in the given `slot`.
        [[nodiscard]] bool SetLeftHandWeapon(const WeaponSlot slot, const int weaponId) const
        {
            return SetWeapon(slot, weaponId, true);
        }

        // --- EQUIPPED ARMOR ---

        /// @brief Get the player's armor in the given `slot`.
        [[nodiscard]] int GetArmor(ArmorType slot) const;

        /// @brief Set the player's armor in the given `slot`.
        [[nodiscard]] bool SetArmor(ArmorType slot, int armorId) const;

        // --- EQUIPPED RINGS ---

        /// @brief Get the player's ring in the given `slot` (0 or 1).
        [[nodiscard]] int GetRing(int slot) const;

        /// @brief Set the player's ring in the given `slot` (0 or 1).
        [[nodiscard]] bool SetRing(int slot, int ringId) const;

    private:
        DSRHook* hook;
        const Firelink::BasePointer playerInsPtr; // "PlayerIns" or "ClientPlayerIns"
    };

} // namespace Firelink::DarkSouls1R
