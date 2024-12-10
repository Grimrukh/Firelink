#pragma once

#include <vector>
#include <string>

#include "GrimHook/Pointer.h"
#include "GrimHook/Process.h"

#include "DSREnums.h"


// ReSharper disable once CppInconsistentNaming
class __declspec(dllexport) DSRHook
{
    GrimHook::ManagedProcess* m_process = nullptr;

    GrimHook::BasePointer* m_worldChrMan = nullptr;
    GrimHook::BasePointer* m_chrClassBase = nullptr;

    public:
        explicit DSRHook(GrimHook::ManagedProcess* process);

        // RULE OF FIVE
        ~DSRHook();
        DSRHook(const DSRHook&) = delete;
        DSRHook& operator=(const DSRHook&) = delete;
        DSRHook(DSRHook&&) noexcept;
        DSRHook& operator=(DSRHook&&) noexcept;

        // Static strings and AOBs
        static inline const std::wstring processName = L"DarkSoulsRemastered.exe";
        static inline const std::wstring windowTitle = L"DARK SOULS™: REMASTERED";
        static inline const std::wstring worldChrManAob = L"48 8B 05 ? ? ? ? 48 8B 48 68 48 85 C9 0F 84 ? ? ? ? 48 39 5E 10 0F 84 ? ? ? ? 48";
        static inline const std::wstring chrClassBaseAob = L"48 8B 05 ? ? ? ? 48 85 C0 ? ? F3 0F 58 80 AC 00 00 00";
        static inline const std::wstring chrClassWarpAob = L"48 8B 05 ? ? ? ? 66 0F 7F 80 ? ? ? ? 0F 28 02 66 0F 7F 80 ? ? ? ? C6 80";

        void findAoBs();

        [[nodiscard]] bool isGameLoaded() const;

        [[nodiscard]] std::wstring getPlayerModel() const;

        [[nodiscard]] int* getPlayerHp() const;

        [[nodiscard]] std::vector<int> getPlayerActiveSpEffects() const;

        [[nodiscard]] bool playerHasSpEffect(int spEffectId) const;

        // --- EQUIPPED WEAPONS ---

        [[nodiscard]] int getRightHandWeapon(WeaponSlot slot) const;

        void setRightHandWeapon(WeaponSlot slot, int weaponId) const;

        [[nodiscard]] int getLeftHandWeapon(WeaponSlot slot) const;

        void setLeftHandWeapon(WeaponSlot slot, int weaponId) const;
};
