#pragma once

namespace world_chr_man_offsets
{
    constexpr int PLAYER_INS = 0x68;  // LPVOID -> EnemyIns
    constexpr int DEATH_CAM = 0x70;  // bool
}

namespace enemy_ins_offsets
{
    constexpr int LOCAL_HANDLE = 0x8;  // int
    // 0x10: pad[8]
    // 0x18: ptr -> ??
    // 0x20: ptr -> ??
    // 0x28: float?
    // 0x2C: float?
    // 0x30: ptr -> ?? anim stuff?
    // 0x38: ptr -> ?? static memory
    // 0x40: ptr -> ??
    // 0x48: ptr -> ?? weird
    // 0x50: ptr -> ??
    // 0x58: int = 2
    // 0x5C: int = 2
    // 0x60: ptr -> ??
    // 0x68: ptr -> ??
    // 0x70: ptr -> ??
    // 0x78: int ?
    // 0x7C: 0
    // 0x80: ptr -> ?? static memory
    constexpr int MODEL_NAME = 0x88;  // wchar[6] (0xC)
    // 0x94: int ?
    // 0x98: long = 5
    // 0x100: long = 7
    // ...
    // 0x120: ptr -> ??

    constexpr int TEAM_TYPE = 0xD8;  // int

    constexpr int SP_EFFECT_MANAGER_ADDR = 0x278;  // LPVOID -> SpEffectManager

    constexpr int CURRENT_HP = 0x3E8;  // int
    constexpr int MAX_HP = 0x3EC;  // int
}

namespace sp_effect_manager_offsets
{
    constexpr int FIRST_SP_EFFECT_ENTRY = 0x8;  // LPVOID -> SpEffectEntry
    // 0x10: some combination of bytes/flags (int); seems to have byte 1 at 0x10 when 40 is active (maybe whenever a non-permanent SpEffect is active?)
    constexpr int UNK_FLOAT14 = 0x14;  // float (-1)
    constexpr int ENEMY_INS_ADDR = 0x18;  // LPVOID -> EnemyIns
    // more...
}

namespace sp_effect_entry_offsets
{
    constexpr int LIFE = 0x0;  // float
    constexpr int UNK_FLOAT04 = 0x4;  // float
    constexpr int UNK_FLOAT08 = 0x8;  // float
    constexpr int UNK_FLOAT0C = 0xC;  // float
    constexpr int UNK_FLOAT10 = 0x10;  // float
    constexpr int UNK_INT14 = 0x14;  // int (4)
    constexpr int UNK_LONG18 = 0x18;  // long
    constexpr int CHR_LOCAL_HANDLE = 0x20;  // int
    constexpr int UNK_INT24 = 0x24;  // int
    constexpr int UNK_LONG28 = 0x28;  // long
    constexpr int SP_EFFECT_ID = 0x30;  // int
    constexpr int UNK_INT34 = 0x34;  // int (1)
    constexpr int UNK_PTR38 = 0x38;  // LPVOID -> some array of floats
    constexpr int NEXT_ENTRY_PTR = 0x40;  // LPVOID -> SpEffectEntry
    constexpr int PREV_ENTRY_PTR = 0x48;  // LPVOID -> SpEffectEntry
}

namespace chr_class_base_offsets
{
    constexpr int CHR_ASM = 0x10;  // LPVOID -> ChrAsm
}

namespace chr_asm_offsets
{
    constexpr int HP = 0x14;  // int
    constexpr int BASE_MAX_HP = 0x18;  // int
    constexpr int MP = 0x20;  // int
    constexpr int MAX_MP = 0x24;  // int
    constexpr int SP = 0x30;  // int
    constexpr int MAX_SP = 0x34;  // int

    constexpr int VITALITY = 0x40;  // int
    constexpr int ATTUNEMENT = 0x48;  // int
    constexpr int ENDURANCE = 0x50;  // int
    constexpr int STRENGTH = 0x58;  // int
    constexpr int DEXTERITY = 0x60;  // int
    constexpr int INTELLIGENCE = 0x68;  // int
    constexpr int FAITH = 0x70;  // int
    constexpr int HUMANITY = 0x84;  // int
    constexpr int RESISTANCE = 0x88;  // int

    constexpr int SOUL_LEVEL = 0x90;  // int
    constexpr int SOULS = 0x94;  // int
    constexpr int LIFETIME_SOULS = 0x98;  // int

    constexpr int CHR_TYPE = 0xA4;  // int
    constexpr int GENDER = 0xCA;  // byte (1 = Male)
    constexpr int SHOP_LEVEL = 0xCC;  // byte  // TODO: ?
    constexpr int CLASS = 0xCE;  // byte
    constexpr int PHYSIQUE = 0xCF;  // byte
    constexpr int GIFT = 0xD0;  // byte

    constexpr int MULTIPLAYER_COUNT = 0xD4;  // byte
    constexpr int COOP_SUCCESS_COUNT = 0xD8;  // byte
    constexpr int INVASION_SUCCESS_COUNT = 0xDC;  // byte

    constexpr int WARRIOR_OF_SUNLIGHT_RANK = 0xED;  // byte
    constexpr int DARKWRAITH_RANK = 0xEE;  // byte
    constexpr int PATH_OF_THE_DRAGON_RANK = 0xEF;  // byte
    constexpr int GRAVELORD_SERVANT_RANK = 0xF0;  // byte
    constexpr int FOREST_HUNTER_RANK = 0xF1;  // byte
    constexpr int DARKMOON_BLADE_RANK = 0xF2;  // byte
    constexpr int CHAOS_SERVANT_RANK = 0xF3;  // byte

    constexpr int POISON_RESISTANCE = 0x100;  // int
    constexpr int BLEED_RESISTANCE = 0x104;  // int
    constexpr int TOXIC_RESISTANCE = 0x108;  // int
    constexpr int CURSE_RESISTANCE = 0x10C;  // int

    constexpr int COVENANT_TYPE = 0x113;  // byte
    constexpr int FACE_TYPE = 0x114;  // byte
    constexpr int HAIR_TYPE = 0x115;  // byte
    constexpr int HAIR_EYES_COLOR = 0x116;  // byte
    constexpr int CURSE_LEVEL = 0x117;  // byte
    constexpr int INVADER_TYPE = 0x118;  // byte

    constexpr int STEAM_PROFILE_NAME_OFFSET = 0x164;  // LPVOID -> null-terminated std::wstring

    // NOTE: Picking up and dropping items does NOT change these indices. Seems to be determined early and only
    // incremented from there. And if you drop an item and pick it up, it still has the same index. Maybe some kind
    // of anti-cheat system... True hot-swapping probably requires these indices to change in sync with the IDs.
    constexpr int UNKNOWN_INV_INDEX = 0x2A0;  // seems to come before first weapon... maybe 'first index'?
    constexpr int PRIMARY_LEFT_WEAPON_INV_INDEX = 0x2A4;
    constexpr int PRIMARY_RIGHT_WEAPON_INV_INDEX = 0x2A8;
    constexpr int SECONDARY_LEFT_WEAPON_INV_INDEX = 0x2AC;
    constexpr int SECONDARY_RIGHT_WEAPON_INV_INDEX = 0x2B0;
    constexpr int PRIMARY_ARROW_INV_INDEX = 0x2B4;
    constexpr int PRIMARY_BOLT_INV_INDEX = 0x2B8;
    constexpr int SECONDARY_ARROW_INV_INDEX = 0x2BC;
    constexpr int SECONDARY_BOLT_INV_INDEX = 0x2C0;
    constexpr int HELM_INV_INDEX = 0x2C4;
    constexpr int CHEST_INV_INDEX = 0x2C8;
    constexpr int GAUNTLETS_INV_INDEX = 0x2CC;
    constexpr int LEGGINGS_INV_INDEX = 0x2D0;
    constexpr int HAIR_INV_INDEX = 0x2D4;  // (always -1)
    constexpr int EQUIPPED_RING1_INV_INDEX = 0x2D8;
    constexpr int EQUIPPED_RING2_INV_INDEX = 0x2DC;
    constexpr int EQUIPPED_GOOD1_INV_INDEX = 0x2E0;
    constexpr int EQUIPPED_GOOD2_INV_INDEX = 0x2E4;
    constexpr int EQUIPPED_GOOD3_INV_INDEX = 0x2E8;
    constexpr int EQUIPPED_GOOD4_INV_INDEX = 0x2EC;
    constexpr int EQUIPPED_GOOD5_INV_INDEX = 0x2F0;

    // 0x2F4: four zero bytes
    // 0x2F8: eight zero bytes
    // 0x300: a big 8-byte pointer (0x141...)

    constexpr int HANDED_STYLE = 0x308;  // HandedStyle
    constexpr int CURRENT_LEFT_WEAPON_SLOT = 0x30C;  // WeaponSlot
    constexpr int CURRENT_RIGHT_WEAPON_SLOT = 0x310;  // WeaponSlot
    constexpr int CURRENT_LEFT_ARROW_SLOT = 0x314;  // WeaponSlot
    constexpr int CURRENT_RIGHT_ARROW_SLOT = 0x318;  // WeaponSlot
    constexpr int CURRENT_LEFT_BOLT_SLOT = 0x31C;  // WeaponSlot
    constexpr int CURRENT_RIGHT_BOLT_SLOT = 0x320;  // WeaponSlot
    // These are real GameParam IDs:
    constexpr int PRIMARY_LEFT_WEAPON = 0x324;  // int
    constexpr int PRIMARY_RIGHT_WEAPON = 0x328;  // int
    constexpr int SECONDARY_LEFT_WEAPON = 0x32C;  // int
    constexpr int SECONDARY_RIGHT_WEAPON = 0x330;  // int
    constexpr int PRIMARY_ARROW = 0x334;  // int
    constexpr int PRIMARY_BOLT = 0x338;  // int
    constexpr int SECONDARY_ARROW = 0x340;  // int
    constexpr int SECONDARY_BOLT = 0x344;  // int
    constexpr int HELM = 0x344;  // int
    constexpr int CHEST = 0x348;  // int
    constexpr int GAUNTLETS = 0x34C;  // int
    constexpr int LEGGINGS = 0x350;  // int
    constexpr int HAIR = 0x354;  // int
    constexpr int EQUIPPED_RING1 = 0x358;  // int
    constexpr int EQUIPPED_RING2 = 0x35C;  // int
    constexpr int EQUIPPED_GOOD1 = 0x360;  // int
    constexpr int EQUIPPED_GOOD2 = 0x364;  // int
    constexpr int EQUIPPED_GOOD3 = 0x368;  // int
    constexpr int EQUIPPED_GOOD4 = 0x36C;  // int
    constexpr int EQUIPPED_GOOD5 = 0x370;  // int

    constexpr int HEAD_SIZE = 0x388;  // float
    constexpr int CHEST_SIZE = 0x38C;  // float
    constexpr int ABDOMEN_SIZE = 0x390;  // float
    constexpr int ARMS_SIZE = 0x394;  // float
    constexpr int LEGS_SIZE = 0x398;  // float

    constexpr int ATTUNEMENT_SLOT_DATA_OFFSET = 0x418;  // LPVOID (slot 1 is 0x18, slot 2 is 0x20, ..., slot 12 is 0x70)

    constexpr int GESTURE_EQUIP_DATA_OFFSET = 0x450;  // LPVOID

    constexpr int HAIR_COLOR_RED = 0x4C0;  // float
    constexpr int HAIR_COLOR_GREEN = 0x4C4;  // float
    constexpr int HAIR_COLOR_BLUE = 0x4C8;  // float
    constexpr int PUPIL_COLOR_RED = 0x4D0;  // float
    constexpr int PUPIL_COLOR_GREEN = 0x4D4;  // float
    constexpr int PUPIL_COLOR_BLUE = 0x4D8;  // float

    constexpr int FACE_DATA_ARRAY = 0x4E0;  // byte[100] (NOT a pointer)
    // NOTE: Too lazy to expose all the individual face data bytes.

    constexpr int GESTURE_UNLOCK_DATA = 0x568;  // LPVOID
}