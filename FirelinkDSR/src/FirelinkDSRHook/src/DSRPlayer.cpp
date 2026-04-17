#include <FirelinkCore/MemoryUtils.h>
#include <FirelinkCore/Pointer.h>
#include <FirelinkDSRHook/DSROffsets.h>
#include <FirelinkDSRHook/DSRPlayer.h>

#include <ranges>

using namespace Firelink;

std::u16string FirelinkDSR::DSRPlayer::GetPlayerName() const
{
    return playerInsPtr.ReadUTF16String(PLAYER_INS::CHR_INS_NO_VTABLE + CHR_INS_NO_VTABLE::CHR_NAME, 0x28);
}

std::pair<int, int> FirelinkDSR::DSRPlayer::GetPlayerHp() const
{
    const int currentHP = playerInsPtr.Read<int32_t>(PLAYER_INS::CHR_INS_NO_VTABLE + CHR_INS_NO_VTABLE::CURRENT_HP);
    const int maxHP = playerInsPtr.Read<int32_t>(PLAYER_INS::CHR_INS_NO_VTABLE + CHR_INS_NO_VTABLE::MAX_HP);
    return {currentHP, maxHP};
}

std::vector<int> FirelinkDSR::DSRPlayer::GetPlayerActiveSpEffects() const
{
    if (playerInsPtr.IsNull())
        return {};

    const BasePointer specialEffectPtr =
        playerInsPtr.ReadPointer("SpecialEffect", PLAYER_INS::CHR_INS_NO_VTABLE + CHR_INS_NO_VTABLE::SPECIAL_EFFECT_PTR);

    // Active SpEffects are implemented in the game engine as a linked list.
    const void* nextEntryNode = specialEffectPtr.Read<void*>(SPECIAL_EFFECT::FIRST_SP_EFFECT_NODE_PTR);
    if (nextEntryNode == nullptr)
        return {}; // no SpEffects (extremely unlikely for player!)

    const std::shared_ptr<ManagedProcess> process = hook->GetProcess();
    std::vector<int> spEffectIDs; // can't know the size in advance
    while (nextEntryNode != nullptr)
    {
        const void* spEffectIdOffset = GetOffsetPointer(nextEntryNode, SP_EFFECT_NODE::SP_EFFECT_ID);
        int spEffectId = process->Read<int32_t>(spEffectIdOffset);
        spEffectIDs.push_back(spEffectId);

        // Read next entry address (nullptr for last entry).
        const void* nextNodePtrOffset = GetOffsetPointer(nextEntryNode, SP_EFFECT_NODE::NEXT_ENTRY_PTR);
        nextEntryNode = process->Read<void*>(nextNodePtrOffset);
    }

    return spEffectIDs;
}

bool FirelinkDSR::DSRPlayer::HasSpEffect(const int spEffectId) const
{
    std::vector<int> playerSpEffects = GetPlayerActiveSpEffects();
    return std::ranges::find(playerSpEffects.begin(), playerSpEffects.end(), spEffectId) != playerSpEffects.end();
}

// --- EQUIPPED WEAPONS ---

FirelinkDSR::WeaponSlot FirelinkDSR::DSRPlayer::GetWeaponSlot(const bool isLeftHand) const
{
    const BasePointer playerGameData = GetPlayerGameData();
    if (playerGameData.IsNull())
        return WeaponSlot::CURRENT; // no game data, return current slot

    return playerGameData.Read<WeaponSlot>(
        isLeftHand ? PLAYER_GAME_DATA::CURRENT_LEFT_WEAPON_SLOT : PLAYER_GAME_DATA::CURRENT_RIGHT_WEAPON_SLOT);
}

int FirelinkDSR::DSRPlayer::GetWeapon(WeaponSlot slot, const bool isLeftHand) const
{
    const BasePointer playerGameData = GetPlayerGameData();
    if (playerGameData.IsNull())
        return -1;

    if (slot == WeaponSlot::CURRENT)
    {
        // Read player's current equipped slot from game.
        slot = playerGameData.Read<WeaponSlot>(
            isLeftHand ? PLAYER_GAME_DATA::CURRENT_LEFT_WEAPON_SLOT : PLAYER_GAME_DATA::CURRENT_RIGHT_WEAPON_SLOT);
    }

    switch (slot)
    {
        case WeaponSlot::PRIMARY:
            return playerGameData.Read<int32_t>(
                isLeftHand ? PLAYER_GAME_DATA::PRIMARY_LEFT_WEAPON : PLAYER_GAME_DATA::PRIMARY_RIGHT_WEAPON);
        case WeaponSlot::SECONDARY:
            return playerGameData.Read<int32_t>(
                isLeftHand ? PLAYER_GAME_DATA::SECONDARY_LEFT_WEAPON : PLAYER_GAME_DATA::SECONDARY_RIGHT_WEAPON);
        case WeaponSlot::CURRENT:
            return -1; // shouldn't be reachable
    }

    return -1;
}

bool FirelinkDSR::DSRPlayer::SetWeapon(WeaponSlot slot, const int weaponId, const bool isLeftHand) const
{
    const BasePointer playerGameData = GetPlayerGameData();
    if (playerGameData.IsNull())
        return false;

    if (slot == WeaponSlot::CURRENT)
    {
        // Read player's current equipped slot from game.
        slot = playerGameData.Read<WeaponSlot>(
            isLeftHand ? PLAYER_GAME_DATA::CURRENT_LEFT_WEAPON_SLOT : PLAYER_GAME_DATA::CURRENT_RIGHT_WEAPON_SLOT);
    }

    switch (slot)
    {
        case WeaponSlot::PRIMARY:
            return playerGameData.Write<int32_t>(
                isLeftHand ? PLAYER_GAME_DATA::PRIMARY_LEFT_WEAPON : PLAYER_GAME_DATA::PRIMARY_RIGHT_WEAPON, weaponId);
        case WeaponSlot::SECONDARY:
            return playerGameData.Write<int32_t>(
                isLeftHand ? PLAYER_GAME_DATA::SECONDARY_LEFT_WEAPON : PLAYER_GAME_DATA::SECONDARY_RIGHT_WEAPON,
                weaponId);
        case WeaponSlot::CURRENT:
            return false;
    }

    return false;
}

int FirelinkDSR::DSRPlayer::GetArmor(const ArmorType slot) const
{
    const BasePointer playerGameData = GetPlayerGameData();
    if (playerGameData.IsNull())
        return -1;

    switch (slot)
    {
        case ArmorType::HEAD:
            return playerGameData.Read<int32_t>(PLAYER_GAME_DATA::HEAD_ARMOR);
        case ArmorType::BODY:
            return playerGameData.Read<int32_t>(PLAYER_GAME_DATA::BODY_ARMOR);
        case ArmorType::ARMS:
            return playerGameData.Read<int32_t>(PLAYER_GAME_DATA::ARMS_ARMOR);
        case ArmorType::LEGS:
            return playerGameData.Read<int32_t>(PLAYER_GAME_DATA::LEGS_ARMOR);
        default:
            return -1; // not reachable
    }
}

bool FirelinkDSR::DSRPlayer::SetArmor(const ArmorType slot, const int armorId) const
{
    const BasePointer playerGameData = GetPlayerGameData();
    if (playerGameData.IsNull())
        return false;

    switch (slot)
    {
        case ArmorType::HEAD:
            return playerGameData.Write<int32_t>(PLAYER_GAME_DATA::HEAD_ARMOR, armorId);
        case ArmorType::BODY:
            return playerGameData.Write<int32_t>(PLAYER_GAME_DATA::BODY_ARMOR, armorId);
        case ArmorType::ARMS:
            return playerGameData.Write<int32_t>(PLAYER_GAME_DATA::ARMS_ARMOR, armorId);
        case ArmorType::LEGS:
            return playerGameData.Write<int32_t>(PLAYER_GAME_DATA::LEGS_ARMOR, armorId);
        default:
            return false; // not reachable
    }
}

int FirelinkDSR::DSRPlayer::GetRing(const int slot) const
{
    if (slot < 0 || slot > 1)
    {
        Error("Invalid ring slot (must be 0 or 1).");
        return -1;
    }

    const BasePointer playerGameData = GetPlayerGameData();
    if (playerGameData.IsNull())
        return -1;

    if (slot == 0)
        return playerGameData.Read<int32_t>(PLAYER_GAME_DATA::EQUIPPED_RING1);
    return playerGameData.Read<int32_t>(PLAYER_GAME_DATA::EQUIPPED_RING2);
}

bool FirelinkDSR::DSRPlayer::SetRing(const int slot, const int ringId) const
{
    if (slot < 0 || slot > 1)
    {
        Error("Invalid ring slot (must be 0 or 1).");
        return false;
    }

    const BasePointer playerGameData = GetPlayerGameData();
    if (playerGameData.IsNull())
        return false;

    if (slot == 0)
        return playerGameData.Write<int32_t>(PLAYER_GAME_DATA::EQUIPPED_RING1, ringId);
    return playerGameData.Write<int32_t>(PLAYER_GAME_DATA::EQUIPPED_RING2, ringId);
}
