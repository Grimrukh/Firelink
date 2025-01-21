#pragma once

#define OFFSET  inline constexpr int

namespace world_chr_man_offsets
{
    OFFSET  PLAYER_INS = 0x68;  // void* -> EnemyIns
    OFFSET  DEATH_CAM = 0x70;  // bool
}

namespace enemy_ins_offsets
{
    OFFSET  LOCAL_HANDLE = 0x8;  // int
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
    OFFSET  MODEL_NAME = 0x88;  // -> char16_t[6] (0xC)
    // 0x94: int ?
    // 0x98: long = 5
    // 0x100: long = 7
    // ...
    // 0x120: void* -> ??

    OFFSET  TEAM_TYPE = 0xD8;  // int

    OFFSET  SP_EFFECT_MANAGER_ADDR = 0x278;  // void* -> SpEffectManager

    OFFSET  CURRENT_HP = 0x3E8;  // int
    OFFSET  MAX_HP = 0x3EC;  // int
}

namespace sp_effect_manager_offsets
{
    OFFSET  FIRST_SP_EFFECT_ENTRY = 0x8;  // void* -> SpEffectEntry
    // 0x10: some combination of bytes/flags (int); seems to have byte 1 at 0x10 when 40 is active (maybe whenever a non-permanent SpEffect is active?)
    OFFSET  UNK_FLOAT14 = 0x14;  // float (-1)
    OFFSET  ENEMY_INS_ADDR = 0x18;  // void* -> EnemyIns
    // more...
}

namespace sp_effect_entry_offsets
{
    OFFSET  LIFE = 0x0;  // float
    OFFSET  UNK_FLOAT04 = 0x4;  // float
    OFFSET  UNK_FLOAT08 = 0x8;  // float
    OFFSET  UNK_FLOAT0C = 0xC;  // float
    OFFSET  UNK_FLOAT10 = 0x10;  // float
    OFFSET  UNK_INT14 = 0x14;  // int (4)
    OFFSET  UNK_LONG18 = 0x18;  // long
    OFFSET  CHR_LOCAL_HANDLE = 0x20;  // int
    OFFSET  UNK_INT24 = 0x24;  // int
    OFFSET  UNK_LONG28 = 0x28;  // long
    OFFSET  SP_EFFECT_ID = 0x30;  // int
    OFFSET  UNK_INT34 = 0x34;  // int (1)
    OFFSET  UNK_PTR38 = 0x38;  // void* -> some array of floats
    OFFSET  NEXT_ENTRY_PTR = 0x40;  // void* -> SpEffectEntry
    OFFSET  PREV_ENTRY_PTR = 0x48;  // void* -> SpEffectEntry
}

namespace chr_class_base_offsets
{
    OFFSET  CHR_ASM = 0x10;  // void* -> ChrAsm
}

namespace chr_asm_offsets
{
    OFFSET  HP = 0x14;  // int
    OFFSET  BASE_MAX_HP = 0x18;  // int
    OFFSET  MP = 0x20;  // int
    OFFSET  MAX_MP = 0x24;  // int
    OFFSET  SP = 0x30;  // int
    OFFSET  MAX_SP = 0x34;  // int

    OFFSET  VITALITY = 0x40;  // int
    OFFSET  ATTUNEMENT = 0x48;  // int
    OFFSET  ENDURANCE = 0x50;  // int
    OFFSET  STRENGTH = 0x58;  // int
    OFFSET  DEXTERITY = 0x60;  // int
    OFFSET  INTELLIGENCE = 0x68;  // int
    OFFSET  FAITH = 0x70;  // int
    OFFSET  HUMANITY = 0x84;  // int
    OFFSET  RESISTANCE = 0x88;  // int

    OFFSET  SOUL_LEVEL = 0x90;  // int
    OFFSET  SOULS = 0x94;  // int
    OFFSET  LIFETIME_SOULS = 0x98;  // int

    OFFSET  CHR_TYPE = 0xA4;  // int
    OFFSET  GENDER = 0xCA;  // byte (1 = Male)
    OFFSET  SHOP_LEVEL = 0xCC;  // byte  // TODO: ?
    OFFSET  CLASS = 0xCE;  // byte
    OFFSET  PHYSIQUE = 0xCF;  // byte
    OFFSET  GIFT = 0xD0;  // byte

    OFFSET  MULTIPLAYER_COUNT = 0xD4;  // byte
    OFFSET  COOP_SUCCESS_COUNT = 0xD8;  // byte
    OFFSET  INVASION_SUCCESS_COUNT = 0xDC;  // byte

    OFFSET  WARRIOR_OF_SUNLIGHT_RANK = 0xED;  // byte
    OFFSET  DARKWRAITH_RANK = 0xEE;  // byte
    OFFSET  PATH_OF_THE_DRAGON_RANK = 0xEF;  // byte
    OFFSET  GRAVELORD_SERVANT_RANK = 0xF0;  // byte
    OFFSET  FOREST_HUNTER_RANK = 0xF1;  // byte
    OFFSET  DARKMOON_BLADE_RANK = 0xF2;  // byte
    OFFSET  CHAOS_SERVANT_RANK = 0xF3;  // byte

    OFFSET  POISON_RESISTANCE = 0x100;  // int
    OFFSET  BLEED_RESISTANCE = 0x104;  // int
    OFFSET  TOXIC_RESISTANCE = 0x108;  // int
    OFFSET  CURSE_RESISTANCE = 0x10C;  // int

    OFFSET  COVENANT_TYPE = 0x113;  // byte
    OFFSET  FACE_TYPE = 0x114;  // byte
    OFFSET  HAIR_TYPE = 0x115;  // byte
    OFFSET  HAIR_EYES_COLOR = 0x116;  // byte
    OFFSET  CURSE_LEVEL = 0x117;  // byte
    OFFSET  INVADER_TYPE = 0x118;  // byte

    OFFSET  STEAM_PROFILE_NAME_OFFSET  = 0x164;  // void* -> null-terminated UTF-16

    // NOTE: Picking up and dropping items does NOT change these indices. Seems to be determined early and only
    // incremented from there. And if you drop an item and pick it up, it still has the same index. Maybe some kind
    // of anti-cheat system... True hot-swapping probably requires these indices to change in sync with the IDs.
    OFFSET  UNKNOWN_INV_INDEX = 0x2A0;  // seems to come before first weapon... maybe 'first index'?
    OFFSET  PRIMARY_LEFT_WEAPON_INV_INDEX = 0x2A4;
    OFFSET  PRIMARY_RIGHT_WEAPON_INV_INDEX = 0x2A8;
    OFFSET  SECONDARY_LEFT_WEAPON_INV_INDEX = 0x2AC;
    OFFSET  SECONDARY_RIGHT_WEAPON_INV_INDEX = 0x2B0;
    OFFSET  PRIMARY_ARROW_INV_INDEX = 0x2B4;
    OFFSET  PRIMARY_BOLT_INV_INDEX = 0x2B8;
    OFFSET  SECONDARY_ARROW_INV_INDEX = 0x2BC;
    OFFSET  SECONDARY_BOLT_INV_INDEX = 0x2C0;
    OFFSET  HELM_INV_INDEX = 0x2C4;
    OFFSET  CHEST_INV_INDEX = 0x2C8;
    OFFSET  GAUNTLETS_INV_INDEX = 0x2CC;
    OFFSET  LEGGINGS_INV_INDEX = 0x2D0;
    OFFSET  HAIR_INV_INDEX = 0x2D4;  // (always -1)
    OFFSET  EQUIPPED_RING1_INV_INDEX = 0x2D8;
    OFFSET  EQUIPPED_RING2_INV_INDEX = 0x2DC;
    OFFSET  EQUIPPED_GOOD1_INV_INDEX = 0x2E0;
    OFFSET  EQUIPPED_GOOD2_INV_INDEX = 0x2E4;
    OFFSET  EQUIPPED_GOOD3_INV_INDEX = 0x2E8;
    OFFSET  EQUIPPED_GOOD4_INV_INDEX = 0x2EC;
    OFFSET  EQUIPPED_GOOD5_INV_INDEX = 0x2F0;

    // 0x2F4: four zero bytes
    // 0x2F8: eight zero bytes
    // 0x300: a big 8-byte pointer (0x141...)

    OFFSET  HANDED_STYLE = 0x308;  // HandedStyle
    OFFSET  CURRENT_LEFT_WEAPON_SLOT = 0x30C;  // WeaponSlot
    OFFSET  CURRENT_RIGHT_WEAPON_SLOT = 0x310;  // WeaponSlot
    OFFSET  CURRENT_LEFT_ARROW_SLOT = 0x314;  // WeaponSlot
    OFFSET  CURRENT_RIGHT_ARROW_SLOT = 0x318;  // WeaponSlot
    OFFSET  CURRENT_LEFT_BOLT_SLOT = 0x31C;  // WeaponSlot
    OFFSET  CURRENT_RIGHT_BOLT_SLOT = 0x320;  // WeaponSlot
    // These are real GameParam IDs:
    OFFSET  PRIMARY_LEFT_WEAPON = 0x324;  // int
    OFFSET  PRIMARY_RIGHT_WEAPON = 0x328;  // int
    OFFSET  SECONDARY_LEFT_WEAPON = 0x32C;  // int
    OFFSET  SECONDARY_RIGHT_WEAPON = 0x330;  // int
    OFFSET  PRIMARY_ARROW = 0x334;  // int
    OFFSET  PRIMARY_BOLT = 0x338;  // int
    OFFSET  SECONDARY_ARROW = 0x340;  // int
    OFFSET  SECONDARY_BOLT = 0x344;  // int
    OFFSET  HELM = 0x344;  // int
    OFFSET  CHEST = 0x348;  // int
    OFFSET  GAUNTLETS = 0x34C;  // int
    OFFSET  LEGGINGS = 0x350;  // int
    OFFSET  HAIR = 0x354;  // int
    OFFSET  EQUIPPED_RING1 = 0x358;  // int
    OFFSET  EQUIPPED_RING2 = 0x35C;  // int
    OFFSET  EQUIPPED_GOOD1 = 0x360;  // int
    OFFSET  EQUIPPED_GOOD2 = 0x364;  // int
    OFFSET  EQUIPPED_GOOD3 = 0x368;  // int
    OFFSET  EQUIPPED_GOOD4 = 0x36C;  // int
    OFFSET  EQUIPPED_GOOD5 = 0x370;  // int

    OFFSET  HEAD_SIZE = 0x388;  // float
    OFFSET  CHEST_SIZE = 0x38C;  // float
    OFFSET  ABDOMEN_SIZE = 0x390;  // float
    OFFSET  ARMS_SIZE = 0x394;  // float
    OFFSET  LEGS_SIZE = 0x398;  // float

    OFFSET  ATTUNEMENT_SLOT_DATA_OFFSET  = 0x418;  // void* (slot 1 is 0x18, slot 2 is 0x20, ..., slot 12 is 0x70)

    OFFSET  GESTURE_EQUIP_DATA_OFFSET  = 0x450;  // void*

    OFFSET  HAIR_COLOR_RED = 0x4C0;  // float
    OFFSET  HAIR_COLOR_GREEN = 0x4C4;  // float
    OFFSET  HAIR_COLOR_BLUE = 0x4C8;  // float
    OFFSET  PUPIL_COLOR_RED = 0x4D0;  // float
    OFFSET  PUPIL_COLOR_GREEN = 0x4D4;  // float
    OFFSET  PUPIL_COLOR_BLUE = 0x4D8;  // float

    OFFSET  FACE_DATA_ARRAY = 0x4E0;  // byte[100] (NOT a pointer)
    // NOTE: Too lazy to expose all the individual face data bytes.

    OFFSET  GESTURE_UNLOCK_DATA = 0x568;  // void*
}

#undef OFFSET
