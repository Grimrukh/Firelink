#pragma once

namespace world_chr_man_offsets
{
    inline constexpr int PLAYER_INS = 0x68;  // void* -> EnemyIns
    inline constexpr int DEATH_CAM = 0x70;  // bool
}

namespace enemy_ins_offsets
{
    inline constexpr int LOCAL_HANDLE = 0x8;  // int
    // 0x10: pad[8]
    // 0x18: void* -> ??
    // 0x20: void* -> ??
    // 0x28: float?
    // 0x2C: float?
    // 0x30: void* -> ?? anim stuff?
    // 0x38: void* -> ?? static memory
    // 0x40: void* -> ??
    // 0x48: void* -> ?? weird
    // 0x50: void* -> ??
    // 0x58: int = 2
    // 0x5C: int = 2
    // 0x60: void* -> ??
    // 0x68: void* -> ??
    // 0x70: void* -> ??
    // 0x78: int ?
    // 0x7C: 0
    // 0x80: void* -> ?? static memory
    inline constexpr int MODEL_NAME = 0x88;  // -> char16_t[6] (0xC)
    // 0x94: int ?
    // 0x98: long = 5
    // 0x100: long = 7
    // ...
    // 0x120: void* -> ??

    inline constexpr int TEAM_TYPE = 0xD8;  // int

    inline constexpr int SP_EFFECT_MANAGER_ADDR = 0x278;  // void* -> SpEffectManager

    inline constexpr int CURRENT_HP = 0x3E8;  // int
    inline constexpr int MAX_HP = 0x3EC;  // int
}

namespace sp_effect_manager_offsets
{
    inline constexpr int FIRST_SP_EFFECT_ENTRY = 0x8;  // void* -> SpEffectEntry
    // 0x10: some combination of bytes/flags (int); seems to have byte 1 at 0x10 when 40 is active (maybe whenever a non-permanent SpEffect is active?)
    inline constexpr int UNK_FLOAT14 = 0x14;  // float (-1)
    inline constexpr int ENEMY_INS_ADDR = 0x18;  // void* -> EnemyIns
    // more...
}

namespace sp_effect_entry_offsets
{
    inline constexpr int LIFE = 0x0;  // float
    inline constexpr int UNK_FLOAT04 = 0x4;  // float
    inline constexpr int UNK_FLOAT08 = 0x8;  // float
    inline constexpr int UNK_FLOAT0C = 0xC;  // float
    inline constexpr int UNK_FLOAT10 = 0x10;  // float
    inline constexpr int UNK_INT14 = 0x14;  // int (4)
    inline constexpr int UNK_LONG18 = 0x18;  // long
    inline constexpr int CHR_LOCAL_HANDLE = 0x20;  // int
    inline constexpr int UNK_INT24 = 0x24;  // int
    inline constexpr int UNK_LONG28 = 0x28;  // long
    inline constexpr int SP_EFFECT_ID = 0x30;  // int
    inline constexpr int UNK_INT34 = 0x34;  // int (1)
    inline constexpr int UNK_PTR38 = 0x38;  // void* -> some array of floats
    inline constexpr int NEXT_ENTRY_PTR = 0x40;  // void* -> SpEffectEntry
    inline constexpr int PREV_ENTRY_PTR = 0x48;  // void* -> SpEffectEntry
}

namespace chr_class_base_offsets
{
    inline constexpr int CHR_ASM = 0x10;  // void* -> ChrAsm
}

namespace chr_asm_offsets
{
    inline constexpr int HP = 0x14;  // int
    inline constexpr int BASE_MAX_HP = 0x18;  // int
    inline constexpr int MP = 0x20;  // int
    inline constexpr int MAX_MP = 0x24;  // int
    inline constexpr int SP = 0x30;  // int
    inline constexpr int MAX_SP = 0x34;  // int

    inline constexpr int VITALITY = 0x40;  // int
    inline constexpr int ATTUNEMENT = 0x48;  // int
    inline constexpr int ENDURANCE = 0x50;  // int
    inline constexpr int STRENGTH = 0x58;  // int
    inline constexpr int DEXTERITY = 0x60;  // int
    inline constexpr int INTELLIGENCE = 0x68;  // int
    inline constexpr int FAITH = 0x70;  // int
    inline constexpr int HUMANITY = 0x84;  // int
    inline constexpr int RESISTANCE = 0x88;  // int

    inline constexpr int SOUL_LEVEL = 0x90;  // int
    inline constexpr int SOULS = 0x94;  // int
    inline constexpr int LIFETIME_SOULS = 0x98;  // int

    inline constexpr int CHR_TYPE = 0xA4;  // int
    inline constexpr int GENDER = 0xCA;  // byte (1 = Male)
    inline constexpr int SHOP_LEVEL = 0xCC;  // byte  // TODO: ?
    inline constexpr int CLASS = 0xCE;  // byte
    inline constexpr int PHYSIQUE = 0xCF;  // byte
    inline constexpr int GIFT = 0xD0;  // byte

    inline constexpr int MULTIPLAYER_COUNT = 0xD4;  // byte
    inline constexpr int COOP_SUCCESS_COUNT = 0xD8;  // byte
    inline constexpr int INVASION_SUCCESS_COUNT = 0xDC;  // byte

    inline constexpr int WARRIOR_OF_SUNLIGHT_RANK = 0xED;  // byte
    inline constexpr int DARKWRAITH_RANK = 0xEE;  // byte
    inline constexpr int PATH_OF_THE_DRAGON_RANK = 0xEF;  // byte
    inline constexpr int GRAVELORD_SERVANT_RANK = 0xF0;  // byte
    inline constexpr int FOREST_HUNTER_RANK = 0xF1;  // byte
    inline constexpr int DARKMOON_BLADE_RANK = 0xF2;  // byte
    inline constexpr int CHAOS_SERVANT_RANK = 0xF3;  // byte

    inline constexpr int POISON_RESISTANCE = 0x100;  // int
    inline constexpr int BLEED_RESISTANCE = 0x104;  // int
    inline constexpr int TOXIC_RESISTANCE = 0x108;  // int
    inline constexpr int CURSE_RESISTANCE = 0x10C;  // int

    inline constexpr int COVENANT_TYPE = 0x113;  // byte
    inline constexpr int FACE_TYPE = 0x114;  // byte
    inline constexpr int HAIR_TYPE = 0x115;  // byte
    inline constexpr int HAIR_EYES_COLOR = 0x116;  // byte
    inline constexpr int CURSE_LEVEL = 0x117;  // byte
    inline constexpr int INVADER_TYPE = 0x118;  // byte

    inline constexpr int STEAM_PROFILE_NAME_OFFSET = 0x164;  // void* -> null-terminated UTF-16

    // NOTE: Picking up and dropping items does NOT change these indices. Seems to be determined early and only
    // incremented from there. And if you drop an item and pick it up, it still has the same index. Maybe some kind
    // of anti-cheat system... True hot-swapping probably requires these indices to change in sync with the IDs.
    inline constexpr int UNKNOWN_INV_INDEX = 0x2A0;  // seems to come before first weapon... maybe 'first index'?
    inline constexpr int PRIMARY_LEFT_WEAPON_INV_INDEX = 0x2A4;
    inline constexpr int PRIMARY_RIGHT_WEAPON_INV_INDEX = 0x2A8;
    inline constexpr int SECONDARY_LEFT_WEAPON_INV_INDEX = 0x2AC;
    inline constexpr int SECONDARY_RIGHT_WEAPON_INV_INDEX = 0x2B0;
    inline constexpr int PRIMARY_ARROW_INV_INDEX = 0x2B4;
    inline constexpr int PRIMARY_BOLT_INV_INDEX = 0x2B8;
    inline constexpr int SECONDARY_ARROW_INV_INDEX = 0x2BC;
    inline constexpr int SECONDARY_BOLT_INV_INDEX = 0x2C0;
    inline constexpr int HELM_INV_INDEX = 0x2C4;
    inline constexpr int CHEST_INV_INDEX = 0x2C8;
    inline constexpr int GAUNTLETS_INV_INDEX = 0x2CC;
    inline constexpr int LEGGINGS_INV_INDEX = 0x2D0;
    inline constexpr int HAIR_INV_INDEX = 0x2D4;  // (always -1)
    inline constexpr int EQUIPPED_RING1_INV_INDEX = 0x2D8;
    inline constexpr int EQUIPPED_RING2_INV_INDEX = 0x2DC;
    inline constexpr int EQUIPPED_GOOD1_INV_INDEX = 0x2E0;
    inline constexpr int EQUIPPED_GOOD2_INV_INDEX = 0x2E4;
    inline constexpr int EQUIPPED_GOOD3_INV_INDEX = 0x2E8;
    inline constexpr int EQUIPPED_GOOD4_INV_INDEX = 0x2EC;
    inline constexpr int EQUIPPED_GOOD5_INV_INDEX = 0x2F0;

    // 0x2F4: four zero bytes
    // 0x2F8: eight zero bytes
    // 0x300: a big 8-byte pointer (0x141...)

    inline constexpr int HANDED_STYLE = 0x308;  // HandedStyle
    inline constexpr int CURRENT_LEFT_WEAPON_SLOT = 0x30C;  // WeaponSlot
    inline constexpr int CURRENT_RIGHT_WEAPON_SLOT = 0x310;  // WeaponSlot
    inline constexpr int CURRENT_LEFT_ARROW_SLOT = 0x314;  // WeaponSlot
    inline constexpr int CURRENT_RIGHT_ARROW_SLOT = 0x318;  // WeaponSlot
    inline constexpr int CURRENT_LEFT_BOLT_SLOT = 0x31C;  // WeaponSlot
    inline constexpr int CURRENT_RIGHT_BOLT_SLOT = 0x320;  // WeaponSlot
    // These are real GameParam IDs:
    inline constexpr int PRIMARY_LEFT_WEAPON = 0x324;  // int
    inline constexpr int PRIMARY_RIGHT_WEAPON = 0x328;  // int
    inline constexpr int SECONDARY_LEFT_WEAPON = 0x32C;  // int
    inline constexpr int SECONDARY_RIGHT_WEAPON = 0x330;  // int
    inline constexpr int PRIMARY_ARROW = 0x334;  // int
    inline constexpr int PRIMARY_BOLT = 0x338;  // int
    inline constexpr int SECONDARY_ARROW = 0x340;  // int
    inline constexpr int SECONDARY_BOLT = 0x344;  // int
    inline constexpr int HELM = 0x344;  // int
    inline constexpr int CHEST = 0x348;  // int
    inline constexpr int GAUNTLETS = 0x34C;  // int
    inline constexpr int LEGGINGS = 0x350;  // int
    inline constexpr int HAIR = 0x354;  // int
    inline constexpr int EQUIPPED_RING1 = 0x358;  // int
    inline constexpr int EQUIPPED_RING2 = 0x35C;  // int
    inline constexpr int EQUIPPED_GOOD1 = 0x360;  // int
    inline constexpr int EQUIPPED_GOOD2 = 0x364;  // int
    inline constexpr int EQUIPPED_GOOD3 = 0x368;  // int
    inline constexpr int EQUIPPED_GOOD4 = 0x36C;  // int
    inline constexpr int EQUIPPED_GOOD5 = 0x370;  // int

    inline constexpr int HEAD_SIZE = 0x388;  // float
    inline constexpr int CHEST_SIZE = 0x38C;  // float
    inline constexpr int ABDOMEN_SIZE = 0x390;  // float
    inline constexpr int ARMS_SIZE = 0x394;  // float
    inline constexpr int LEGS_SIZE = 0x398;  // float

    inline constexpr int ATTUNEMENT_SLOT_DATA_OFFSET = 0x418;  // void* (slot 1 is 0x18, slot 2 is 0x20, ..., slot 12 is 0x70)

    inline constexpr int GESTURE_EQUIP_DATA_OFFSET = 0x450;  // void*

    inline constexpr int HAIR_COLOR_RED = 0x4C0;  // float
    inline constexpr int HAIR_COLOR_GREEN = 0x4C4;  // float
    inline constexpr int HAIR_COLOR_BLUE = 0x4C8;  // float
    inline constexpr int PUPIL_COLOR_RED = 0x4D0;  // float
    inline constexpr int PUPIL_COLOR_GREEN = 0x4D4;  // float
    inline constexpr int PUPIL_COLOR_BLUE = 0x4D8;  // float

    inline constexpr int FACE_DATA_ARRAY = 0x4E0;  // byte[100] (NOT a pointer)
    // NOTE: Too lazy to expose all the individual face data bytes.

    inline constexpr int GESTURE_UNLOCK_DATA = 0x568;  // void*
}
