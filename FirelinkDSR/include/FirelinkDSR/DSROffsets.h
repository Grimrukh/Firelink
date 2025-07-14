/* Offsets in structs (namespaces) in DS1R.
 *
 * We use these offsets 'lists' rather than proper mapped structs so it isn't necessary to read/write the entire struct
 * to edit a single field's value.
 */
#pragma once

namespace WORLD_CHR_MAN
{
    // 0x0: void* -> vtable in heap
    inline constexpr int WORLD_INFO_OWNER_PTR = 0x10;  // void* -> WorldInfoOwner
    inline constexpr int WORLD_AREA_CHR_ARRAY_LEN = 0x18;  // int (length of WorldAreaChrArray == 9)
    inline constexpr int WORLD_AREA_CHR_ARRAY_PTR = 0x20;  // void* -> WorldAreaChr
    inline constexpr int WORLD_BLOCK_CHR_ARRAY_LEN = 0x28;  // int (length of WorldBlockChrArray == 17)
    inline constexpr int WORLD_BLOCK_CHR_ARRAY_PTR = 0x30;  // void* -> WorldBlockChr
    // ...
    inline constexpr int PLAYER_INS = 0x68;  // void* -> PlayerIns
    inline constexpr int DEATH_CAM = 0x70;  // bool
}

namespace WORLD_BLOCK_CHR
{
    inline constexpr int WORLD_AREA_CHR_PTR = 0x10;  // void* -> WorldAreaChr (parent of block)
    // 0x48: Start of `WorldBlock`
    inline constexpr int CHR_COUNT = 0x48;  // uint32_t
    inline constexpr int PLAYER_NODE_DETAILS_ARRAY_PTR = 0x50;  // void* -> PlayerNodeDetails (array)
}

namespace PLAYER_NODE_DETAILS  // bad name IMO
{
    inline constexpr int CHR_INS_PTR = 0x0;  // void* -> ChrIns
    // ...
}

// NOTE: Both CHR_INS and PLAYER_INS have CHR_INS_NO_VTABLE at offset 0x8.
// The vtable differs for players (host/clients) and non-players (AI) and PLAYER_INS has a lot more fields afterward.

namespace CHR_INS
{
    inline constexpr int CHR_INS_VTABLE_PTR = 0x0;  // void* -> vtable in heap
    inline constexpr int CHR_INS_NO_VTABLE = 0x8;  // ChrIns_NoVtable (NOT a pointer)
    // END OF STRUCT
}

namespace PLAYER_INS
{
    inline constexpr int PLAYER_INS_VTABLE_PTR = 0x0;  // void* -> PlayerInsVTable
    inline constexpr int CHR_INS_NO_VTABLE = 0x8;  // ChrIns_NoVtable (NOT a pointer)
    // ...
    inline constexpr int PLAYER_NUMBER = 0x570;  // int (0 == PC with no guests, 1 == main PC with guests, 2+ == phantom)
    // 0x574: undefined[4]
    inline constexpr int PLAYER_GAME_DATA_PTR = 0x578;  // void* -> PlayerGameData
}

namespace CHR_INS_NO_VTABLE  // length = 0x568
{
    inline constexpr int LOCAL_HANDLE = 0x0;  // int
    // 0x08: pad[8]
    inline constexpr int CONNECTED_PLAYERS_CHR_SLOT_ARRAY = 0x10;  // void* -> ChrSlot
    // 0x18: void* -> ChrIns_field0x18 (contains some anim stuff)
    // 0x20: float?
    // 0x24: float?
    // 0x28: void* -> ChrRes
    // 0x30: void* -> ?? heap memory
    // 0x38: void* -> ?? unknown array start
    // 0x40: void* -> ?? weird
    // 0x48: void* -> ?? unknown array end
    // 0x50: int = 2
    // 0x54: int = 2
    // 0x58: void* -> ChrModel
    // 0x60: void* -> PlayerCtrl
    // 0x68: void* -> PadManipulator
    // 0x70: void* -> ChrInsProxyListener
    // 0x78: void* -> ?? heap memory
    inline constexpr int CHR_NAME = 0x80;  // char16_t[0x28]
    // 0xA8: void* -> MsbResCap
    // 0xB0: int (MsbIndex)
    // 0xB4: int?
    // 0xB8: void* -> ChrTaeAnimEvent
    inline constexpr int NPC_PARAM_ID = 0xC0;  // int
    inline constexpr int CHARA_INIT_PARAM_ID = 0xC4;  // int
    inline constexpr int CHR_NUM = 0xC8;  // uint32_t
    inline constexpr int CHR_TYPE = 0xCC;  // int
    inline constexpr int TEAM_TYPE = 0xD0;  // int
    // ...
    inline constexpr int SPECIAL_EFFECT_PTR = 0x270;  // void* -> SpecialEffect

    inline constexpr int CURRENT_HP = 0x3E0;  // int
    inline constexpr int MAX_HP = 0x3E4;  // int
    inline constexpr int CURRENT_MP = 0x3E8;  // int
    inline constexpr int MAX_MP = 0x3EC;  // int
    inline constexpr int CURRENT_SP = 0x3F0;  // int
    inline constexpr int MAX_SP = 0x3F4;  // int
}

namespace CHR_SLOT  // length = 0x38
{
    inline constexpr int PLAYER_INS_PTR = 0x0;  // void* -> PlayerIns
}

namespace SPECIAL_EFFECT
{
    inline constexpr int FIRST_SP_EFFECT_NODE_PTR = 0x8;  // void* -> SpEffectNode
    // 0x10: some combination of bytes/flags (int); seems to have byte 1 at 0x10 when 40 is active (maybe whenever a non-permanent SpEffect is active?)
    inline constexpr int UNK_FLOAT14 = 0x14;  // float (-1)
    inline constexpr int PLAYER_INS_PARENT_PTR = 0x18;  // void* -> PlayerIns
    // more...
}

namespace SP_EFFECT_NODE
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

namespace CHR_CLASS_BASE
{
    inline constexpr int PLAYER_GAME_DATA_PTR = 0x10;  // void* -> PlayerGameData
}

namespace PLAYER_GAME_DATA
{
    // 0x0: void* -> vtable (unmapped)
    // Start of `PlayerGameData_AttributeInfo` struct.
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
    // ...
    inline constexpr int STEAM_PROFILE_NAME_OFFSET = 0x164;  // void* -> null-terminated UTF-16  TODO: to field below?
    // ...
    // 0x1B4: undefined (after PlayerGameData_AttributeInfo)
    // ...
    inline constexpr int STEAM_NAME = 0x1C8;  // char16_t[0x28] (null-terminated UTF-16)

    // 0x280: Start of `EquipGameData` struct.

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

    // Start of `ChrAsm` struct.
    // 0x300: void* -> (vtable in heap)
    inline constexpr int HANDED_STYLE = 0x308;  // HandedStyle
    inline constexpr int CURRENT_LEFT_WEAPON_SLOT = 0x30C;  // 0 == Primary, 1 == Secondary
    inline constexpr int CURRENT_RIGHT_WEAPON_SLOT = 0x310;  // 0 == Primary, 1 == Secondary
    inline constexpr int CURRENT_LEFT_ARROW_SLOT = 0x314;  // 0 == Primary, 1 == Secondary
    inline constexpr int CURRENT_RIGHT_ARROW_SLOT = 0x318;  // 0 == Primary, 1 == Secondary
    inline constexpr int CURRENT_LEFT_BOLT_SLOT = 0x31C;  // 0 == Primary, 1 == Secondary
    inline constexpr int CURRENT_RIGHT_BOLT_SLOT = 0x320;  // 0 == Primary, 1 == Secondary
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

    // 0x400: start of `EquipMagicData` struct (heap vtable *, then undefined[8])
    // 0x410: void* -> parent EquipGameData
    inline constexpr int EQUIP_MAGIC_DATA_PTR = 0x418;  // void* -> EquipMagicData

    inline constexpr int GESTURE_EQUIP_DATA_PTR = 0x450;  // void*

    // 0x498: undefined[8] (after EquipGameData struct)

    // 0x4A0: Start of `FaceData` struct.
    inline constexpr int HAIR_COLOR_RED = 0x4C0;  // float
    inline constexpr int HAIR_COLOR_GREEN = 0x4C4;  // float
    inline constexpr int HAIR_COLOR_BLUE = 0x4C8;  // float
    inline constexpr int PUPIL_COLOR_RED = 0x4D0;  // float
    inline constexpr int PUPIL_COLOR_GREEN = 0x4D4;  // float
    inline constexpr int PUPIL_COLOR_BLUE = 0x4D8;  // float

    inline constexpr int FACE_DATA_ARRAY = 0x4E0;  // byte[100] (NOT a pointer)
    // NOTE: Too lazy to expose all the individual face data bytes.

    inline constexpr int GESTURE_GAME_DATA_PTR = 0x568;  // void* -> GestureGameData
}

namespace EQUIP_MAGIC_DATA
{
    // 0x0: void* -> vtable in heap (unmapped)
    // 0x8: undefined[8]
    // 0x10: void* -> parent EquipGameData
    inline constexpr int ATTUNEMENT_SLOT_ARRAY_12 = 0x18;  // (int ID, int count)[12]
    inline constexpr int EQUIPPED_ATTUNEMENT_SLOT = 0x78;  // int (1-12, 1-indexed) TODO: confirm 1-indexed
}