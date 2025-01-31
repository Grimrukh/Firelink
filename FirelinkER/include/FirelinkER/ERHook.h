#pragma once

#include <vector>
#include <string>

#include "Firelink/BaseHook.h"
#include "Firelink/Pointer.h"
#include "Firelink/Process.h"

#include "Export.h"


namespace FirelinkER
{
    /// @brief Name of ER process, for the user to find it.
    inline constexpr wchar_t ER_PROCESS_NAME[] = L"ELDEN_RING.exe";
    /// @brief Name of ER window title, for the user to find it.
    inline constexpr wchar_t ER_WINDOW_TITLE[] = L"ELDEN RING™";

    class FIRELINKER_API ERHook final : public Firelink::BaseHook
    {
    public:
        /// @brief Construct a ERHook using a shared pointer to a `ManagedProcess`.
        explicit ERHook(std::shared_ptr<Firelink::ManagedProcess> process);

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

        /// @brief Get current NG+ level.
        [[nodiscard]] uint32_t GetNGPlusLevel() const;

        /// @brief Set current NG+ level.
        void SetNGPlusLevel(uint32_t level) const;
};

}
