#pragma once

#include <FirelinkDSRHook/Export.h>

#include <FirelinkCore/BaseHook.h>
#include <FirelinkCore/Process.h>

namespace Firelink::DarkSouls1R
{
    /// @brief Name of DSR process, for the user to find it.
    inline constexpr wchar_t DSR_PROCESS_NAME[] = L"DarkSoulsRemastered.exe";
    /// @brief Name of DSR window title, for the user to find it.
    inline constexpr wchar_t DSR_WINDOW_TITLE[] = L"DARK SOULS™: REMASTERED";

    class FIRELINK_DSR_HOOK_API DSRHook final : public Firelink::BaseHook
    {
    public:
        /// @brief Construct a DSRHook using a shared pointer to a `ManagedProcess`.
        explicit DSRHook(std::shared_ptr<Firelink::ManagedProcess> process);

        /// @brief Refresh all pointers, including base AoB pointers like "WorldChrMan".
        void RefreshAllPointers() override;

        /// @brief Refresh child pointers (should be faster).
        void RefreshChildPointers() override;

        /// @brief Check if the game is loaded by checking if the WorldChrMan pointer is valid.
        [[nodiscard]] bool IsGameLoaded() const;

        /// POINTER GETTERS

        [[nodiscard]] const Firelink::BasePointer* WorldChrMan() const { return m_WorldChrMan; }
        [[nodiscard]] const Firelink::BasePointer* ChrClassBase() const { return m_ChrClassBase; }
        [[nodiscard]] const Firelink::ChildPointer* PlayerIns() const { return m_PlayerIns; }
        [[nodiscard]] const Firelink::ChildPointer* PlayerGameData() const { return m_PlayerGameData; }
        [[nodiscard]] const Firelink::ChildPointer* ClientPlayerIns() const { return m_ClientPlayerIns; }

    protected:
        Firelink::BasePointer* m_WorldChrMan = nullptr;
        Firelink::BasePointer* m_ChrClassBase = nullptr;
        Firelink::ChildPointer* m_PlayerIns = nullptr;
        Firelink::ChildPointer* m_PlayerGameData = nullptr;
        Firelink::ChildPointer* m_ClientPlayerIns = nullptr;
    };

} // namespace Firelink::DarkSouls1R
