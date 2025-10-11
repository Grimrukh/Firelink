#pragma once

#include <Firelink/BaseHook.h>
#include <Firelink/Pointer.h>
#include <Firelink/Process.h>

#include <FirelinkERHook/Export.h>

namespace FirelinkER
{
    /// @brief Name of ER process, for the user to find it.
    inline constexpr wchar_t ER_PROCESS_NAME[] = L"ELDEN_RING.exe";
    /// @brief Name of ER window title, for the user to find it.
    inline constexpr wchar_t ER_WINDOW_TITLE[] = L"ELDEN RING™";

    class FIRELINK_ER_HOOK_API ERHook final : public Firelink::BaseHook
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

        /// @brief Get current NG+ level.
        [[nodiscard]] uint32_t GetNGPlusLevel() const;

        /// @brief Set current NG+ level.
        void SetNGPlusLevel(uint32_t level) const;

    protected:
        const Firelink::BasePointer* m_WorldChrMan = nullptr;
        const Firelink::BasePointer* m_GameDataMan = nullptr;
        const Firelink::BasePointer* m_SoloParamRepository = nullptr;
        const Firelink::BasePointer* m_EventFlagMan = nullptr;

        const Firelink::BasePointer* m_PlayerIns = nullptr;
    };

} // namespace FirelinkER
