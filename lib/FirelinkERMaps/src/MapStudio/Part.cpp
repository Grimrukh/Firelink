#include "MSBBufferHelpers.h"
#include <FirelinkCore/BinaryReadWrite.h>
#include <FirelinkCore/BinaryValidation.h>
#include <FirelinkCore/Collections.h>
#include <FirelinkERMaps/MapStudio/Model.h>
#include <FirelinkERMaps/MapStudio/MSB.h>
#include <FirelinkERMaps/MapStudio/MSBFormatError.h>
#include <FirelinkERMaps/MapStudio/Part.h>
#include <FirelinkERMaps/MapStudio/Region.h>

#include <cstdint>

using namespace Firelink;
using namespace Firelink::BinaryReadWrite;
using namespace Firelink::BinaryValidation;
using namespace Firelink::EldenRing::Maps::MapStudio::BufferHelpers;
using namespace Firelink::EldenRing::Maps;
using namespace Firelink::EldenRing::Maps::MapStudio;

// MACRO: Check that unused struct offset is zero.
#define ASSERT_ZERO_STRUCT_OFFSET(name, offset)                                                         \
    if ((offset) != 0)                                                                                  \
    {                                                                                                   \
        throw MSBFormatError("Expected struct " #name " offset to be 0, but got " + std::to_string(offset)); \
    }

// MACRO: Check that header offset is non-zero, then read struct.
#define READ_STRUCT(name)                                                                   \
    if (header.name##Offset == 0)                                                           \
    {                                                                                       \
        throw MSBFormatError("Expected struct " #name " offset to be non-zero, but got 0"); \
    }                                                                                       \
    reader.Seek(entryStart + header.name##Offset);                                         \
    (name).Read(reader);

// MACRO: Check supertype data offset is non-zero, then read supertype data.
#define READ_SUPERTYPE_DATA()                                                             \
    if (header.supertypeDataOffset == 0)                                                  \
    {                                                                                     \
        throw MSBFormatError("Expected supertype data offset to be non-zero, but got 0"); \
    }                                                                                     \
    reader.Seek(entryStart + header.supertypeDataOffset);                                         \
    ReadSupertypeData(reader);

// MACRO: Check subtype data offset is non-zero (the case for all subtypes), then read subtype data.
#define READ_SUBTYPE_DATA()                                                             \
    if (header.subtypeDataOffset == 0)                                                  \
    {                                                                                   \
        throw MSBFormatError("Expected subtype data offset to be non-zero, but got 0"); \
    }                                                                                   \
    reader.Seek(entryStart + header.subtypeDataOffset);                                         \
    if (!DeserializeSubtypeData(reader))                                                \
    {                                                                                   \
        throw MSBFormatError("Failed to read subtype data for part " + GetNameUTF8());  \
    }

// MACRO: Record struct offset in header, then write struct.
#define WRITE_STRUCT(name)                             \
    header.name##Offset = writer.Position() - entryStart; \
    (name).Write(writer);

// MACRO: Record supertype data offset in header, then write supertype data.
#define WRITE_SUPERTYPE_DATA()                                \
    header.supertypeDataOffset = static_cast<int64_t>(writer.Position() - entryStart); \
    WriteSupertypeData(writer);

// MACRO: Record subtype data offset in header, then write subtype data (passing in supertype/subtype indices).
#define WRITE_SUBTYPE_DATA()                                    \
    header.subtypeDataOffset = static_cast<int64_t>(writer.Position() - entryStart);     \
    SerializeSubtypeData(writer, supertypeIndex, subtypeIndex);

struct MapStudio::PartHeader
{
    int64_t nameOffset;           // Offset to the part's name
    int32_t modelInstanceId;      // Unique instance ID for the model
    PartType subtype;             // Part type
    int32_t subtypeIndex;         // Index of the subtype
    int32_t modelIndex;           // Model index (MODEL_PARAM_ST reference)
    int64_t sibPathOffset;        // Offset to the SIB path
    Vector3 translate;            // Translation vector
    Vector3 rotate;               // Rotation vector
    Vector3 scale;                // Scale vector
    int32_t unk44;                // Unknown field at offset 44
    int32_t eventLayer;           // MapStudio/ceremony event layer
    int32_t _zero;                // 0
    int64_t drawInfo1Offset;      // Offset for DrawInfo1 data
    int64_t drawInfo2Offset;      // Offset for DrawInfo2 data
    int64_t supertypeDataOffset;  // Offset for Part supertype data
    int64_t subtypeDataOffset;    // Offset for Part subtype data
    int64_t gparamOffset;         // Offset for GParam data
    int64_t sceneGparamOffset;    // Offset for SceneGParam data
    int64_t grassConfigOffset;    // Offset for GrassConfig data
    int64_t unkStruct8Offset;     // Offset for unknown data struct 8
    int64_t unkStruct9Offset;     // Offset for unknown data struct 9
    int64_t tileLoadConfigOffset; // Offset for TileLoad data
    int64_t unkStruct11Offset;    // Offset for unknown data struct 11
    std::array<uint8_t, 24> _pad1;     // Padding (three unused offsets?)

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

void DrawInfo1::Read(BufferReader& reader)
{
    displayGroups = GroupBitSet<256>(reader);
    drawGroups = GroupBitSet<256>(reader);
    collisionMask = GroupBitSet<1024>(reader);
    condition1A = reader.Read<uint8_t>();
    condition1B = reader.Read<uint8_t>();
    unkC2 = reader.Read<uint8_t>();
    unkC3 = reader.Read<uint8_t>();
    unkC4 = reader.Read<int16_t>();
    unkC6 = reader.Read<int16_t>();
    reader.AssertPad(0xC0);
}

void DrawInfo1::Write(BufferWriter& writer) const
{
    displayGroups.Write(writer);
    drawGroups.Write(writer);
    collisionMask.Write(writer);
    writer.Write(condition1A);
    writer.Write(condition1B);
    writer.Write(unkC2);
    writer.Write(unkC3);
    writer.Write(unkC4);
    writer.Write(unkC6);
    writer.WritePad(0xC0);
}

void DrawInfo2::Read(BufferReader& reader)
{
    condition2 = reader.Read<int32_t>();
    displayGroups2 = GroupBitSet<256>(reader);
    unk24 = reader.Read<int16_t>();
    unk26 = reader.Read<int16_t>();
    reader.AssertPad(0x20);
}

void DrawInfo2::Write(BufferWriter& writer) const
{
    writer.Write(condition2);
    displayGroups2.Write(writer);
    writer.Write(unk24);
    writer.Write(unk26);
    writer.WritePad(0x20);
}

struct PartDataStruct
{
    uint32_t entityId;
    uint8_t unk04;
    padding<3> _pad1; // includes former "Lantern ID"
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
    std::array<uint32_t, 8> entityGroupIds;
    int16_t unk3C;
    int16_t unk3E;

    void Validate() const { AssertPadding("PartDataStruct", _pad1); }
};

void GParam::Read(BufferReader& reader)
{
    lightSetId = reader.Read<int32_t>();
    fogId = reader.Read<int32_t>();
    lightScatteringId = reader.Read<int32_t>();
    environmentMapId = reader.Read<int32_t>();
    reader.AssertPad(16);
}

void GParam::Write(BufferWriter& writer) const
{
    writer.Write(lightSetId);
    writer.Write(fogId);
    writer.Write(lightScatteringId);
    writer.Write(environmentMapId);
    writer.WritePad(16);
}

void SceneGParam::Read(BufferReader& reader)
{
    reader.AssertPad(0x10);
    transitionTime = reader.Read<float>();
    reader.AssertValue<int32_t>(0, "SceneGParamData[0x20]");

    unk18 = reader.Read<int8_t>();
    unk19 = reader.Read<int8_t>();
    unk1A = reader.Read<int8_t>();
    unk1B = reader.Read<int8_t>();

    unk1C = reader.Read<int8_t>();
    unk1D = reader.Read<int8_t>();
    reader.AssertPad(2);

    unk20 = reader.Read<int8_t>();
    unk21 = reader.Read<int8_t>();
    reader.AssertPad(2);

    reader.AssertPad(0x44);
}

void SceneGParam::Write(BufferWriter& writer) const
{
    writer.WritePad(0x10);
    writer.Write(transitionTime);
    writer.Write(0);

    writer.Write(unk18);
    writer.Write(unk19);
    writer.Write(unk1A);
    writer.Write(unk1B);

    writer.Write(unk1C);
    writer.Write(unk1D);
    writer.WritePad(2);

    writer.Write(unk20);
    writer.Write(unk21);
    writer.WritePad(2);

    writer.WritePad(0x44);
}

void GrassConfig::Read(BufferReader& reader)
{
    unk00 = reader.Read<int32_t>();
    unk04 = reader.Read<int32_t>();
    unk08 = reader.Read<int32_t>();
    unk0C = reader.Read<int32_t>();
    unk10 = reader.Read<int32_t>();
    unk14 = reader.Read<int32_t>();
    unk18 = reader.Read<int32_t>();
    reader.AssertValue<int32_t>(0, "UnkPartStruct7 zero");
}

void GrassConfig::Write(BufferWriter& writer) const
{
    writer.Write(unk00);
    writer.Write(unk04);
    writer.Write(unk08);
    writer.Write(unk0C);
    writer.Write(unk10);
    writer.Write(unk14);
    writer.Write(unk18);
    writer.Write(0);
}

void UnkPartStruct8::Read(BufferReader& reader)
{
    unk00 = reader.Read<int32_t>();
    if (unk00 != 0 && unk00 != 1)
    {
        throw MSBFormatError("UnkPartStruct8: Expected unk00 to be 0 or 1, but got " + std::to_string(unk00));
    }
    reader.AssertPad(0x1C);
}

void UnkPartStruct8::Write(BufferWriter& writer) const
{
    writer.Write<int32_t>(unk00);
    writer.WritePad(0x1C);
}

void UnkPartStruct9::Read(BufferReader& reader)
{
    unk00 = reader.Read<int32_t>();
    reader.AssertPad(0x1C);
}

void UnkPartStruct9::Write(BufferWriter& writer) const
{
    writer.Write<int32_t>(unk00);
    writer.WritePad(0x1C);
}

void TileLoadConfig::Read(BufferReader& reader)
{
    mapId = reader.Read<int32_t>();
    unk04 = reader.Read<int32_t>();
    reader.AssertValue<int32_t>(0, "UnkPartStruct10 zero");
    unk0C = reader.Read<int32_t>();
    unk10 = reader.Read<int32_t>();
    if (unk10 != 0 && unk10 != 1)
    {
        throw MSBFormatError("UnkPartStruct10: Expected unk10 to be 0 or 1, but got " + std::to_string(unk10));
    }
    unk14 = reader.Read<int32_t>();
    reader.AssertPad(0x8);
}

void TileLoadConfig::Write(BufferWriter& writer) const
{
    writer.Write(mapId);
    writer.Write(unk04);
    writer.Write(0);
    writer.Write(unk0C);
    writer.Write(unk10);
    writer.Write(unk14);
    writer.WritePad(0x8);
}

void UnkPartStruct11::Read(BufferReader& reader)
{
    unk00 = reader.Read<int32_t>();
    unk04 = reader.Read<int32_t>();
    reader.AssertPad(0x18);
}

void UnkPartStruct11::Write(BufferWriter& writer) const
{
    writer.Write(unk00);
    writer.Write(unk04);
    writer.WritePad(0x18);
}

void Part::Deserialize(BufferReader& reader)
{
    const size_t start = reader.Position();

    const auto header = BufferHelpers::ReadValidatedStruct<PartHeader>(reader);

    reader.Seek(start + header.nameOffset);
    m_name = BufferHelpers::ReadUTF16String(reader);

    reader.Seek(start + header.sibPathOffset);
    m_sibPath = BufferHelpers::ReadUTF16String(reader);

    modelIndex = header.modelIndex;
    modelInstanceId = header.modelInstanceId;
    translate = header.translate;
    rotate = header.rotate;
    scale = header.scale;
    unk44 = header.unk44;
    eventLayer = header.eventLayer;

    DeserializeStructs(reader, header, start);
}

void Part::Serialize(BufferWriter& writer, const int supertypeIndex, const int subtypeIndex) const
{
    const size_t start = writer.Position();

        const void* scope = this;

    auto header = PartHeader{
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
    writer.Reserve<PartHeader>("PartHeader", scope);

    // TODO: Funky combined string padding rules?
    header.nameOffset = static_cast<int64_t>(writer.Position() - start);
    BufferHelpers::WriteUTF16String(writer, m_name);
    header.sibPathOffset = static_cast<int64_t>(writer.Position() - start);
    BufferHelpers::WriteUTF16String(writer, m_sibPath);
    writer.PadAlign(8);

    // No additional alignment necessary before or after structs.
    SerializeStructs(writer, header, start, supertypeIndex, subtypeIndex);

    header.Validate();
    writer.Fill<PartHeader>("PartHeader", header, scope);

    
}

void Part::ReadSupertypeData(BufferReader& reader)
{
    auto supertypeData = BufferHelpers::ReadValidatedStruct<PartDataStruct>(reader);
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
    std::ranges::copy(supertypeData.entityGroupIds, entityGroupIds.begin());
    unk3C = supertypeData.unk3C;
    unk3E = supertypeData.unk3E;
}

void Part::WriteSupertypeData(BufferWriter& writer) const
{
    auto supertypeData = PartDataStruct{
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
    std::ranges::copy(entityGroupIds, supertypeData.entityGroupIds.begin());

    BufferHelpers::WriteValidatedStruct(writer, supertypeData);
}

void Part::OnReferencedEntryDestroy(const Entry* destroyedEntry)
{
    EntityEntry::OnReferencedEntryDestroy(destroyedEntry);
    if (const auto destroyedModel = dynamic_cast<const Model*>(destroyedEntry))
        model.OnReferencedEntryDestroy(destroyedModel);
}

void Part::DeserializeEntryReferences(
    const std::vector<Model*>& models, const std::vector<Part*>& parts, const std::vector<Region*>& regions)
{
    model.SetFromIndex(models, modelIndex);
}

void Part::SerializeEntryIndices(
    const std::vector<Model*>& models, const std::vector<Part*>& parts, const std::vector<Region*>& regions)
{
    modelIndex = model.ToIndex(this, models);
}

void MapPiecePart::DeserializeStructs(BufferReader& reader, const PartHeader& header, std::size_t entryStart)
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

void MapPiecePart::SerializeStructs(
    BufferWriter& writer,
    PartHeader& header,
    std::size_t entryStart,
    const int& supertypeIndex,
    const int& subtypeIndex) const
{
    WRITE_STRUCT(drawInfo1);
    header.drawInfo2Offset = 0; // unused
    WRITE_SUPERTYPE_DATA();
    WRITE_SUBTYPE_DATA();
    WRITE_STRUCT(gparam);
    header.sceneGparamOffset = 0; // unused
    WRITE_STRUCT(grassConfig);
    WRITE_STRUCT(unkStruct8);
    WRITE_STRUCT(unkStruct9);
    WRITE_STRUCT(tileLoadConfig);
    WRITE_STRUCT(unkStruct11);
}

bool MapPiecePart::DeserializeSubtypeData(BufferReader& reader)
{
    // Just padding.
    reader.AssertPad(8);
    return true;
}

bool MapPiecePart::SerializeSubtypeData(BufferWriter& writer, int supertypeIndex, int subtypeIndex) const
{
    writer.WritePad(8);
    return true;
}

void CharacterPart::DeserializeStructs(BufferReader& reader, const PartHeader& header, std::size_t entryStart)
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
    BufferWriter& writer,
    PartHeader& header,
    std::size_t entryStart,
    const int& supertypeIndex,
    const int& subtypeIndex) const
{
    WRITE_STRUCT(drawInfo1);
    header.drawInfo2Offset = 0; // unused
    WRITE_SUPERTYPE_DATA();
    WRITE_SUBTYPE_DATA();
    WRITE_STRUCT(gparam);
    header.sceneGparamOffset = 0; // unused
    header.grassConfigOffset = 0; // unused
    WRITE_STRUCT(unkStruct8);
    header.unkStruct9Offset = 0; // unused
    WRITE_STRUCT(tileLoadConfig);
    header.unkStruct11Offset = 0; // unused
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
    std::array<int, 4> specialEffectSetParamIds;
    padding<40> _pad2;
    int64_t _x80;
    int _zero4;
    float sUnk84;
    // Five instances of four-short pattern [-1, -1 , -1, 0xA]:
    std::array<int16_t, 4> _pad3a;
    std::array<int16_t, 4> _pad3b;
    std::array<int16_t, 4> _pad3c;
    std::array<int16_t, 4> _pad3d;
    std::array<int16_t, 4> _pad3e;
    padding<0x10> _pad4;

    void Validate() const
    {
        AssertPadding("CharacterSubtypeData", _pad1, _pad2, _pad4);
        AssertValue("CharacterSubtypeData", 0, _zero);
        AssertValue("CharacterSubtypeData", 0, _zero3);
        AssertValue("CharacterSubtypeData", 0, _zero4);
        AssertValue("CharacterSubtypeData", 0x80, _x80);

        constexpr std::array<int16_t, 4> pad3 = {-1, -1, -1, 0xA};
        ValidateArray("CharacterSubtypeData", pad3, _pad3a);
        ValidateArray("CharacterSubtypeData", pad3, _pad3b);
        ValidateArray("CharacterSubtypeData", pad3, _pad3c);
        ValidateArray("CharacterSubtypeData", pad3, _pad3d);
        ValidateArray("CharacterSubtypeData", pad3, _pad3e);
    }
};

bool CharacterPart::DeserializeSubtypeData(BufferReader& reader)
{
    const auto subtypeData = BufferHelpers::ReadValidatedStruct<CharacterSubtypeData>(reader);
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

bool CharacterPart::SerializeSubtypeData(BufferWriter& writer, int supertypeIndex, int subtypeIndex) const
{
    const auto subtypeData = CharacterSubtypeData{
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
    BufferHelpers::WriteValidatedStruct(writer, subtypeData);
    return true;
}

void CharacterPart::DeserializeEntryReferences(
    const std::vector<Model*>& models, const std::vector<Part*>& parts, const std::vector<Region*>& regions)
{
    Part::DeserializeEntryReferences(models, parts, regions);
    drawParent.SetFromIndex(parts, drawParentIndex);
}

void CharacterPart::SerializeEntryIndices(
    const std::vector<Model*>& models, const std::vector<Part*>& parts, const std::vector<Region*>& regions)
{
    Part::SerializeEntryIndices(models, parts, regions);
    drawParentIndex = drawParent.ToIndex(this, parts);
}

void CharacterPart::DeserializePatrolRouteEventReference(const std::vector<PatrolRouteEvent*>& patrolRouteEvents)
{
    patrolRouteEvent.SetFromIndex16(patrolRouteEvents, patrolRouteEventIndex);
}

void CharacterPart::SerializePatrolRouteEventIndex(const std::vector<PatrolRouteEvent*>& patrolRouteEvents)
{
    patrolRouteEventIndex = patrolRouteEvent.ToIndex16(this, patrolRouteEvents);
}

void PlayerStartPart::DeserializeStructs(BufferReader& reader, const PartHeader& header, std::size_t entryStart)
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

void PlayerStartPart::SerializeStructs(
    BufferWriter& writer,
    PartHeader& header,
    std::size_t entryStart,
    const int& supertypeIndex,
    const int& subtypeIndex) const
{
    WRITE_STRUCT(drawInfo1);
    header.drawInfo2Offset = 0; // unused
    WRITE_SUPERTYPE_DATA();
    WRITE_SUBTYPE_DATA();
    header.gparamOffset = 0;      // unused
    header.sceneGparamOffset = 0; // unused
    header.grassConfigOffset = 0; // unused
    WRITE_STRUCT(unkStruct8);
    header.unkStruct9Offset = 0; // unused
    WRITE_STRUCT(tileLoadConfig);
    header.unkStruct11Offset = 0; // unused
}

bool PlayerStartPart::DeserializeSubtypeData(BufferReader& reader)
{
    // Not bothering with a struct.
    sUnk00 = reader.Read<int32_t>();
    reader.AssertPad(0xC);
    return true;
}

bool PlayerStartPart::SerializeSubtypeData(BufferWriter& writer, int supertypeIndex, int subtypeIndex) const
{
    writer.Write(sUnk00);
    writer.WritePad(0xC);
    return true;
}

void CollisionPart::DeserializeStructs(BufferReader& reader, const PartHeader& header, std::size_t entryStart)
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

void CollisionPart::SerializeStructs(
    BufferWriter& writer,
    PartHeader& header,
    std::size_t entryStart,
    const int& supertypeIndex,
    const int& subtypeIndex) const
{
    WRITE_STRUCT(drawInfo1);
    WRITE_STRUCT(drawInfo2);
    WRITE_SUPERTYPE_DATA();
    WRITE_SUBTYPE_DATA();
    WRITE_STRUCT(gparam);
    WRITE_STRUCT(sceneGparam);
    header.grassConfigOffset = 0; // unused
    WRITE_STRUCT(unkStruct8);
    header.unkStruct9Offset = 0; // unused
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
    int16_t sUnk26; // 0 or 1
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
    int16_t sUnk4C; // 0 or 1
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
            throw MSBFormatError("CollisionPartSubtypeData: Expected sUnk26 to be 0 or 1, but got " + std::to_string(sUnk26));
        }
        if (sUnk4C != 0 && sUnk4C != 1)
        {
            throw MSBFormatError("CollisionPartSubtypeData: Expected sUnk4C to be 0 or 1, but got " + std::to_string(sUnk4C));
        }
    }
};

bool CollisionPart::DeserializeSubtypeData(BufferReader& reader)
{
    const auto subtypeData = BufferHelpers::ReadValidatedStruct<CollisionPartSubtypeData>(reader);
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

bool CollisionPart::SerializeSubtypeData(BufferWriter& writer, int supertypeIndex, int subtypeIndex) const
{
    const auto subtypeData = CollisionPartSubtypeData{
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
    BufferHelpers::WriteValidatedStruct(writer, subtypeData);
    return true;
}

void DummyAssetPart::DeserializeStructs(BufferReader& reader, const PartHeader& header, std::size_t entryStart)
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

void DummyAssetPart::SerializeStructs(
    BufferWriter& writer,
    PartHeader& header,
    std::size_t entryStart,
    const int& supertypeIndex,
    const int& subtypeIndex) const
{
    WRITE_STRUCT(drawInfo1);
    header.drawInfo2Offset = 0; // unused
    WRITE_SUPERTYPE_DATA();
    WRITE_SUBTYPE_DATA();
    WRITE_STRUCT(gparam);
    header.sceneGparamOffset = 0; // unused
    header.grassConfigOffset = 0; // unused
    WRITE_STRUCT(unkStruct8);
    header.unkStruct9Offset = 0; // unused
    WRITE_STRUCT(tileLoadConfig);
    header.unkStruct11Offset = 0; // unused
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

bool DummyAssetPart::DeserializeSubtypeData(BufferReader& reader)
{
    const auto subtypeData = BufferHelpers::ReadValidatedStruct<DummyAssetSubtypeData>(reader);
    sUnk18 = subtypeData.sUnk18;
    return true;
}

bool DummyAssetPart::SerializeSubtypeData(BufferWriter& writer, int supertypeIndex, int subtypeIndex) const
{
    const auto subtypeData = DummyAssetSubtypeData{0, 0, -1, 0, -1, -1, sUnk18, -1};
    BufferHelpers::WriteValidatedStruct(writer, subtypeData);
    return true;
}

void ConnectCollisionPart::DeserializeStructs(BufferReader& reader, const PartHeader& header, std::size_t entryStart)
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

void ConnectCollisionPart::SerializeStructs(
    BufferWriter& writer,
    PartHeader& header,
    std::size_t entryStart,
    const int& supertypeIndex,
    const int& subtypeIndex) const
{
    WRITE_STRUCT(drawInfo1);
    WRITE_STRUCT(drawInfo2);
    WRITE_SUPERTYPE_DATA();
    WRITE_SUBTYPE_DATA();
    header.gparamOffset = 0;      // unused
    header.sceneGparamOffset = 0; // unused
    header.grassConfigOffset = 0; // unused
    WRITE_STRUCT(unkStruct8);
    header.unkStruct9Offset = 0; // unused
    WRITE_STRUCT(tileLoadConfig);
    WRITE_STRUCT(unkStruct11);
}

struct ConnectCollisionSubtypeData
{
    int32_t collisionIndex;
    std::array<int8_t, 4> connectedMapId; // unsigned?
    uint8_t sUnk08;
    bool sUnk09;
    uint8_t sUnk0A;
    bool sUnk0B;
    int32_t _zero;

    void Validate() const { AssertValue("ConnectCollisionSubtypeData._zero", 0, _zero); }
};

bool ConnectCollisionPart::DeserializeSubtypeData(BufferReader& reader)
{
    const auto subtypeData = BufferHelpers::ReadValidatedStruct<ConnectCollisionSubtypeData>(reader);
    collisionIndex = subtypeData.collisionIndex;
    connectedMapId = subtypeData.connectedMapId;
    sUnk08 = subtypeData.sUnk08;
    sUnk09 = subtypeData.sUnk09;
    sUnk0A = subtypeData.sUnk0A;
    sUnk0B = subtypeData.sUnk0B;
    return true;
}

bool ConnectCollisionPart::SerializeSubtypeData(BufferWriter& writer, int supertypeIndex, int subtypeIndex) const
{
    const auto subtypeData = ConnectCollisionSubtypeData{
        .collisionIndex = collisionIndex,
        .connectedMapId = connectedMapId,
        .sUnk08 = sUnk08,
        .sUnk09 = sUnk09,
        .sUnk0A = sUnk0A,
        .sUnk0B = sUnk0B,
    };
    BufferHelpers::WriteValidatedStruct(writer, subtypeData);
    return true;
}

void ConnectCollisionPart::DeserializeCollisionReference(const std::vector<CollisionPart*>& collisions)
{
    collision.SetFromIndex(collisions, collisionIndex);
}

void ConnectCollisionPart::SerializeCollisionIndex(const std::vector<CollisionPart*>& collisions)
{
    collisionIndex = collision.ToIndex(this, collisions);
}

void AssetPart::DeserializeStructs(BufferReader& reader, const PartHeader& header, std::size_t entryStart)
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

void AssetPart::SerializeStructs(
    BufferWriter& writer,
    PartHeader& header,
    std::size_t entryStart,
    const int& supertypeIndex,
    const int& subtypeIndex) const
{
    // Uses all structs except `SceneGParam`.
    WRITE_STRUCT(drawInfo1);
    WRITE_STRUCT(drawInfo2);
    WRITE_SUPERTYPE_DATA();
    WRITE_SUBTYPE_DATA();
    WRITE_STRUCT(gparam);
    header.sceneGparamOffset = 0; // unused
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
    std::array<int, 6> drawParentPartsIndices;
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
    int64_t _extraAssetData1Offset; // 0x88
    int64_t _extraAssetData2Offset; // 0xC8
    int64_t _extraAssetData3Offset; // 0x108
    int64_t _extraAssetData4Offset; // 0x148

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

void ExtraAssetData1::Read(BufferReader& reader)
{
    unk00 = reader.Read<int16_t>();
    reader.AssertValue<int16_t>(-1, "ExtraAssetData1 padding");
    unk04 = reader.Read<bool>();
    disableTorrentAssetOnly = reader.Read<bool>();
    reader.AssertValue<int16_t>(-1, "ExtraAssetData1 padding");
    constexpr std::array constants{0, 0, -1, -1, -1};
    BufferHelpers::AssertReadValues(reader, constants, "ExtraAssetData1 padding");
    unk1C = reader.Read<int>();
    reader.AssertValue<int32_t>(0, "ExtraAssetData1 padding");
    unk24 = reader.Read<int16_t>();
    unk26 = reader.Read<int16_t>();
    unk28 = reader.Read<int32_t>();
    unk2C = reader.Read<int32_t>();
    reader.AssertPad(0x10);
}

void ExtraAssetData1::Write(BufferWriter& writer) const
{
    writer.Write(unk00);
    writer.Write<int16_t>(-1);
    writer.Write(unk04);
    writer.Write(disableTorrentAssetOnly);
    writer.Write<int16_t>(-1);
    constexpr std::array constants{0, 0, -1, -1, -1};
    BufferHelpers::WriteValues(writer, constants);
    writer.Write(unk1C);
    writer.Write<int32_t>(0);
    writer.Write(unk24);
    writer.Write(unk26);
    writer.Write(unk28);
    writer.Write(unk2C);
    writer.WritePad(0x10);
}

void ExtraAssetData2::Read(BufferReader& reader)
{
    unk00 = reader.Read<int>();
    unk04 = reader.Read<int>();
    constexpr std::array constants = {-1, 0, 0};
    BufferHelpers::AssertReadValues(reader, constants, "ExtraAssetData2 padding");
    unk14 = reader.Read<float>();
    reader.AssertValue<int>(0, "ExtraAssetData2 padding");
    unk1C = reader.Read<uint8_t>();
    unk1D = reader.Read<uint8_t>();
    unk1E = reader.Read<uint8_t>();
    unk1F = reader.Read<uint8_t>();
    reader.AssertPad(0x20);
}

void ExtraAssetData2::Write(BufferWriter& writer) const
{
    writer.Write(unk00);
    writer.Write(unk04);
    constexpr std::array constants = {-1, 0, 0};
    BufferHelpers::WriteValues(writer, constants);
    writer.Write(unk14);
    writer.Write<int>(0);
    writer.Write(unk1C);
    writer.Write(unk1D);
    writer.Write(unk1E);
    writer.Write(unk1F);
    writer.WritePad(0x20);
}

void ExtraAssetData3::Read(BufferReader& reader)
{
    unk00 = reader.Read<int>();
    unk04 = reader.Read<float>();
    reader.AssertValue<int8_t>(-1, "ExtraAssetData3 padding");
    unk09 = reader.Read<uint8_t>();
    unk0A = reader.Read<uint8_t>();
    unk0B = reader.Read<uint8_t>();
    unk0C = reader.Read<int16_t>();
    unk0E = reader.Read<int16_t>();
    unk10 = reader.Read<float>();
    disableWhenMapLoadedWithId = BufferHelpers::ReadValues<uint8_t, 4>(reader);
    unk18 = reader.Read<int>();
    unk1C = reader.Read<int>();
    unk20 = reader.Read<int>();
    unk24 = reader.Read<uint8_t>();
    unk25 = reader.Read<bool>();
    reader.AssertPad(2);
    reader.AssertPad(24);
}

void ExtraAssetData3::Write(BufferWriter& writer) const
{
    writer.Write(unk00);
    writer.Write(unk04);
    writer.Write<int8_t>(-1);
    writer.Write(unk09);
    writer.Write(unk0A);
    writer.Write(unk0B);
    writer.Write(unk0C);
    writer.Write(unk0E);
    writer.Write(unk10);
    BufferHelpers::WriteValues(writer, disableWhenMapLoadedWithId);
    writer.Write(unk18);
    writer.Write(unk1C);
    writer.Write(unk20);
    writer.Write(unk24);
    writer.Write(unk25);
    writer.WritePad(2);
    writer.WritePad(24);
}

void ExtraAssetData4::Read(BufferReader& reader)
{
    unk00 = reader.Read<bool>();
    unk01 = reader.Read<uint8_t>();
    unk02 = reader.Read<uint8_t>();
    unk03 = reader.Read<bool>();
    reader.AssertPad(60);
}

void ExtraAssetData4::Write(BufferWriter& writer) const
{
    writer.Write(unk00);
    writer.Write(unk01);
    writer.Write(unk02);
    writer.Write(unk03);
    writer.WritePad(60);
}

bool AssetPart::DeserializeSubtypeData(BufferReader& reader)
{
    auto subtypeData = BufferHelpers::ReadValidatedStruct<AssetPartSubtypeData>(reader);
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
    std::ranges::copy(subtypeData.drawParentPartsIndices, drawParentPartsIndices.begin());
    sUnk50 = subtypeData.sUnk50;
    sUnk51 = subtypeData.sUnk51;
    sUnk53 = subtypeData.sUnk53;
    sUnk54 = subtypeData.sUnk54;
    sUnk58 = subtypeData.sUnk58;
    sUnk5C = subtypeData.sUnk5C;
    sUnk60 = subtypeData.sUnk60;
    sUnk64 = subtypeData.sUnk64;

    extraData1.Read(reader);
    extraData2.Read(reader);
    extraData3.Read(reader);
    extraData4.Read(reader);
    return true;
}

bool AssetPart::SerializeSubtypeData(BufferWriter& writer, int supertypeIndex, int subtypeIndex) const
{
    auto subtypeData = AssetPartSubtypeData{
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
    std::ranges::copy(drawParentPartsIndices, subtypeData.drawParentPartsIndices.begin());

    BufferHelpers::WriteValidatedStruct(writer, subtypeData);

    extraData1.Write(writer);
    extraData2.Write(writer);
    extraData3.Write(writer);
    extraData4.Write(writer);
    return true;
}

void AssetPart::DeserializeEntryReferences(
    const std::vector<Model*>& models, const std::vector<Part*>& parts, const std::vector<Region*>& regions)
{
    Part::DeserializeEntryReferences(models, parts, regions);
    SetReferenceArray(drawParentParts, drawParentPartsIndices, parts);
}

void AssetPart::SerializeEntryIndices(
    const std::vector<Model*>& models, const std::vector<Part*>& parts, const std::vector<Region*>& regions)
{
    Part::SerializeEntryIndices(models, parts, regions);
    SetIndexArray(this, drawParentPartsIndices, drawParentParts, parts);
}
