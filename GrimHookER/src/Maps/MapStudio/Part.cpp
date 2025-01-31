#include <cstdint>

#include "GrimHookER/Maps/MapStudio/Part.h"
#include "GrimHook/BinaryReadWrite.h"
#include "GrimHook/BinaryValidation.h"
#include "GrimHook/Collections.h"
#include "GrimHook/MemoryUtils.h"
#include "GrimHookER/Maps/MapStudio/Model.h"
#include "GrimHookER/Maps/MapStudio/MSB.h"
#include "GrimHookER/Maps/MapStudio/MSBFormatError.h"
#include "GrimHookER/Maps/MapStudio/Region.h"

using namespace std;
using namespace GrimHook;
using namespace GrimHook::BinaryReadWrite;
using namespace GrimHook::BinaryValidation;
using namespace GrimHookER::Maps;
using namespace GrimHookER::Maps::MapStudio;

// MACRO: Check that unused struct offset is zero.
#define ASSERT_ZERO_STRUCT_OFFSET(name, offset) \
    if ((offset) != 0) { \
        throw MSBFormatError("Expected struct " #name " offset to be 0, but got " + to_string(offset)); \
    }

// MACRO: Check that header offset is non-zero, then read struct.
#define READ_STRUCT(name) \
    if (header.name##Offset == 0) { \
        throw MSBFormatError("Expected struct " #name " offset to be non-zero, but got 0"); \
    } \
    stream.seekg(entryStart + header.name##Offset); \
    (name).Read(stream);

// MACRO: Check supertype data offset is non-zero, then read supertype data.
#define READ_SUPERTYPE_DATA() \
    if (header.supertypeDataOffset == 0) { \
        throw MSBFormatError("Expected supertype data offset to be non-zero, but got 0"); \
    } \
    stream.seekg(entryStart + header.supertypeDataOffset); \
    ReadSupertypeData(stream);

// MACRO: Check subtype data offset is non-zero (the case for all subtypes), then read subtype data.
#define READ_SUBTYPE_DATA() \
    if (header.subtypeDataOffset == 0) { \
        throw MSBFormatError("Expected subtype data offset to be non-zero, but got 0"); \
    } \
    stream.seekg(entryStart + header.subtypeDataOffset); \
    if (!DeserializeSubtypeData(stream)) { \
        throw MSBFormatError("Failed to read subtype data for part " + m_name); \
    }

// MACRO: Record struct offset in header, then write struct.
#define WRITE_STRUCT(name) \
    header.name##Offset = stream.tellp() - entryStart; \
    (name).Write(stream);

// MACRO: Record supertype data offset in header, then write supertype data.
#define WRITE_SUPERTYPE_DATA() \
    header.supertypeDataOffset = stream.tellp() - entryStart; \
    WriteSupertypeData(stream);

// MACRO: Record subtype data offset in header, then write subtype data (passing in supertype/subtype indices).
#define WRITE_SUBTYPE_DATA() \
    header.subtypeDataOffset = stream.tellp() - entryStart; \
    SerializeSubtypeData(stream, supertypeIndex, subtypeIndex);


struct MapStudio::PartHeader
{
    int64_t nameOffset;            // Offset to the part's name
    int32_t modelInstanceId;       // Unique instance ID for the model
    PartType subtype;              // Part type
    int32_t subtypeIndex;          // Index of the subtype
    int32_t modelIndex;            // Model index (MODEL_PARAM_ST reference)
    int64_t sibPathOffset;         // Offset to the SIB path
    Vector3 translate;             // Translation vector
    Vector3 rotate;                // Rotation vector
    Vector3 scale;                 // Scale vector
    int32_t unk44;                 // Unknown field at offset 44
    int32_t eventLayer;            // MapStudio/ceremony event layer
    int32_t _zero;                 // 0
    int64_t drawInfo1Offset;       // Offset for DrawInfo1 data
    int64_t drawInfo2Offset;       // Offset for DrawInfo2 data
    int64_t supertypeDataOffset;   // Offset for Part supertype data
    int64_t subtypeDataOffset;     // Offset for Part subtype data
    int64_t gparamOffset;          // Offset for GParam data
    int64_t sceneGparamOffset;     // Offset for SceneGParam data
    int64_t grassConfigOffset;     // Offset for GrassConfig data
    int64_t unkStruct8Offset;      // Offset for unknown data struct 8
    int64_t unkStruct9Offset;      // Offset for unknown data struct 9
    int64_t tileLoadConfigOffset;  // Offset for TileLoad data
    int64_t unkStruct11Offset;     // Offset for unknown data struct 11
    array<uint8_t, 24> _pad1;      // Padding (three unused offsets?)

    void Validate() const
    {
        AssertValue("PartHeader._zero", 0, _zero);
        // Validate guaranteed offsets. Note that all subtypes have subtype data, even if just padding.
        AssertNotValue("PartHeader.nameOffset", 0, nameOffset);
        AssertNotValue("PartHeader.sibPathOffset", 0, sibPathOffset);
        AssertNotValue("PartHeader.supertypeDataOffset", 0, supertypeDataOffset);
        AssertNotValue("PartHeader.subtypeDataOffset", 0, subtypeDataOffset);
        AssertPadding("PartHeader", _pad1);
    }
};


void DrawInfo1::Read(ifstream& stream)
{
    displayGroups = GroupBitSet<256>(stream);
    drawGroups = GroupBitSet<256>(stream);
    collisionMask = GroupBitSet<1024>(stream);
    condition1A = ReadValue<uint8_t>(stream);
    condition1B = ReadValue<uint8_t>(stream);
    unkC2 = ReadValue<uint8_t>(stream);
    unkC3 = ReadValue<uint8_t>(stream);
    unkC4 = ReadValue<int16_t>(stream);
    unkC6 = ReadValue<int16_t>(stream);
    AssertReadValues<uint8_t, 0xC0>(stream, 0, "DrawInfoData1 padding");
}


void DrawInfo1::Write(ofstream& stream) const
{
    displayGroups.Write(stream);
    drawGroups.Write(stream);
    collisionMask.Write(stream);
    WriteValue(stream, condition1A);
    WriteValue(stream, condition1B);
    WriteValue(stream, unkC2);
    WriteValue(stream, unkC3);
    WriteValue(stream, unkC4);
    WriteValue(stream, unkC6);
    WritePadBytes(stream, 0xC0);
}


void DrawInfo2::Read(ifstream& stream)
{
    condition2 = ReadValue<int32_t>(stream);
    displayGroups2 = GroupBitSet<256>(stream);
    unk24 = ReadValue<int16_t>(stream);
    unk26 = ReadValue<int16_t>(stream);
    AssertReadValues<uint8_t, 0x20>(stream, 0, "DrawInfoData2 padding");
}


void DrawInfo2::Write(ofstream& stream) const
{
    WriteValue(stream, condition2);
    displayGroups2.Write(stream);
    WriteValue(stream, unk24);
    WriteValue(stream, unk26);
    WritePadBytes(stream, 0x20);
}


struct PartDataStruct
{
    uint32_t entityId;
    uint8_t unk04;
    padding<3> _pad1;  // includes former "Lantern ID"
    int8_t lodId;
    uint8_t unk09;
    int8_t isPointLightShadowSource; // Always 0 or -1?
    uint8_t unk0B;
    bool isShadowSource;
    uint8_t isStaticShadowSource;   // TODO: bool?
    uint8_t isCascade3ShadowSource; // TODO: bool?
    uint8_t unk0F;
    uint8_t unk10;
    bool isShadowDestination;
    bool isShadowOnly; // Always 0?
    bool drawByReflectCam;
    bool drawOnlyReflectCam;
    uint8_t enableOnAboveShadow;
    bool disablePointLightEffect;
    uint8_t unk17;
    int32_t unk18;
    array<uint32_t, 8> entityGroupIds;
    int16_t unk3C;
    int16_t unk3E;

    void Validate() const
    {
        AssertPadding("PartDataStruct", _pad1);
    }
};


void GParam::Read(ifstream& stream)
{
    lightSetId = ReadValue<int32_t>(stream);
    fogId = ReadValue<int32_t>(stream);
    lightScatteringId = ReadValue<int32_t>(stream);
    environmentMapId = ReadValue<int32_t>(stream);
    AssertReadValues<uint8_t, 16>(stream, 0, "GParamData padding");
}


void GParam::Write(ofstream& stream) const
{
    WriteValue(stream, lightSetId);
    WriteValue(stream, fogId);
    WriteValue(stream, lightScatteringId);
    WriteValue(stream, environmentMapId);
    WritePadBytes(stream, 16);
}


void SceneGParam::Read(ifstream& stream)
{
    AssertReadValues<uint8_t, 0x10>(stream, 0, "SceneGParamData padding 0x10");
    transitionTime = ReadValue<float>(stream);
    AssertReadValue<int32_t>(stream, 0, "SceneGParamData[0x20]");

    unk18 = ReadValue<int8_t>(stream);
    unk19 = ReadValue<int8_t>(stream);
    unk1A = ReadValue<int8_t>(stream);
    unk1B = ReadValue<int8_t>(stream);

    unk1C = ReadValue<int8_t>(stream);
    unk1D = ReadValue<int8_t>(stream);
    AssertReadValues<uint8_t, 2>(stream, 0, "SceneGParamData padding 0x2");

    unk20 = ReadValue<int8_t>(stream);
    unk21 = ReadValue<int8_t>(stream);
    AssertReadValues<uint8_t, 2>(stream, 0, "SceneGParamData padding 0x2");

    AssertReadValues<uint8_t, 0x44>(stream, 0, "SceneGParamData padding 0x44");
}


void SceneGParam::Write(ofstream& stream) const
{
    WritePadBytes(stream, 0x10);
    WriteValue(stream, transitionTime);
    WriteValue(stream, 0);

    WriteValue(stream, unk18);
    WriteValue(stream, unk19);
    WriteValue(stream, unk1A);
    WriteValue(stream, unk1B);

    WriteValue(stream, unk1C);
    WriteValue(stream, unk1D);
    WritePadBytes(stream, 2);

    WriteValue(stream, unk20);
    WriteValue(stream, unk21);
    WritePadBytes(stream, 2);

    WritePadBytes(stream, 0x44);
}


void GrassConfig::Read(ifstream& stream)
{
    unk00 = ReadValue<int32_t>(stream);
    unk04 = ReadValue<int32_t>(stream);
    unk08 = ReadValue<int32_t>(stream);
    unk0C = ReadValue<int32_t>(stream);
    unk10 = ReadValue<int32_t>(stream);
    unk14 = ReadValue<int32_t>(stream);
    unk18 = ReadValue<int32_t>(stream);
    AssertReadValue<int32_t>(stream, 0, "UnkPartStruct7 zero");
}


void GrassConfig::Write(ofstream& stream) const
{
    WriteValue(stream, unk00);
    WriteValue(stream, unk04);
    WriteValue(stream, unk08);
    WriteValue(stream, unk0C);
    WriteValue(stream, unk10);
    WriteValue(stream, unk14);
    WriteValue(stream, unk18);
    WriteValue(stream, 0);
}


void UnkPartStruct8::Read(ifstream& stream)
{
    unk00 = ReadValue<int32_t>(stream);
    if (unk00 != 0 && unk00 != 1)
    {
        throw MSBFormatError("UnkPartStruct8: Expected unk00 to be 0 or 1, but got " + to_string(unk00));
    }
    AssertReadValues<uint8_t, 0x1C>(stream, 0, "UnkPartStruct8 padding 0x1C");
}


void UnkPartStruct8::Write(ofstream& stream) const
{
    WriteValue<int32_t>(stream, unk00);
    WritePadBytes(stream, 0x1C);
}


void UnkPartStruct9::Read(ifstream& stream)
{
    unk00 = ReadValue<int32_t>(stream);
    AssertReadValues<uint8_t, 0x1C>(stream, 0, "UnkPartStruct9 padding 0x1C");
}


void UnkPartStruct9::Write(ofstream& stream) const
{
    WriteValue<int32_t>(stream, unk00);
    WritePadBytes(stream, 0x1C);
}


void TileLoadConfig::Read(ifstream& stream)
{
    mapId = ReadValue<int32_t>(stream);
    unk04 = ReadValue<int32_t>(stream);
    AssertReadValue<int32_t>(stream, 0, "UnkPartStruct10 zero");
    unk0C = ReadValue<int32_t>(stream);
    unk10 = ReadValue<int32_t>(stream);
    if (unk10 != 0 && unk10 != 1)
    {
        throw MSBFormatError("UnkPartStruct10: Expected unk10 to be 0 or 1, but got " + to_string(unk10));
    }
    unk14 = ReadValue<int32_t>(stream);
    AssertReadValues<uint8_t, 0x8>(stream, 0, "UnkPartStruct10 padding 0x8");
}


void TileLoadConfig::Write(ofstream& stream) const
{
    WriteValue(stream, mapId);
    WriteValue(stream, unk04);
    WriteValue(stream, 0);
    WriteValue(stream, unk0C);
    WriteValue(stream, unk10);
    WriteValue(stream, unk14);
    WritePadBytes(stream, 0x8);
}


void UnkPartStruct11::Read(ifstream& stream)
{
    unk00 = ReadValue<int32_t>(stream);
    unk04 = ReadValue<int32_t>(stream);
    AssertReadValues<uint8_t, 0x18>(stream, 0, "UnkPartStruct11 padding 0x18");
}


void UnkPartStruct11::Write(ofstream& stream) const
{
    WriteValue(stream, unk00);
    WriteValue(stream, unk04);
    WritePadBytes(stream, 0x18);
}


void Part::Deserialize(ifstream& stream)
{
    const streampos start = stream.tellg();

    const auto header = ReadValidatedStruct<PartHeader>(stream);

    stream.seekg(start + header.nameOffset);
    const u16string nameWide = ReadUTF16String(stream);
    m_name = UTF16ToUTF8(nameWide);

    stream.seekg(start + header.sibPathOffset);
    const u16string sibPathWide = ReadUTF16String(stream);
    sibPath = UTF16ToUTF8(sibPathWide);

    modelIndex = header.modelIndex;
    modelInstanceId = header.modelInstanceId;
    translate = header.translate;
    rotate = header.rotate;
    scale = header.scale;
    unk44 = header.unk44;
    eventLayer = header.eventLayer;

    DeserializeStructs(stream, header, start);
}


void Part::Serialize(ofstream& stream, const int supertypeIndex, const int subtypeIndex) const
{
    const streampos start = stream.tellp();

    Reserver reserver(stream, true);

    auto header = PartHeader
    {
        .modelInstanceId = modelInstanceId,
        .subtype = GetType(),
        .subtypeIndex = subtypeIndex,
        .modelIndex = modelIndex,
        .translate = translate,
        .rotate = rotate,
        .scale = scale,
        .unk44 = unk44,
        .eventLayer = eventLayer,
    };
    reserver.ReserveValidatedStruct("PartHeader", sizeof(PartHeader));

    // TODO: Funky combined string padding rules?
    header.nameOffset = stream.tellp() - start;
    WriteUTF16String(stream, m_name);
    header.sibPathOffset = stream.tellp() - start;
    WriteUTF16String(stream, sibPath);
    AlignStream(stream, 8);

    // No additional alignment necessary before or after structs.
    SerializeStructs(stream, header, start, supertypeIndex, subtypeIndex);

    reserver.FillValidatedStruct("PartHeader", header);

    reserver.Finish();
}


void Part::ReadSupertypeData(ifstream& stream)
{
    auto supertypeData = ReadValidatedStruct<PartDataStruct>(stream);
    m_entityId = supertypeData.entityId;
    unk04 = supertypeData.unk04;
    lodId = supertypeData.lodId;
    unk09 = supertypeData.unk09;
    isPointLightShadowSource = supertypeData.isPointLightShadowSource;
    unk0B = supertypeData.unk0B;
    isShadowSource = supertypeData.isShadowSource;
    isStaticShadowSource = supertypeData.isStaticShadowSource;
    isCascade3ShadowSource = supertypeData.isCascade3ShadowSource;
    unk0F = supertypeData.unk0F;
    unk10 = supertypeData.unk10;
    isShadowDestination = supertypeData.isShadowDestination;
    isShadowOnly = supertypeData.isShadowOnly;
    drawByReflectCam = supertypeData.drawByReflectCam;
    drawOnlyReflectCam = supertypeData.drawOnlyReflectCam;
    enableOnAboveShadow = supertypeData.enableOnAboveShadow;
    disablePointLightEffect = supertypeData.disablePointLightEffect;
    unk17 = supertypeData.unk17;
    unk18 = supertypeData.unk18;
    ranges::copy(supertypeData.entityGroupIds, entityGroupIds.begin());
    unk3C = supertypeData.unk3C;
    unk3E = supertypeData.unk3E;
}


void Part::WriteSupertypeData(ofstream& stream) const
{
    auto supertypeData = PartDataStruct
    {
        .entityId = m_entityId,
        .unk04 = unk04,
        .lodId = lodId,
        .unk09 = unk09,
        .isPointLightShadowSource = isPointLightShadowSource,
        .unk0B = unk0B,
        .isShadowSource = isShadowSource,
        .isStaticShadowSource = isStaticShadowSource,
        .isCascade3ShadowSource = isCascade3ShadowSource,
        .unk0F = unk0F,
        .unk10 = unk10,
        .isShadowDestination = isShadowDestination,
        .isShadowOnly = isShadowOnly,
        .drawByReflectCam = drawByReflectCam,
        .drawOnlyReflectCam = drawOnlyReflectCam,
        .enableOnAboveShadow = enableOnAboveShadow,
        .disablePointLightEffect = disablePointLightEffect,
        .unk17 = unk17,
        .unk18 = unk18,
        .unk3C = unk3C,
        .unk3E = unk3E,
    };
    ranges::copy(entityGroupIds, supertypeData.entityGroupIds.begin());

    WriteValidatedStruct(stream, supertypeData);
}

void Part::OnReferencedEntryDestroy(const Entry* destroyedEntry)
{
    EntityEntry::OnReferencedEntryDestroy(destroyedEntry);
    if (const auto destroyedModel = dynamic_cast<const Model*>(destroyedEntry))
        model.OnReferencedEntryDestroy(destroyedModel);
}


void Part::DeserializeEntryReferences(const vector<Model*>& models, const vector<Part*>& parts, const vector<Region*>& regions)
{
    model.SetFromIndex(models, modelIndex);
}


void Part::SerializeEntryIndices(const vector<Model*>& models, const vector<Part*>& parts, const vector<Region*>& regions)
{
    modelIndex = model.ToIndex(this, models);
}


void MapPiecePart::DeserializeStructs(ifstream& stream, const PartHeader& header, const streampos& entryStart)
{
    READ_STRUCT(drawInfo1);
    ASSERT_ZERO_STRUCT_OFFSET("DrawInfo2", header.drawInfo2Offset);
    READ_SUPERTYPE_DATA();
    READ_SUBTYPE_DATA();
    READ_STRUCT(gparam);
    ASSERT_ZERO_STRUCT_OFFSET("SceneGParam", header.sceneGparamOffset);
    READ_STRUCT(grassConfig);
    READ_STRUCT(unkStruct8);
    READ_STRUCT(unkStruct9);
    READ_STRUCT(tileLoadConfig);
    READ_STRUCT(unkStruct11);
}


void MapPiecePart::SerializeStructs(ofstream& stream, PartHeader& header, const streampos& entryStart,
                                     const int& supertypeIndex, const int& subtypeIndex) const
                                     {

    WRITE_STRUCT(drawInfo1);
    header.drawInfo2Offset = 0;  // unused
    WRITE_SUPERTYPE_DATA();
    WRITE_SUBTYPE_DATA();
    WRITE_STRUCT(gparam);
    header.sceneGparamOffset = 0;  // unused
    WRITE_STRUCT(grassConfig);
    WRITE_STRUCT(unkStruct8);
    WRITE_STRUCT(unkStruct9);
    WRITE_STRUCT(tileLoadConfig);
    WRITE_STRUCT(unkStruct11);
}


bool MapPiecePart::DeserializeSubtypeData(ifstream& stream)
{
    // Just padding.
    AssertReadValues<uint8_t, 8>(stream, 0, "MapPiece subtype data");
    return true;
}


bool MapPiecePart::SerializeSubtypeData(ofstream& stream, int supertypeIndex, int subtypeIndex) const
{
    WritePadBytes(stream, 8);
    return true;
}


void CharacterPart::DeserializeStructs(ifstream& stream, const PartHeader& header, const streampos& entryStart)
{
    READ_STRUCT(drawInfo1);
    ASSERT_ZERO_STRUCT_OFFSET("DrawInfo2", header.drawInfo2Offset);
    READ_SUPERTYPE_DATA();
    READ_SUBTYPE_DATA();
    READ_STRUCT(gparam);
    ASSERT_ZERO_STRUCT_OFFSET("SceneGParam", header.sceneGparamOffset);
    ASSERT_ZERO_STRUCT_OFFSET("GrassConfig", header.grassConfigOffset);
    READ_STRUCT(unkStruct8);
    ASSERT_ZERO_STRUCT_OFFSET("UnkStruct9", header.unkStruct9Offset);
    READ_STRUCT(tileLoadConfig);
    ASSERT_ZERO_STRUCT_OFFSET("UnkStruct11", header.unkStruct11Offset);
}


void CharacterPart::SerializeStructs(
    ofstream& stream,
    PartHeader& header,
    const streampos& entryStart,
    const int& supertypeIndex,
    const int& subtypeIndex) const
{
    WRITE_STRUCT(drawInfo1);
    header.drawInfo2Offset = 0;  // unused
    WRITE_SUPERTYPE_DATA();
    WRITE_SUBTYPE_DATA();
    WRITE_STRUCT(gparam);
    header.sceneGparamOffset = 0;  // unused
    header.grassConfigOffset = 0;  // unused
    WRITE_STRUCT(unkStruct8);
    header.unkStruct9Offset = 0;  // unused
    WRITE_STRUCT(tileLoadConfig);
    header.unkStruct11Offset = 0;  // unused
}


struct CharacterSubtypeData
{
    padding<8> _pad1;
    int aiId;
    int characterId;
    int talkId;
    uint8_t _zero;
    bool sUnk15;
    int16_t platoonId;
    int playerId;
    int drawParentIndex;
    int16_t patrolRouteEventIndex;
    int16_t _zero2;
    int sUnk24;
    int sUnk28;
    int activateConditionParamId;
    int _zero3;
    int sUnk34;
    int backAwayEventAnimationId;
    int sUnk3C;
    array<int, 4> specialEffectSetParamIds;
    padding<40> _pad2;
    int64_t _x80;
    int _zero4;
    float sUnk84;
    // Five instances of four-short pattern [-1, -1 , -1, 0xA]:
    array<int16_t, 4> _pad3a;
    array<int16_t, 4> _pad3b;
    array<int16_t, 4> _pad3c;
    array<int16_t, 4> _pad3d;
    array<int16_t, 4> _pad3e;
    padding<0x10> _pad4;

    void Validate() const
    {
        AssertPadding("CharacterSubtypeData", _pad1, _pad2, _pad4);
        AssertValue("CharacterSubtypeData", 0, _zero);
        AssertValue("CharacterSubtypeData", 0, _zero3);
        AssertValue("CharacterSubtypeData", 0, _zero4);
        AssertValue("CharacterSubtypeData", 0x80, _x80);

        constexpr array<int16_t, 4> pad3 = {-1, -1, -1, 0xA};
        ValidateArray("CharacterSubtypeData", pad3, _pad3a);
        ValidateArray("CharacterSubtypeData", pad3, _pad3b);
        ValidateArray("CharacterSubtypeData", pad3, _pad3c);
        ValidateArray("CharacterSubtypeData", pad3, _pad3d);
        ValidateArray("CharacterSubtypeData", pad3, _pad3e);
    }
};


bool CharacterPart::DeserializeSubtypeData(ifstream& stream)
{
    const auto subtypeData = ReadValidatedStruct<CharacterSubtypeData>(stream);
    aiId = subtypeData.aiId;
    characterId = subtypeData.characterId;
    talkId = subtypeData.talkId;
    sUnk15 = subtypeData.sUnk15;
    platoonId = subtypeData.platoonId;
    playerId = subtypeData.playerId;
    drawParent = nullptr;
    drawParentIndex = subtypeData.drawParentIndex;
    patrolRouteEvent = nullptr;
    patrolRouteEventIndex = subtypeData.patrolRouteEventIndex;
    sUnk24 = subtypeData.sUnk24;
    sUnk28 = subtypeData.sUnk28;
    activateConditionParamId = subtypeData.activateConditionParamId;
    sUnk34 = subtypeData.sUnk34;
    backAwayEventAnimationId = subtypeData.backAwayEventAnimationId;
    sUnk3C = subtypeData.sUnk3C;
    specialEffectSetParamIds = subtypeData.specialEffectSetParamIds;
    sUnk84 = subtypeData.sUnk84;
    return true;
}


bool CharacterPart::SerializeSubtypeData(ofstream& stream, int supertypeIndex, int subtypeIndex) const
{
    const auto subtypeData = CharacterSubtypeData
    {
        .aiId = aiId,
        .characterId = characterId,
        .talkId = talkId,
        .sUnk15 = sUnk15,
        .platoonId = platoonId,
        .playerId = playerId,
        .drawParentIndex = drawParentIndex,
        .patrolRouteEventIndex = patrolRouteEventIndex,
        .sUnk24 = sUnk24,
        .sUnk28 = sUnk28,
        .activateConditionParamId = activateConditionParamId,
        .sUnk34 = sUnk34,
        .backAwayEventAnimationId = backAwayEventAnimationId,
        .sUnk3C = sUnk3C,
        .specialEffectSetParamIds = specialEffectSetParamIds,
        ._x80 = 0x80,
        .sUnk84 = sUnk84,
        ._pad3a = {-1, -1, -1, 0xA},
        ._pad3b = {-1, -1, -1, 0xA},
        ._pad3c = {-1, -1, -1, 0xA},
        ._pad3d = {-1, -1, -1, 0xA},
        ._pad3e = {-1, -1, -1, 0xA},
    };
    WriteValidatedStruct(stream, subtypeData);
    return true;
}


void CharacterPart::DeserializeEntryReferences(const vector<Model*>& models, const vector<Part*>& parts, const vector<Region*>& regions)
{
    Part::DeserializeEntryReferences(models, parts, regions);
    drawParent.SetFromIndex(parts, drawParentIndex);
}


void CharacterPart::SerializeEntryIndices(const vector<Model*>& models, const vector<Part*>& parts, const vector<Region*>& regions)
{
    Part::SerializeEntryIndices(models, parts, regions);
    drawParentIndex = drawParent.ToIndex(this, parts);
}


void CharacterPart::DeserializePatrolRouteEventReference(const vector<PatrolRouteEvent*>& patrolRouteEvents)
{
    patrolRouteEvent.SetFromIndex16(patrolRouteEvents, patrolRouteEventIndex);
}


void CharacterPart::SerializePatrolRouteEventIndex(const vector<PatrolRouteEvent*>& patrolRouteEvents)
{
    patrolRouteEventIndex = patrolRouteEvent.ToIndex16(this, patrolRouteEvents);
}


void PlayerStartPart::DeserializeStructs(ifstream& stream, const PartHeader& header, const streampos& entryStart)
{
    READ_STRUCT(drawInfo1);
    ASSERT_ZERO_STRUCT_OFFSET("DrawInfo2", header.drawInfo2Offset);
    READ_SUPERTYPE_DATA();
    READ_SUBTYPE_DATA();
    ASSERT_ZERO_STRUCT_OFFSET("GParam", header.gparamOffset);
    ASSERT_ZERO_STRUCT_OFFSET("SceneGParam", header.sceneGparamOffset);
    ASSERT_ZERO_STRUCT_OFFSET("GrassConfig", header.grassConfigOffset);
    READ_STRUCT(unkStruct8);
    ASSERT_ZERO_STRUCT_OFFSET("UnkStruct9", header.unkStruct9Offset);
    READ_STRUCT(tileLoadConfig);
    ASSERT_ZERO_STRUCT_OFFSET("UnkStruct11", header.unkStruct11Offset);
}


void PlayerStartPart::SerializeStructs(ofstream& stream, PartHeader& header, const streampos& entryStart,
                                        const int& supertypeIndex, const int& subtypeIndex) const
                                        {

    WRITE_STRUCT(drawInfo1);
    header.drawInfo2Offset = 0;  // unused
    WRITE_SUPERTYPE_DATA();
    WRITE_SUBTYPE_DATA();
    header.gparamOffset = 0;  // unused
    header.sceneGparamOffset = 0;  // unused
    header.grassConfigOffset = 0;  // unused
    WRITE_STRUCT(unkStruct8);
    header.unkStruct9Offset = 0;  // unused
    WRITE_STRUCT(tileLoadConfig);
    header.unkStruct11Offset = 0;  // unused
}


bool PlayerStartPart::DeserializeSubtypeData(ifstream& stream)
{
    // Not bothering with a struct.
    sUnk00 = ReadValue<int32_t>(stream);
    AssertReadValues<uint8_t, 0xC>(stream, 0, "PlayerStart subtype data padding");
    return true;
}


bool PlayerStartPart::SerializeSubtypeData(ofstream& stream, int supertypeIndex, int subtypeIndex) const
{
    WriteValue(stream, sUnk00);
    WritePadBytes(stream, 0xC);
    return true;
}


void CollisionPart::DeserializeStructs(ifstream& stream, const PartHeader& header, const streampos& entryStart)
{
    READ_STRUCT(drawInfo1);
    READ_STRUCT(drawInfo2);
    READ_SUPERTYPE_DATA();
    READ_SUBTYPE_DATA();
    READ_STRUCT(gparam);
    READ_STRUCT(sceneGparam);
    ASSERT_ZERO_STRUCT_OFFSET("GrassConfig", header.grassConfigOffset);
    READ_STRUCT(unkStruct8);
    ASSERT_ZERO_STRUCT_OFFSET("UnkStruct9", header.unkStruct9Offset);
    READ_STRUCT(tileLoadConfig);
    READ_STRUCT(unkStruct11);
}


void CollisionPart::SerializeStructs(ofstream& stream, PartHeader& header, const streampos& entryStart, const int& supertypeIndex, const int& subtypeIndex) const
{
    WRITE_STRUCT(drawInfo1);
    WRITE_STRUCT(drawInfo2);
    WRITE_SUPERTYPE_DATA();
    WRITE_SUBTYPE_DATA();
    WRITE_STRUCT(gparam);
    WRITE_STRUCT(sceneGparam);
    header.grassConfigOffset = 0;  // unused
    WRITE_STRUCT(unkStruct8);
    header.unkStruct9Offset = 0;  // unused
    WRITE_STRUCT(tileLoadConfig);
    WRITE_STRUCT(unkStruct11);
}


struct CollisionPartSubtypeData
{
    uint8_t hitFilterId;
    uint8_t sUnk01;
    uint8_t sUnk02;
    uint8_t sUnk03;
    float sUnk04;
    padding<12> _pad1;
    float sUnk14;
    int sUnk18;
    int sUnk1C;
    int playRegionId;
    int16_t sUnk24;
    int16_t sUnk26;  // 0 or 1
    int _zero;
    int _minusOne;
    int sUnk30;
    uint8_t sUnk34;
    uint8_t sUnk35;
    bool disableTorrent;
    uint8_t _zero2;
    int _minusOne2;
    int16_t sUnk3C;
    int16_t sUnk3E;
    float sUnk40;
    int _zero3;
    uint32_t enableFastTravelFlagId;
    int16_t sUnk4C;  // 0 or 1
    int16_t sUnk4E;

    void Validate() const
    {
        AssertPadding("CollisionPartSubtypeData", _pad1);
        AssertValue("CollisionPartSubtypeData._zero", 0, _zero);
        AssertValue("CollisionPartSubtypeData._zero2", 0, _zero2);
        AssertValue("CollisionPartSubtypeData._zero3", 0, _zero3);
        AssertValue("CollisionPartSubtypeData._minusOne", -1, _minusOne);
        AssertValue("CollisionPartSubtypeData._minusOne2", -1, _minusOne2);

        if (sUnk26 != 0 && sUnk26 != 1)
        {
            throw MSBFormatError("CollisionPartSubtypeData: Expected sUnk26 to be 0 or 1, but got " + to_string(sUnk26));
        }
        if (sUnk4C != 0 && sUnk4C != 1)
        {
            throw MSBFormatError("CollisionPartSubtypeData: Expected sUnk4C to be 0 or 1, but got " + to_string(sUnk4C));
        }
    }
};


bool CollisionPart::DeserializeSubtypeData(ifstream& stream)
{
    const auto subtypeData = ReadValidatedStruct<CollisionPartSubtypeData>(stream);
    hitFilterId = subtypeData.hitFilterId;
    sUnk01 = subtypeData.sUnk01;
    sUnk02 = subtypeData.sUnk02;
    sUnk03 = subtypeData.sUnk03;
    sUnk04 = subtypeData.sUnk04;
    sUnk14 = subtypeData.sUnk14;
    sUnk18 = subtypeData.sUnk18;
    sUnk1C = subtypeData.sUnk1C;
    playRegionId = subtypeData.playRegionId;
    sUnk24 = subtypeData.sUnk24;
    sUnk26 = subtypeData.sUnk26;
    sUnk30 = subtypeData.sUnk30;
    sUnk34 = subtypeData.sUnk34;
    sUnk35 = subtypeData.sUnk35;
    disableTorrent = subtypeData.disableTorrent;
    sUnk3C = subtypeData.sUnk3C;
    sUnk3E = subtypeData.sUnk3E;
    sUnk40 = subtypeData.sUnk40;
    enableFastTravelFlagId = subtypeData.enableFastTravelFlagId;
    sUnk4C = subtypeData.sUnk4C;
    sUnk4E = subtypeData.sUnk4E;
    return true;
}


bool CollisionPart::SerializeSubtypeData(ofstream& stream, int supertypeIndex, int subtypeIndex) const
{
    const auto subtypeData = CollisionPartSubtypeData
    {
        .hitFilterId = hitFilterId,
        .sUnk01 = sUnk01,
        .sUnk02 = sUnk02,
        .sUnk03 = sUnk03,
        .sUnk04 = sUnk04,
        .sUnk14 = sUnk14,
        .sUnk18 = sUnk18,
        .sUnk1C = sUnk1C,
        .playRegionId = playRegionId,
        .sUnk24 = sUnk24,
        .sUnk26 = sUnk26,
        ._minusOne = -1,
        .sUnk30 = sUnk30,
        .sUnk34 = sUnk34,
        .sUnk35 = sUnk35,
        .disableTorrent = disableTorrent,
        ._minusOne2 = -1,
        .sUnk3C = sUnk3C,
        .sUnk3E = sUnk3E,
        .sUnk40 = sUnk40,
        .enableFastTravelFlagId = enableFastTravelFlagId,
        .sUnk4C = sUnk4C,
        .sUnk4E = sUnk4E,
    };
    WriteValidatedStruct(stream, subtypeData);
    return true;
}


void DummyAssetPart::DeserializeStructs(ifstream& stream, const PartHeader& header, const streampos& entryStart)
{
    READ_STRUCT(drawInfo1);
    ASSERT_ZERO_STRUCT_OFFSET("DrawInfo2", header.drawInfo2Offset);
    READ_SUPERTYPE_DATA();
    READ_SUBTYPE_DATA();
    READ_STRUCT(gparam);
    ASSERT_ZERO_STRUCT_OFFSET("SceneGParam", header.sceneGparamOffset);
    ASSERT_ZERO_STRUCT_OFFSET("GrassConfig", header.grassConfigOffset);
    READ_STRUCT(unkStruct8);
    ASSERT_ZERO_STRUCT_OFFSET("UnkStruct9", header.unkStruct9Offset);
    READ_STRUCT(tileLoadConfig);
    ASSERT_ZERO_STRUCT_OFFSET("UnkStruct11", header.unkStruct11Offset);
}


void DummyAssetPart::SerializeStructs(ofstream& stream, PartHeader& header, const streampos& entryStart, const int& supertypeIndex, const int& subtypeIndex) const
{
    WRITE_STRUCT(drawInfo1);
    header.drawInfo2Offset = 0;  // unused
    WRITE_SUPERTYPE_DATA();
    WRITE_SUBTYPE_DATA();
    header.gparamOffset = 0;  // unused
    header.sceneGparamOffset = 0;  // unused
    header.grassConfigOffset = 0;  // unused
    WRITE_STRUCT(unkStruct8);
    header.unkStruct9Offset = 0;  // unused
    WRITE_STRUCT(tileLoadConfig);
    header.unkStruct11Offset = 0;  // unused
}


struct DummyAssetSubtypeData
{
    int32_t _zero1;
    int32_t _zero2;
    int32_t _minusOne1;
    int32_t _zero3;
    int32_t _minusOne2;
    int32_t _minusOne3;
    int32_t sUnk18;
    int32_t _minusOne4;

    void Validate() const
    {
        AssertValue("DummyAssetSubtypeData._zero1", 0, _zero1);
        AssertValue("DummyAssetSubtypeData._zero2", 0, _zero2);
        AssertValue("DummyAssetSubtypeData._zero3", 0, _zero3);
        AssertValue("DummyAssetSubtypeData._minusOne1", -1, _minusOne1);
        AssertValue("DummyAssetSubtypeData._minusOne2", -1, _minusOne2);
        AssertValue("DummyAssetSubtypeData._minusOne3", -1, _minusOne3);
        AssertValue("DummyAssetSubtypeData._minusOne4", -1, _minusOne4);
    }
};


bool DummyAssetPart::DeserializeSubtypeData(ifstream& stream)
{
    const auto subtypeData = ReadValidatedStruct<DummyAssetSubtypeData>(stream);
    sUnk18 = subtypeData.sUnk18;
    return true;
}


bool DummyAssetPart::SerializeSubtypeData(ofstream& stream, int supertypeIndex, int subtypeIndex) const
{
    const auto subtypeData = DummyAssetSubtypeData
    {
        0, 0, -1, 0, -1, -1, sUnk18, -1};
    WriteValidatedStruct(stream, subtypeData);
    return true;
}


void ConnectCollisionPart::DeserializeStructs(ifstream& stream, const PartHeader& header, const streampos& entryStart)
{
    READ_STRUCT(drawInfo1);
    READ_STRUCT(drawInfo2);
    READ_SUPERTYPE_DATA();
    READ_SUBTYPE_DATA();
    ASSERT_ZERO_STRUCT_OFFSET("GParam", header.gparamOffset);
    ASSERT_ZERO_STRUCT_OFFSET("SceneGParam", header.sceneGparamOffset);
    ASSERT_ZERO_STRUCT_OFFSET("GrassConfig", header.grassConfigOffset);
    READ_STRUCT(unkStruct8);
    ASSERT_ZERO_STRUCT_OFFSET("UnkStruct9", header.unkStruct9Offset);
    READ_STRUCT(tileLoadConfig);
    READ_STRUCT(unkStruct11);
}


void ConnectCollisionPart::SerializeStructs(ofstream& stream, PartHeader& header, const streampos& entryStart, const int& supertypeIndex, const int& subtypeIndex) const
{
    WRITE_STRUCT(drawInfo1);
    header.drawInfo2Offset = 0;  // unused
    WRITE_SUPERTYPE_DATA();
    WRITE_SUBTYPE_DATA();
    header.gparamOffset = 0;  // unused
    header.sceneGparamOffset = 0;  // unused
    header.grassConfigOffset = 0;  // unused
    WRITE_STRUCT(unkStruct8);
    header.unkStruct9Offset = 0;  // unused
    WRITE_STRUCT(tileLoadConfig);
    WRITE_STRUCT(unkStruct11);
}


struct ConnectCollisionSubtypeData
{
    int32_t collisionIndex;
    array<int8_t, 4> connectedMapId;  // unsigned?
    uint8_t sUnk08;
    bool sUnk09;
    uint8_t sUnk0A;
    bool sUnk0B;
    int32_t _zero;

    void Validate() const
    {
        AssertValue("ConnectCollisionSubtypeData._zero", 0, _zero);
    }
};


bool ConnectCollisionPart::DeserializeSubtypeData(ifstream& stream)
{
    const auto subtypeData = ReadValidatedStruct<ConnectCollisionSubtypeData>(stream);
    collisionIndex = subtypeData.collisionIndex;
    connectedMapId = subtypeData.connectedMapId;
    sUnk08 = subtypeData.sUnk08;
    sUnk09 = subtypeData.sUnk09;
    sUnk0A = subtypeData.sUnk0A;
    sUnk0B = subtypeData.sUnk0B;
    return true;
}


bool ConnectCollisionPart::SerializeSubtypeData(ofstream& stream, int supertypeIndex, int subtypeIndex) const
{
    const auto subtypeData = ConnectCollisionSubtypeData
    {
        .collisionIndex = collisionIndex,
        .connectedMapId = connectedMapId,
        .sUnk08 = sUnk08,
        .sUnk09 = sUnk09,
        .sUnk0A = sUnk0A,
        .sUnk0B = sUnk0B,
    };
    WriteValidatedStruct(stream, subtypeData);
    return true;
}


void ConnectCollisionPart::DeserializeCollisionReference(const vector<CollisionPart*>& collisions)
{
    collision.SetFromIndex(collisions, collisionIndex);
}


void ConnectCollisionPart::SerializeCollisionIndex(const vector<CollisionPart*>& collisions)
{
    collisionIndex = collision.ToIndex(this, collisions);
}


void AssetPart::DeserializeStructs(ifstream& stream, const PartHeader& header, const streampos& entryStart)
{
    // Uses all structs except `SceneGParam`.
    READ_STRUCT(drawInfo1);
    READ_STRUCT(drawInfo2);
    READ_SUPERTYPE_DATA();
    READ_SUBTYPE_DATA();
    READ_STRUCT(gparam);
    ASSERT_ZERO_STRUCT_OFFSET("SceneGParam", header.sceneGparamOffset);
    READ_STRUCT(grassConfig);
    READ_STRUCT(unkStruct8);
    READ_STRUCT(unkStruct9);
    READ_STRUCT(tileLoadConfig);
    READ_STRUCT(unkStruct11);
}


void AssetPart::SerializeStructs(ofstream& stream, PartHeader& header, const streampos& entryStart, const int& supertypeIndex, const int& subtypeIndex) const
{
    // Uses all structs except `SceneGParam`.
    WRITE_STRUCT(drawInfo1);
    WRITE_STRUCT(drawInfo2);
    WRITE_SUPERTYPE_DATA();
    WRITE_SUBTYPE_DATA();
    WRITE_STRUCT(gparam);
    header.sceneGparamOffset = 0;  // unused
    WRITE_STRUCT(grassConfig);
    WRITE_STRUCT(unkStruct8);
    WRITE_STRUCT(unkStruct9);
    WRITE_STRUCT(tileLoadConfig);
    WRITE_STRUCT(unkStruct11);
}


struct AssetPartSubtypeData
{
    int16_t _zero1;
    int16_t sUnk02;
    padding<12> _pad1;
    uint8_t sUnk10;
    bool sUnk11;
    uint8_t sUnk12;
    padding<9> _pad2;
    int16_t sfxParamRelativeId;
    int16_t sUnk1E;
    int _minusOne;
    int sUnk24;
    int sUnk28;
    int _zero2;
    int sUnk30;
    int sUnk34;
    array<int, 6> drawParentPartsIndices;
    bool sUnk50;
    uint8_t sUnk51;
    uint8_t _zero3;
    uint8_t sUnk53;
    int sUnk54;
    int sUnk58;
    int sUnk5C;
    int sUnk60;
    int sUnk64;
    // Constant offsets to four Asset sub-structs (packed immediately after this struct).
    int64_t _extraAssetData1Offset;  // 0x88
    int64_t _extraAssetData2Offset;  // 0xC8
    int64_t _extraAssetData3Offset;  // 0x108
    int64_t _extraAssetData4Offset;  // 0x148

    void Validate() const
    {
        AssertValue("AssetPartSubtypeData._zero1", 0, _zero1);
        AssertValue("AssetPartSubtypeData._zero2", 0, _zero2);
        AssertValue("AssetPartSubtypeData._zero3", 0, _zero3);
        AssertValue("AssetPartSubtypeData._minusOne", -1, _minusOne);
        AssertPadding("AssetPartSubtypeData", _pad1, _pad2);

        AssertValue("AssetPartSubtypeData._extraAssetData1Offset", 0x88, _extraAssetData1Offset);
        AssertValue("AssetPartSubtypeData._extraAssetData2Offset", 0xC8, _extraAssetData2Offset);
        AssertValue("AssetPartSubtypeData._extraAssetData3Offset", 0x108, _extraAssetData3Offset);
        AssertValue("AssetPartSubtypeData._extraAssetData4Offset", 0x148, _extraAssetData4Offset);
    }
};


void ExtraAssetData1::Read(ifstream& stream)
{
    unk00 = ReadValue<int16_t>(stream);
    AssertReadValue<int16_t>(stream, -1, "ExtraAssetData1 padding");
    unk04 = ReadValue<bool>(stream);
    disableTorrentAssetOnly = ReadValue<bool>(stream);
    AssertReadValue<int16_t>(stream, -1, "ExtraAssetData1 padding");
    constexpr array constants{0, 0, -1, -1, -1};
    AssertReadValues(stream, constants, "ExtraAssetData1 padding");
    unk1C = ReadValue<int>(stream);
    AssertReadValue<int32_t>(stream, 0, "ExtraAssetData1 padding");
    unk24 = ReadValue<int16_t>(stream);
    unk26 = ReadValue<int16_t>(stream);
    unk28 = ReadValue<int32_t>(stream);
    unk2C = ReadValue<int32_t>(stream);
    AssertReadPadBytes(stream, 0x10, "ExtraAssetData1 padding");
}


void ExtraAssetData1::Write(ofstream& stream) const
{
    WriteValue(stream, unk00);
    WriteValue<int16_t>(stream, -1);
    WriteValue(stream, unk04);
    WriteValue(stream, disableTorrentAssetOnly);
    WriteValue<int16_t>(stream, -1);
    constexpr array constants{0, 0, -1, -1, -1};
    WriteValues(stream, constants);
    WriteValue(stream, unk1C);
    WriteValue<int32_t>(stream, 0);
    WriteValue(stream, unk24);
    WriteValue(stream, unk26);
    WriteValue(stream, unk28);
    WriteValue(stream, unk2C);
    WritePadBytes(stream, 0x10);
}


void ExtraAssetData2::Read(ifstream& stream)
{
    unk00 = ReadValue<int>(stream);
    unk04 = ReadValue<int>(stream);
    constexpr array constants = {-1, 0, 0};
    AssertReadValues(stream, constants, "ExtraAssetData2 padding");
    unk14 = ReadValue<float>(stream);
    AssertReadValue<int>(stream, 0, "ExtraAssetData2 padding");
    unk1C = ReadValue<uint8_t>(stream);
    unk1D = ReadValue<uint8_t>(stream);
    unk1E = ReadValue<uint8_t>(stream);
    unk1F = ReadValue<uint8_t>(stream);
    AssertReadPadBytes(stream, 0x20, "ExtraAssetData2 padding");
}


void ExtraAssetData2::Write(ofstream& stream) const
{
    WriteValue(stream, unk00);
    WriteValue(stream, unk04);
    constexpr array constants = {-1, 0, 0};
    WriteValues(stream, constants);
    WriteValue(stream, unk14);
    WriteValue<int>(stream, 0);
    WriteValue(stream, unk1C);
    WriteValue(stream, unk1D);
    WriteValue(stream, unk1E);
    WriteValue(stream, unk1F);
    WritePadBytes(stream, 0x20);
}


void ExtraAssetData3::Read(ifstream& stream)
{
    unk00 = ReadValue<int>(stream);
    unk04 = ReadValue<float>(stream);
    AssertReadValue<int8_t>(stream, -1, "ExtraAssetData3 padding");
    unk09 = ReadValue<uint8_t>(stream);
    unk0A = ReadValue<uint8_t>(stream);
    unk0B = ReadValue<uint8_t>(stream);
    unk0C = ReadValue<int16_t>(stream);
    unk0E = ReadValue<int16_t>(stream);
    unk10 = ReadValue<float>(stream);
    disableWhenMapLoadedWithId = ReadValues<uint8_t, 4>(stream);
    unk18 = ReadValue<int>(stream);
    unk1C = ReadValue<int>(stream);
    unk20 = ReadValue<int>(stream);
    unk24 = ReadValue<uint8_t>(stream);
    unk25 = ReadValue<bool>(stream);
    AssertReadPadBytes(stream, 2, "ExtraAssetData3 padding");
    AssertReadPadBytes(stream, 24, "ExtraAssetData3 padding");
}


void ExtraAssetData3::Write(ofstream& stream) const
{
    WriteValue(stream, unk00);
    WriteValue(stream, unk04);
    WriteValue<int8_t>(stream, -1);
    WriteValue(stream, unk09);
    WriteValue(stream, unk0A);
    WriteValue(stream, unk0B);
    WriteValue(stream, unk0C);
    WriteValue(stream, unk0E);
    WriteValue(stream, unk10);
    WriteValues(stream, disableWhenMapLoadedWithId);
    WriteValue(stream, unk18);
    WriteValue(stream, unk1C);
    WriteValue(stream, unk20);
    WriteValue(stream, unk24);
    WriteValue(stream, unk25);
    WritePadBytes(stream, 2);
    WritePadBytes(stream, 24);
}


void ExtraAssetData4::Read(ifstream& stream)
{
    unk00 = ReadValue<bool>(stream);
    unk01 = ReadValue<uint8_t>(stream);
    unk02 = ReadValue<uint8_t>(stream);
    unk03 = ReadValue<bool>(stream);
    AssertReadPadBytes(stream, 60, "ExtraAssetData4 padding");
}


void ExtraAssetData4::Write(ofstream& stream) const
{
    WriteValue(stream, unk00);
    WriteValue(stream, unk01);
    WriteValue(stream, unk02);
    WriteValue(stream, unk03);
    WritePadBytes(stream, 60);
}


bool AssetPart::DeserializeSubtypeData(ifstream& stream)
{
    auto subtypeData = ReadValidatedStruct<AssetPartSubtypeData>(stream);
    sUnk10 = subtypeData.sUnk10;
    sUnk11 = subtypeData.sUnk11;
    sUnk12 = subtypeData.sUnk12;
    sfxParamRelativeId = subtypeData.sfxParamRelativeId;
    sUnk1E = subtypeData.sUnk1E;
    sUnk24 = subtypeData.sUnk24;
    sUnk28 = subtypeData.sUnk28;
    sUnk30 = subtypeData.sUnk30;
    sUnk34 = subtypeData.sUnk34;
    ClearReferenceArray(drawParentParts);
    ranges::copy(subtypeData.drawParentPartsIndices, drawParentPartsIndices.begin());
    sUnk50 = subtypeData.sUnk50;
    sUnk51 = subtypeData.sUnk51;
    sUnk53 = subtypeData.sUnk53;
    sUnk54 = subtypeData.sUnk54;
    sUnk58 = subtypeData.sUnk58;
    sUnk5C = subtypeData.sUnk5C;
    sUnk60 = subtypeData.sUnk60;
    sUnk64 = subtypeData.sUnk64;

    extraData1.Read(stream);
    extraData2.Read(stream);
    extraData3.Read(stream);
    extraData4.Read(stream);
    return true;
}


bool AssetPart::SerializeSubtypeData(ofstream& stream, int supertypeIndex, int subtypeIndex) const
{
    auto subtypeData = AssetPartSubtypeData
    {
        .sUnk10 = sUnk10,
        .sUnk11 = sUnk11,
        .sUnk12 = sUnk12,
        .sfxParamRelativeId = sfxParamRelativeId,
        .sUnk1E = sUnk1E,
        ._minusOne = -1,
        .sUnk24 = sUnk24,
        .sUnk28 = sUnk28,
        .sUnk30 = sUnk30,
        .sUnk34 = sUnk34,
        .sUnk50 = sUnk50,
        .sUnk51 = sUnk51,
        .sUnk53 = sUnk53,
        .sUnk54 = sUnk54,
        .sUnk58 = sUnk58,
        .sUnk5C = sUnk5C,
        .sUnk60 = sUnk60,
        .sUnk64 = sUnk64,
        ._extraAssetData1Offset = 0x88,
        ._extraAssetData2Offset = 0xC8,
        ._extraAssetData3Offset = 0x108,
        ._extraAssetData4Offset = 0x148,
    };
    ranges::copy(drawParentPartsIndices, subtypeData.drawParentPartsIndices.begin());

    WriteValidatedStruct(stream, subtypeData);

    extraData1.Write(stream);
    extraData2.Write(stream);
    extraData3.Write(stream);
    extraData4.Write(stream);
    return true;
}


void AssetPart::DeserializeEntryReferences(const vector<Model*>& models, const vector<Part*>& parts, const vector<Region*>& regions)
{
    Part::DeserializeEntryReferences(models, parts, regions);
    SetReferenceArray(drawParentParts, drawParentPartsIndices, parts);
}


void AssetPart::SerializeEntryIndices(const vector<Model*>& models, const vector<Part*>& parts, const vector<Region*>& regions)
{
    Part::SerializeEntryIndices(models, parts, regions);
    SetIndexArray(this, drawParentPartsIndices, drawParentParts, parts);
}
