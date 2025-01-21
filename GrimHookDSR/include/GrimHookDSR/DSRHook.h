#pragma once

#include <vector>
#include <string>

#include "GrimHook/BaseHook.h"
#include "GrimHook/Pointer.h"
#include "GrimHook/Process.h"

#include "Export.h"
#include "DSREnums.h"


namespace DSRHook
{
    /// @brief Name of DSR process, for the user to find it.
    inline constexpr wchar_t DSR_PROCESS_NAME[] = L"DarkSoulsRemastered.exe";
    /// @brief Name of DSR window title, for the user to find it.
    inline constexpr wchar_t DSR_WINDOW_TITLE[] = L"DARK SOULS™: REMASTERED";

    class GRIMHOOKDSR_API DSRHook final : public GrimHook::BaseHook
    {
    public:
        /// @brief Construct a DSRHook using a shared pointer to a `ManagedProcess`.
        explicit DSRHook(std::shared_ptr<GrimHook::ManagedProcess> process);

        /// @brief Refresh all pointers, including base AoB pointers like "WorldChrMan".
        void RefreshAllPointers() override;

        /// @brief Refresh child pointers (should be faster).
        void RefreshChildPointers() override;

        /// @brief Check if the game is loaded by checking if the WorldChrMan pointer is valid.
        [[nodiscard]] bool IsGameLoaded() const;

        /// @brief Get the player model name from the game. (Should always be "c0000".)
        [[nodiscard]] std::u16string GetPlayerModelName() const;

        /// @brief Get the player's current and maximum HP in-game.
        [[nodiscard]] std::pair<int, int> GetPlayerHp() const;

        /// @brief Get a vector of the player's active SpEffects in-game.
        [[nodiscard]] std::vector<int> GetPlayerActiveSpEffects() const;

        /// @brief Check if player currently has a specific SpEffect active.
        [[nodiscard]] bool PlayerHasSpEffect(int spEffectId) const;

        // --- EQUIPPED WEAPONS ---

        /// @brief Get the player's weapon in the given `slot` and given hand.
        [[nodiscard]] int GetWeapon(WeaponSlot slot, bool isLeftHand = false) const;

        /// @brief Set the player's weapon in the given `slot` and given hand.
        [[nodiscard]] bool SetWeapon(WeaponSlot slot, int weaponId, bool isLeftHand = false) const;

        /// @brief Convenient method for getting the player's right-hand weapon in the given `slot`.
        [[nodiscard]] int GetRightHandWeapon(const WeaponSlot slot) const
        {
            return GetWeapon(slot, false);
        }
        /// @brief Convenient method for setting the player's right-hand weapon in the given `slot`.
        [[nodiscard]] bool SetRightHandWeapon(const WeaponSlot slot, const int weaponId) const
        {
            return SetWeapon(slot, weaponId, false);
        }

        /// @brief Convenient method for getting the player's left-hand weapon in the given `slot`.
        [[nodiscard]] int GetLeftHandWeapon(const WeaponSlot slot) const
        {
            return GetWeapon(slot, true);
        }
        /// @brief Convenient method for setting the player's left-hand weapon in the given `slot`.
        [[nodiscard]] bool SetLeftHandWeapon(const WeaponSlot slot, const int weaponId) const
        {
            return SetWeapon(slot, weaponId, true);
        }
};

}
