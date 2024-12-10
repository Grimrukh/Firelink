#include <string>
#include <vector>

#include "GrimHook/Logging.h"
#include "GrimHook/Pointer.h"
#include "GrimHook/Process.h"

#include "GrimHookDSR/DSRHook.h"
#include "GrimHookDSR/DSROffsets.h"

using namespace std;
using namespace GrimHook;


DSRHook::DSRHook(ManagedProcess* process)
    : m_process(process)
{
    findAoBs();
}

DSRHook::~DSRHook()
{
    delete m_worldChrMan;
    delete m_chrClassBase;
}

DSRHook::DSRHook(DSRHook&& other) noexcept
    : m_process(other.m_process), m_worldChrMan(other.m_worldChrMan), m_chrClassBase(other.m_chrClassBase)
{
    other.m_process = nullptr;
    other.m_worldChrMan = nullptr;
    other.m_chrClassBase = nullptr;
}

DSRHook& DSRHook::operator=(DSRHook&& other) noexcept
{
    if (this != &other)
    {
        delete m_worldChrMan;
        delete m_chrClassBase;

        m_process = other.m_process;
        m_worldChrMan = other.m_worldChrMan;
        m_chrClassBase = other.m_chrClassBase;

        other.m_process = nullptr;
        other.m_worldChrMan = nullptr;
        other.m_chrClassBase = nullptr;
    }
    return *this;
}

void DSRHook::findAoBs()
{
    delete m_worldChrMan;
    delete m_chrClassBase;

    Info(L"Searching for WorldChrMan AOB...");
    if (const LPCVOID worldChrManAobAddr = m_process->findPattern(worldChrManAob))
    {
        m_worldChrMan = BasePointer::withJumpInstruction(m_process, L"WorldChrMan", worldChrManAobAddr, 0x3, { 0 });
        Info(L"WorldChrMan address found.");
    }
    else
    {
        Warning(L"WorldChrMan AOB not found.");
        m_worldChrMan = nullptr;
    }

    Info(L"Searching for ChrClassBase AOB...");
    if (const LPCVOID chrClassBaseAobPtr = m_process->findPattern(chrClassBaseAob))
    {
        m_chrClassBase = BasePointer::withJumpInstruction(m_process, L"ChrClassBase", chrClassBaseAobPtr, 0x3, { 0 });
        Info(L"ChrClassBase address found.");
    }
    else
    {
        Warning(L"ChrClassBase AOB not found.");
        m_chrClassBase = nullptr;
    }
}

bool DSRHook::isGameLoaded() const
{
    // WorldChrMan only resolves to non-null if game is loaded.
    return !m_worldChrMan->isNull();
}

wstring DSRHook::getPlayerModel() const
{
    auto playerInsPtr = ChildPointer(L"PlayerIns", m_worldChrMan, { world_chr_man_offsets::PLAYER_INS });
    wstring modelName = playerInsPtr.readUnicodeString(enemy_ins_offsets::MODEL_NAME, 5);
    return modelName;
}

int* DSRHook::getPlayerHp() const
{
    auto playerInsPtr = ChildPointer(L"PlayerIns", m_worldChrMan, { world_chr_man_offsets::PLAYER_INS });
    const int currentHP = playerInsPtr.readInt32(enemy_ins_offsets::CURRENT_HP);
    const int maxHP = playerInsPtr.readInt32(enemy_ins_offsets::MAX_HP);
    return new int[2] { currentHP, maxHP };
}

vector<int> DSRHook::getPlayerActiveSpEffects() const
{
    auto playerInsPtr = ChildPointer(L"PlayerIns", m_worldChrMan, { world_chr_man_offsets::PLAYER_INS });
    if (playerInsPtr.isNull())
        return {};  // not loaded yet
    auto spEffectManagerPtr = ChildPointer(L"SpEffectManager", &playerInsPtr, { enemy_ins_offsets::SP_EFFECT_MANAGER_ADDR });
    if (spEffectManagerPtr.isNull())
        return {};  // not loaded yet

    auto nextEntryAddr = const_cast<LPVOID>(spEffectManagerPtr.readPointer(0x8));
    if (nextEntryAddr == nullptr)
        return {};  // no SpEffects (extremely unlikely for player!)

    vector<int> spEffectIDs;
    while (nextEntryAddr != nullptr)
    {
        auto spEffectEntryPtr = BasePointer(m_process, L"SpEffectEntry", nextEntryAddr, {});
        int spEffectId = spEffectEntryPtr.readInt32(sp_effect_entry_offsets::SP_EFFECT_ID);
        spEffectIDs.push_back(spEffectId);
        nextEntryAddr = const_cast<LPVOID>(spEffectEntryPtr.readPointer(sp_effect_entry_offsets::NEXT_ENTRY_PTR));  // nullptr for last entry
    }
    return spEffectIDs;
}

bool DSRHook::playerHasSpEffect(const int spEffectId) const
{
    vector<int> playerSpEffects = getPlayerActiveSpEffects();
    return ranges::find(playerSpEffects.begin(), playerSpEffects.end(), spEffectId) != playerSpEffects.end();
}

// --- EQUIPPED WEAPONS ---

int DSRHook::getRightHandWeapon(WeaponSlot slot) const
{
    auto chrAsmPtr = ChildPointer(L"ChrAsm", m_chrClassBase, { chr_class_base_offsets::CHR_ASM });
    if (slot == CURRENT)
        slot = static_cast<WeaponSlot>(chrAsmPtr.readInt32(chr_asm_offsets::CURRENT_RIGHT_WEAPON_SLOT));
    switch (slot)
    {
    case PRIMARY:
        return chrAsmPtr.readInt32(chr_asm_offsets::PRIMARY_RIGHT_WEAPON);
    case SECONDARY:
        return chrAsmPtr.readInt32(chr_asm_offsets::SECONDARY_RIGHT_WEAPON);
    case CURRENT:
        return -1;
    }

    return -1;
}

void DSRHook::setRightHandWeapon(WeaponSlot slot, const int weaponId) const
{
    auto chrAsmPtr = ChildPointer(L"ChrAsm", m_chrClassBase, { chr_class_base_offsets::CHR_ASM });
    if (slot == CURRENT)
        slot = static_cast<WeaponSlot>(chrAsmPtr.readInt32(chr_asm_offsets::CURRENT_RIGHT_WEAPON_SLOT));
    switch (slot)
    {
    case PRIMARY:
        chrAsmPtr.writeInt32(chr_asm_offsets::PRIMARY_RIGHT_WEAPON, weaponId);
        break;
    case SECONDARY:
        chrAsmPtr.writeInt32(chr_asm_offsets::SECONDARY_RIGHT_WEAPON, weaponId);
        break;
    case CURRENT:
        break;
    }
}

int DSRHook::getLeftHandWeapon(WeaponSlot slot) const
{
    auto chrAsmPtr = ChildPointer(L"ChrAsm", m_chrClassBase, { chr_class_base_offsets::CHR_ASM });
    if (slot == CURRENT)
        slot = static_cast<WeaponSlot>(chrAsmPtr.readInt32(chr_asm_offsets::CURRENT_LEFT_WEAPON_SLOT));
    switch (slot)
    {
    case PRIMARY:
        return chrAsmPtr.readInt32(chr_asm_offsets::PRIMARY_LEFT_WEAPON);
    case SECONDARY:
        return chrAsmPtr.readInt32(chr_asm_offsets::SECONDARY_LEFT_WEAPON);
    case CURRENT:
        return -1;
    }

    return -1;
}

void DSRHook::setLeftHandWeapon(WeaponSlot slot, int weaponId) const
{
    auto chrAsmPtr = ChildPointer(L"ChrAsm", m_chrClassBase, { chr_class_base_offsets::CHR_ASM });
    if (slot == CURRENT)
        slot = static_cast<WeaponSlot>(chrAsmPtr.readInt32(chr_asm_offsets::CURRENT_LEFT_WEAPON_SLOT));
    switch (slot)
    {
    case PRIMARY:
        chrAsmPtr.writeInt32(chr_asm_offsets::PRIMARY_LEFT_WEAPON, weaponId);
        break;
    case SECONDARY:
        chrAsmPtr.writeInt32(chr_asm_offsets::SECONDARY_LEFT_WEAPON, weaponId);
        break;
    case CURRENT:
        break;
    }
}
