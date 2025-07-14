#include "FirelinkDSR/DSRPlayer.h"

#include "Firelink/MemoryUtils.h"
#include "Firelink/Pointer.h"

#include "FirelinkDSR/DSROffsets.h"

using namespace std;
using namespace Firelink;


u16string FirelinkDSR::DSRPlayer::GetPlayerName() const
{
    return playerInsPtr.ReadUTF16String(
        PLAYER_INS::CHR_INS_NO_VTABLE + CHR_INS_NO_VTABLE::CHR_NAME, 0x28);
}

pair<int, int> FirelinkDSR::DSRPlayer::GetPlayerHp() const
{
    const int currentHP = playerInsPtr.Read<int32_t>(
        PLAYER_INS::CHR_INS_NO_VTABLE + CHR_INS_NO_VTABLE::CURRENT_HP);
    const int maxHP = playerInsPtr.Read<int32_t>(
        PLAYER_INS::CHR_INS_NO_VTABLE + CHR_INS_NO_VTABLE::MAX_HP);
    return { currentHP, maxHP };
}

vector<int> FirelinkDSR::DSRPlayer::GetPlayerActiveSpEffects() const
{
    if (playerInsPtr.IsNull())
        return {};

    const BasePointer specialEffectPtr = playerInsPtr.ReadPointer(
        "SpecialEffect",
        PLAYER_INS::CHR_INS_NO_VTABLE + CHR_INS_NO_VTABLE::SPECIAL_EFFECT_PTR);

    // Active SpEffects are implemented in the game engine as a linked list.
    const void* nextEntryNode = specialEffectPtr.Read<void*>(SPECIAL_EFFECT::FIRST_SP_EFFECT_NODE_PTR);
    if (nextEntryNode == nullptr)
        return {};  // no SpEffects (extremely unlikely for player!)

    const std::shared_ptr<ManagedProcess> process = hook->GetProcess();
    vector<int> spEffectIDs;  // can't know the size in advance
    while (nextEntryNode != nullptr)
    {
        const void* spEffectIdOffset = GetOffsetPointer(nextEntryNode, SP_EFFECT_NODE::SP_EFFECT_ID);
        int spEffectId = process->Read<int32_t>(spEffectIdOffset);
        spEffectIDs.push_back(spEffectId);

        // Read next entry address (nullptr for last entry).
        const void* nextNodePtrOffset = GetOffsetPointer(nextEntryNode, SP_EFFECT_NODE::NEXT_ENTRY_PTR);
        nextEntryNode = process->Read<void*>(nextNodePtrOffset);
    }

    Info("Got player active SpEffects.");

    return spEffectIDs;
}

bool FirelinkDSR::DSRPlayer::HasSpEffect(const int spEffectId) const
{
    vector<int> playerSpEffects = GetPlayerActiveSpEffects();
    return ranges::find(playerSpEffects.begin(), playerSpEffects.end(), spEffectId) != playerSpEffects.end();
}

// --- EQUIPPED WEAPONS ---

WeaponSlot FirelinkDSR::DSRPlayer::GetWeaponSlot(const bool isLeftHand) const
{
    const BasePointer playerGameData = GetPlayerGameData();
    if (playerGameData.IsNull())
        return WeaponSlot::CURRENT;  // no game data, return current slot

    return playerGameData.Read<WeaponSlot>(
        isLeftHand ? PLAYER_GAME_DATA::CURRENT_LEFT_WEAPON_SLOT : PLAYER_GAME_DATA::CURRENT_RIGHT_WEAPON_SLOT);
}

int FirelinkDSR::DSRPlayer::GetWeapon(WeaponSlot slot, const bool isLeftHand) const
{
    const BasePointer playerGameData = GetPlayerGameData();
    if (playerGameData.IsNull())
        return -1;

    if (slot == WeaponSlot::CURRENT)
        // Read player's current equipped slot from game.
        slot = playerGameData.Read<WeaponSlot>(
            isLeftHand ? PLAYER_GAME_DATA::CURRENT_LEFT_WEAPON_SLOT : PLAYER_GAME_DATA::CURRENT_RIGHT_WEAPON_SLOT);

    switch (slot)
    {
        case WeaponSlot::PRIMARY:
            return playerGameData.Read<int32_t>(
                isLeftHand ? PLAYER_GAME_DATA::PRIMARY_LEFT_WEAPON : PLAYER_GAME_DATA::PRIMARY_RIGHT_WEAPON);
        case WeaponSlot::SECONDARY:
            return playerGameData.Read<int32_t>(
                isLeftHand ? PLAYER_GAME_DATA::SECONDARY_LEFT_WEAPON : PLAYER_GAME_DATA::SECONDARY_RIGHT_WEAPON);
        case WeaponSlot::CURRENT:
            return -1;  // shouldn't be reachable
    }

    return -1;
}

bool FirelinkDSR::DSRPlayer::SetWeapon(WeaponSlot slot, const int weaponId, const bool isLeftHand) const
{
    const BasePointer playerGameData = GetPlayerGameData();
    if (playerGameData.IsNull())
        return false;

    if (slot == WeaponSlot::CURRENT)
        // Read player's current equipped slot from game.
            slot = playerGameData.Read<WeaponSlot>(
            isLeftHand ? PLAYER_GAME_DATA::CURRENT_LEFT_WEAPON_SLOT : PLAYER_GAME_DATA::CURRENT_RIGHT_WEAPON_SLOT);

    switch (slot)
    {
        case WeaponSlot::PRIMARY:
            return playerGameData.Write<int32_t>(
                isLeftHand ? PLAYER_GAME_DATA::PRIMARY_LEFT_WEAPON : PLAYER_GAME_DATA::PRIMARY_RIGHT_WEAPON, weaponId);
        case WeaponSlot::SECONDARY:
            return playerGameData.Write<int32_t>(
                isLeftHand ? PLAYER_GAME_DATA::SECONDARY_LEFT_WEAPON : PLAYER_GAME_DATA::SECONDARY_RIGHT_WEAPON, weaponId);
        case WeaponSlot::CURRENT:
            return false;
    }

    return false;
}
