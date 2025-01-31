#include <string>
#include <vector>

#include "Firelink/BaseHook.h"
#include "Firelink/MemoryUtils.h"
#include "Firelink/Pointer.h"
#include "Firelink/Process.h"

#include "FirelinkDSR/DSRHook.h"
#include "FirelinkDSR/DSROffsets.h"

using namespace std;
using namespace Firelink;


namespace
{
    constexpr char worldChrManAob[] = "48 8B 05 ? ? ? ? 48 8B 48 68 48 85 C9 0F 84 ? ? ? ? 48 39 5E 10 0F 84 ? ? ? ? 48";
    constexpr char chrClassBaseAob[] = "48 8B 05 ? ? ? ? 48 85 C0 ? ? F3 0F 58 80 AC 00 00 00";
    constexpr char chrClassWarpAob[] = "48 8B 05 ? ? ? ? 66 0F 7F 80 ? ? ? ? 0F 28 02 66 0F 7F 80 ? ? ? ? C6 80";
}


FirelinkDSR::DSRHook::DSRHook(shared_ptr<ManagedProcess> process) : BaseHook(std::move(process))
{
    DSRHook::RefreshAllPointers();
}

void FirelinkDSR::DSRHook::RefreshAllPointers()
{
    CreatePointerFromAobWithJump3("WorldChrMan", worldChrManAob);
    CreatePointerFromAobWithJump3("ChrClassBase", chrClassBaseAob);
    // TODO: ChrClassWarp.
    // TODO: More AoBs.

    RefreshChildPointers();
}

void FirelinkDSR::DSRHook::RefreshChildPointers()
{
    m_process->CreateChildPointer("WorldChrMan", "PlayerIns", { world_chr_man_offsets::PLAYER_INS });
    m_process->CreateChildPointer("PlayerIns", "SpEffectManager", { enemy_ins_offsets::SP_EFFECT_MANAGER_ADDR });

    m_process->CreateChildPointer("ChrClassBase", "ChrAsm", { chr_class_base_offsets::CHR_ASM });

}

bool FirelinkDSR::DSRHook::IsGameLoaded() const
{
    // WorldChrMan only resolves to non-null if game is loaded.
    return !(*this)["WorldChrMan"]->IsNull();
}

u16string FirelinkDSR::DSRHook::GetPlayerModelName() const
{
    return (*this)["PlayerIns"]->ReadUTF16String(enemy_ins_offsets::MODEL_NAME, 5);
}

pair<int, int> FirelinkDSR::DSRHook::GetPlayerHp() const
{
    const BasePointer* playerIns = (*this)["PlayerIns"];
    const int currentHP = playerIns->Read<int32_t>(enemy_ins_offsets::CURRENT_HP);
    const int maxHP = playerIns->Read<int32_t>(enemy_ins_offsets::MAX_HP);
    return { currentHP, maxHP };
}

vector<int> FirelinkDSR::DSRHook::GetPlayerActiveSpEffects() const
{
    const BasePointer* spEffectManagerPtr = (*this)["SpEffectManager"];
    if (spEffectManagerPtr->IsNull())
        return {};  // not loaded yet

    // Active SpEffects are implemented in the game engine as a linked list.
    auto nextEntryAddr = spEffectManagerPtr->Read<void*>(0x8);
    if (nextEntryAddr == nullptr)
        return {};  // no SpEffects (extremely unlikely for player!)

    const ManagedProcess& process = spEffectManagerPtr->GetProcess();
    vector<int> spEffectIDs;  // can't know the size in advance
    while (nextEntryAddr != nullptr)
    {
        const void* spEffectIdAddr = GetOffsetPointer(nextEntryAddr, sp_effect_entry_offsets::SP_EFFECT_ID);
        int spEffectId = process.Read<int32_t>(spEffectIdAddr);
        spEffectIDs.push_back(spEffectId);

        // Read next entry address (nullptr for last entry).
        const void* nextEntryPtrAddr = GetOffsetPointer(nextEntryAddr, sp_effect_entry_offsets::NEXT_ENTRY_PTR);
        nextEntryAddr = process.Read<void*>(nextEntryPtrAddr);
    }
    return spEffectIDs;
}

bool FirelinkDSR::DSRHook::PlayerHasSpEffect(const int spEffectId) const
{
    vector<int> playerSpEffects = GetPlayerActiveSpEffects();
    return ranges::find(playerSpEffects.begin(), playerSpEffects.end(), spEffectId) != playerSpEffects.end();
}

// --- EQUIPPED WEAPONS ---

int FirelinkDSR::DSRHook::GetWeapon(WeaponSlot slot, const bool isLeftHand) const
{
    const BasePointer* chrAsm = (*this)["ChrAsm"];
    if (slot == WeaponSlot::CURRENT)
        // Read player's current equipped slot from game.
        slot = chrAsm->Read<WeaponSlot>(
            isLeftHand ? chr_asm_offsets::CURRENT_LEFT_WEAPON_SLOT : chr_asm_offsets::CURRENT_RIGHT_WEAPON_SLOT);

    switch (slot)
    {
        case WeaponSlot::PRIMARY:
            return chrAsm->Read<int32_t>(
                isLeftHand ? chr_asm_offsets::PRIMARY_LEFT_WEAPON : chr_asm_offsets::PRIMARY_RIGHT_WEAPON);
        case WeaponSlot::SECONDARY:
            return chrAsm->Read<int32_t>(
                isLeftHand ? chr_asm_offsets::SECONDARY_LEFT_WEAPON : chr_asm_offsets::SECONDARY_RIGHT_WEAPON);
        case WeaponSlot::CURRENT:
            return -1;  // shouldn't be reachable
    }

    return -1;
}

bool FirelinkDSR::DSRHook::SetWeapon(WeaponSlot slot, const int weaponId, const bool isLeftHand) const
{
    const BasePointer* chrAsm = (*this)["ChrAsm"];
    if (slot == WeaponSlot::CURRENT)
        // Read player's current equipped slot from game.
            slot = chrAsm->Read<WeaponSlot>(
            isLeftHand ? chr_asm_offsets::CURRENT_LEFT_WEAPON_SLOT : chr_asm_offsets::CURRENT_RIGHT_WEAPON_SLOT);

    switch (slot)
    {
    case WeaponSlot::PRIMARY:
        return chrAsm->Write<int32_t>(
            isLeftHand ? chr_asm_offsets::PRIMARY_LEFT_WEAPON : chr_asm_offsets::PRIMARY_RIGHT_WEAPON, weaponId);
    case WeaponSlot::SECONDARY:
        return chrAsm->Write<int32_t>(
            isLeftHand ? chr_asm_offsets::SECONDARY_LEFT_WEAPON : chr_asm_offsets::SECONDARY_RIGHT_WEAPON, weaponId);
    case WeaponSlot::CURRENT:
        return false;
    }

    return false;
}
