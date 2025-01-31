#include <cstdint>
#include <fstream>
#include <string>

#include "GrimHookER/Maps/MapStudio/Region.h"
#include "GrimHook/BinaryReadWrite.h"
#include "GrimHook/BinaryValidation.h"
#include "GrimHook/Collections.h"
#include "GrimHook/MemoryUtils.h"
#include "GrimHookER/Maps/MapStudio/MSBFormatError.h"
#include "GrimHookER/Maps/MapStudio/Shape.h"

using namespace std;
using namespace GrimHook;
using namespace GrimHook::BinaryReadWrite;
using namespace GrimHook::BinaryValidation;
using namespace GrimHookER::Maps;
using namespace GrimHookER::Maps::MapStudio;


struct RegionHeaderStruct
{
    int64_t nameOffset;
    RegionType subtype;
    int32_t subtypeIndex;
    ShapeType shapeType;
    Vector3 translate;
    Vector3 rotate;  // Euler angles in radians
    int32_t supertypeIndex;
    int64_t unkShortsAOffset;
    int64_t unkShortsBOffset;
    int32_t unk40;
    int32_t eventLayer;  // Map ceremony layer
    int64_t shapeDataOffset;
    int64_t supertypeDataOffset;
    int64_t subtypeDataOffset;
    int64_t extraDataOffset;

    void Validate() const
    {
        // We always expect certain offsets to be non-zero.
        AssertNotValue("RegionHeaderStruct.nameOffset", 0, nameOffset);
        AssertNotValue("RegionHeaderStruct.unkShortsAOffset", 0, unkShortsAOffset);
        AssertNotValue("RegionHeaderStruct.unkShortsBOffset", 0, unkShortsBOffset);
        AssertNotValue("RegionHeaderStruct.supertypeDataOffset", 0, supertypeDataOffset);
        AssertNotValue("RegionHeaderStruct.extraDataOffset",  0, extraDataOffset);
    }
};


struct RegionDataStruct
{
    int32_t attachedPartIndex;
    uint32_t entityId;
    uint8_t unk08;
    padding<7> _pad1;

    void Validate() const
    {
        AssertPadding("RegionDataStruct", _pad1);
    }
};


struct RegionExtraDataStruct
{
    int32_t mapId;
    int32_t unk04;
    padding<4> _pad1;
    int32_t unk0C;
    padding<16> _pad2;

    void Validate() const
    {
        AssertPadding("RegionExtraDataStruct", _pad1, _pad2);
    }
};


unique_ptr<Shape>& Region::SetShapeType(const ShapeType shapeType)
{
    switch (shapeType)
    {
        case ShapeType::NoShape:
            // Same effect as Point.
            shape = nullptr;
            return shape;
        case ShapeType::PointShape:
            shape = make_unique<Point>();
            return shape;
        case ShapeType::CircleShape:
            shape = make_unique<Circle>();
            return shape;
        case ShapeType::SphereShape:
            shape = make_unique<Sphere>();
            return shape;
        case ShapeType::CylinderShape:
            shape = make_unique<Cylinder>();
            return shape;
        case ShapeType::RectangleShape:
            shape = make_unique<Rectangle>();
            return shape;
        case ShapeType::BoxShape:
            shape = make_unique<Box>();
            return shape;
        case ShapeType::CompositeShape:
            shape = make_unique<Composite>();
            compositeChildren = make_unique<CompositeShapeReferences>();
            return shape;
    }

    throw invalid_argument("Invalid shape type.");
}


void Region::Deserialize(ifstream &stream)
{
    const streampos start = stream.tellg();

    const auto header = ReadValidatedStruct<RegionHeaderStruct>(stream);
    if (header.subtype != GetType())
        throw invalid_argument("Region header/class subtype mismatch.");

    SetShapeType(header.shapeType);
    translate = header.translate;
    rotate = header.rotate;
    this->hUnk40 = header.unk40;

    stream.seekg(start + header.nameOffset);
    const u16string nameWide = ReadUTF16String(stream);
    m_name = GrimHook::UTF16ToUTF8(nameWide);

    stream.seekg(start + header.unkShortsAOffset);
    const auto unkShortsACount = ReadValue<int16_t>(stream);
    for (int i = 0; i < unkShortsACount; i++)
        unkShortsA.push_back(ReadValue<int16_t>(stream));

    stream.seekg(start + header.unkShortsBOffset);
    const auto unkShortsBCount = ReadValue<int16_t>(stream);
    for (int i = 0; i < unkShortsBCount; i++)
        unkShortsB.push_back(ReadValue<int16_t>(stream));

    if (header.shapeDataOffset == 0)
    {
        if (header.shapeType != ShapeType::PointShape)
            throw MSBFormatError("Region shape data offset is zero.");
    }
    else if (header.shapeType == ShapeType::PointShape)
        throw MSBFormatError("Region shape data offset is non-zero for Point shape.");
    else
    {
        stream.seekg(start + header.shapeDataOffset);
        shape->DeserializeShapeData(stream);

        if (header.shapeType == ShapeType::CompositeShape)
        {
            // Region stores child references for composite shape. (Deserialize method above did nothing.)
            // The references are resolved along with all other MSB references.
            compositeChildren = make_unique<CompositeShapeReferences>();
            for (int i = 0; i < 8; i++)
            {
                compositeChildren->regionIndices[i] = ReadValue<int32_t>(stream);
                compositeChildren->unk04s[i] = ReadValue<int32_t>(stream);
            }
        }
    }

    stream.seekg(start + header.supertypeDataOffset);
    const auto supertypeData = ReadValidatedStruct<RegionDataStruct>(stream);
    attachedPartIndex = supertypeData.attachedPartIndex;
    m_entityId = supertypeData.entityId;
    dUnk08 = supertypeData.unk08;

    if (header.subtypeDataOffset > 0)
    {
        stream.seekg(start + header.subtypeDataOffset);
        if (!DeserializeSubtypeData(stream))
        {
            throw MSBFormatError("Region " + m_name + " has non-zero subtype data offset, but no data was expected.");
        }
    }
    else if (DeserializeSubtypeData(stream))
    {
        throw MSBFormatError("Region " + m_name + " has no subtype data offset, but data was expected.");
    }
}


void Region::Serialize(ofstream& stream, const int supertypeIndex, const int subtypeIndex) const
{
    const streampos start = stream.tellp();

    Reserver reserver(stream, true);

    reserver.ReserveValidatedStruct("RegionHeader", sizeof(RegionHeaderStruct));
    auto header = RegionHeaderStruct
    {
        .subtype = GetType(),
        .subtypeIndex = subtypeIndex,
        .shapeType = GetShapeType(),
        .translate = translate,
        .rotate = rotate,
        .supertypeIndex = supertypeIndex,
        .unk40 = hUnk40,
        .eventLayer = eventLayer,
    };

    header.nameOffset = stream.tellp() - start;
    WriteUTF16String(stream, m_name);
    AlignStream(stream, 4);

    header.unkShortsAOffset = stream.tellp() - start;
    WriteValue(stream, static_cast<int16_t>(unkShortsA.size()));
    for (int16_t unkShort : unkShortsA)
    {
        WriteValue(stream, unkShort);
    }
    AlignStream(stream, 4);

    header.unkShortsBOffset = stream.tellp() - start;
    WriteValue(stream, static_cast<int16_t>(unkShortsB.size()));
    for (int16_t unkShort : unkShortsB)
    {
        WriteValue(stream, unkShort);
    }
    AlignStream(stream, 4);

    // Check if Shape is not `nullptr` and not a `Point`.
    if (shape && shape->GetType() != ShapeType::PointShape)
    {
        header.shapeDataOffset = stream.tellp() - start;
        shape->SerializeShapeData(stream);
    }
    else
    {
        header.shapeDataOffset = 0;
    }

    header.supertypeDataOffset = stream.tellp() - start;
    const RegionDataStruct supertypeData
    {
        .attachedPartIndex = attachedPartIndex,
        .entityId = m_entityId,
        .unk08 = dUnk08,
    };
    WriteValidatedStruct(stream, supertypeData);

    // Padding varies across subtypes here, unfortunately. Later (newer?) subtypes align to 8 here.
    if (GetType() > RegionType::BuddySummonPoint && GetType() != RegionType::Other)
    {
        AlignStream(stream, 8);
    }

    const streampos subtypeDataOffset = stream.tellp();
    if (SerializeSubtypeData(stream))
    {
        header.subtypeDataOffset = subtypeDataOffset - start;
    }
    else
    {
        header.subtypeDataOffset = 0;
    }

    // Older region types align here instead.
    if (GetType() <= RegionType::BuddySummonPoint || GetType() == RegionType::Other)
    {
        AlignStream(stream, 8);
    }

    header.extraDataOffset = stream.tellp() - start;
    const RegionExtraDataStruct extraData
    {
        .mapId = mapId,
        .unk04 = eUnk04,
        .unk0C = eUnk0C,
    };
    WriteValidatedStruct(stream, extraData);

    reserver.FillValidatedStruct("RegionHeader", header);

    AlignStream(stream, 8);

    reserver.Finish();
}


void Region::DeserializeEntryReferences(const std::vector<Region*>& regions, const std::vector<Part*>& parts)
{
    if (compositeChildren)
        compositeChildren->SetReferences(regions);
    attachedPart.SetFromIndex(parts, attachedPartIndex);
}

void Region::SerializeEntryIndices(const std::vector<Region*>& regions, const std::vector<Part*>& parts)
{
    if (compositeChildren)
        compositeChildren->SetIndices(this, regions);
    attachedPartIndex = attachedPart.ToIndex(this, parts);
}


bool InvasionPointRegion::DeserializeSubtypeData(ifstream& stream)
{
    // Not bothering with struct.
    priority = ReadValue<int>(stream);
    return true;
}


bool InvasionPointRegion::SerializeSubtypeData(ofstream& stream) const
{
    // Not bothering with struct.
    WriteValue(stream, priority);
    return true;
}


struct EnvironmentMapPointRegionData
{
    float unk00;
    int unk04;
    bool unk0D;
    bool unk0E;
    bool unk0F;
    float unk10;
    float unk14;
    int mapId;
    int unk20;
    int unk24;
    int unk28;
    uint8_t unk2C;
    uint8_t unk2D;

    static  void Validate() {}
};


bool EnvironmentMapPointRegion::DeserializeSubtypeData(ifstream& stream)
{
    const auto subtypeData = ReadValidatedStruct<EnvironmentMapPointRegionData>(stream);
    sUnk00 = subtypeData.unk00;
    sUnk04 = subtypeData.unk04;
    sUnk0D = subtypeData.unk0D;
    sUnk0E = subtypeData.unk0E;
    sUnk0F = subtypeData.unk0F;
    sUnk10 = subtypeData.unk10;
    sUnk14 = subtypeData.unk14;
    sMapId = subtypeData.mapId;
    sUnk20 = subtypeData.unk20;
    sUnk24 = subtypeData.unk24;
    sUnk28 = subtypeData.unk28;
    sUnk2C = subtypeData.unk2C;
    sUnk2D = subtypeData.unk2D;
    return true;
}


bool EnvironmentMapPointRegion::SerializeSubtypeData(ofstream& stream) const
{
    const auto subtypeData = EnvironmentMapPointRegionData
    {
        .unk00 = sUnk00,
        .unk04 = sUnk04,
        .unk0D = sUnk0D,
        .unk0E = sUnk0E,
        .unk0F = sUnk0F,
        .unk10 = sUnk10,
        .unk14 = sUnk14,
        .mapId = sMapId,
        .unk20 = sUnk20,
        .unk24 = sUnk24,
        .unk28 = sUnk28,
        .unk2C = sUnk2C,
        .unk2D = sUnk2D,
    };
    WriteValidatedStruct(stream, subtypeData);
    return true;
}


struct SoundRegionData
{
    int soundType;
    int soundId;
    array<int, 16> childRegionsIndices;
    uint8_t _zero;
    bool unk49;
    padding<2> _pad1;

    void Validate() const
    {
        AssertValue("SoundRegionStruct._zero", 0, _zero);
        AssertPadding("SoundRegionStruct", _pad1);
    }
};


bool SoundRegion::DeserializeSubtypeData(ifstream& stream)
{
    auto subtypeData = ReadValidatedStruct<SoundRegionData>(stream);
    soundType = subtypeData.soundType;
    soundId = subtypeData.soundId;
    ranges::copy(subtypeData.childRegionsIndices, childRegionsIndices.begin());
    sUnk49 = subtypeData.unk49;
    return true;
}


bool SoundRegion::SerializeSubtypeData(ofstream& stream) const
{
    const SoundRegionData subtypeData =
    {
        .soundType = soundType,
        .soundId = soundId,
        .childRegionsIndices = childRegionsIndices,
        ._zero = 0,
        .unk49 = sUnk49,
    };
    WriteValidatedStruct(stream, subtypeData);
    return true;
}


void SoundRegion::DeserializeEntryReferences(const std::vector<Region*>& regions, const std::vector<Part*>& parts)
{
    Region::DeserializeEntryReferences(regions, parts);
    SetReferenceArray(childRegions, childRegionsIndices, regions);
}


void SoundRegion::SerializeEntryIndices(const std::vector<Region*>& regions, const std::vector<Part*>& parts)
{
    Region::SerializeEntryIndices(regions, parts);
    SetIndexArray(this, childRegionsIndices, childRegions, regions);
}



bool VFXRegion::DeserializeSubtypeData(ifstream& stream)
{
    effectId = ReadValue<int>(stream);
    sUnk04 = ReadValue<int>(stream);
    return true;
}


bool VFXRegion::SerializeSubtypeData(ofstream& stream) const
{
    WriteValue(stream, effectId);
    WriteValue(stream, sUnk04);
    return true;
}


bool WindVFXRegion::DeserializeSubtypeData(ifstream& stream)
{
    effectId = ReadValue<int>(stream);
    windRegionIndex = ReadValue<int>(stream);
    sUnk08 = ReadValue<float>(stream);
    return true;
}


bool WindVFXRegion::SerializeSubtypeData(ofstream& stream) const
{
    WriteValue(stream, effectId);
    WriteValue(stream, windRegionIndex);
    WriteValue(stream, sUnk08);
    return true;
}


void WindVFXRegion::DeserializeEntryReferences(const std::vector<Region*>& regions, const std::vector<Part*>& parts)
{
    Region::DeserializeEntryReferences(regions, parts);
    windRegion.SetFromIndex(regions, windRegionIndex);
}


void WindVFXRegion::SerializeEntryIndices(const std::vector<Region*>& regions, const std::vector<Part*>& parts)
{
    Region::SerializeEntryIndices(regions, parts);
    windRegionIndex = windRegion.ToIndex(this, regions);
}


struct SpawnPointRegionData
{
    int _minusOne;
    padding<3> _pad1;

    void Validate() const
    {
        AssertValue("SpawnPointRegionData._minusOne", -1, _minusOne);
        AssertPadding("SpawnPointRegionData", _pad1);
    }
};


bool SpawnPointRegion::DeserializeSubtypeData(ifstream& stream)
{
    ReadValidatedStruct<SpawnPointRegionData>(stream);
    return true;
}


bool SpawnPointRegion::SerializeSubtypeData(ofstream& stream) const
{
    constexpr auto subtypeData = SpawnPointRegionData
    {
        ._minusOne = -1,
    };
    WriteValidatedStruct(stream, subtypeData);
    return true;
}


struct MessageRegionData
{
    int16_t messageId;
    int16_t unk02;
    int hidden;  // 32-bit bool
    int unk08;
    int unk0C;
    uint32_t enableEventFlagId;
    int characterModelName;
    int characterId;
    int animationId;
    int playerId;

    void Validate() const
    {
        if (hidden != 0 && hidden != 1)
            throw MSBFormatError("MessageRegionData.hidden is not a valid boolean value (0 or 1).");
    }
};


bool MessageRegion::DeserializeSubtypeData(ifstream& stream)
{
    const auto subtypeData = ReadValidatedStruct<MessageRegionData>(stream);
    messageId = subtypeData.messageId;
    sUnk02 = subtypeData.unk02;
    hidden = subtypeData.hidden == 1;
    sUnk08 = subtypeData.unk08;
    sUnk0C = subtypeData.unk0C;
    enableEventFlagId = subtypeData.enableEventFlagId;
    characterModelName = subtypeData.characterModelName;
    characterId = subtypeData.characterId;
    animationId = subtypeData.animationId;
    playerId = subtypeData.playerId;
    return true;
}


bool MessageRegion::SerializeSubtypeData(ofstream& stream) const
{
    const auto subtypeData = MessageRegionData
    {
        .messageId = messageId,
        .unk02 = sUnk02,
        .hidden = hidden,
        .unk08 = sUnk08,
        .unk0C = sUnk0C,
        .enableEventFlagId = enableEventFlagId,
        .characterModelName = characterModelName,
        .characterId = characterId,
        .animationId = animationId,
        .playerId = playerId,
    };
    WriteValidatedStruct(stream, subtypeData);
    return true;
}


struct EnvironmentMapEffectBoxRegionData
{
    float enableDist;
    float transitionDist;
    uint8_t unk08;
    uint8_t unk09;
    int16_t unk0A;
    padding<0x18> _pad1;
    float unk24;
    float unk28;
    int16_t unk2C;
    bool unk2E;
    bool unk2F;
    int16_t unk30;
    uint8_t _zero;
    bool unk33;
    int16_t unk34;
    int16_t unk36;
    padding<4> _pad2;

    void Validate() const
    {
        AssertValue("EnvironmentMapEffectBoxRegionData._zero", 0, _zero);
        AssertPadding("EnvironmentMapEffectBoxRegionData", _pad1, _pad2);
    }
};


bool EnvironmentMapEffectBoxRegion::DeserializeSubtypeData(ifstream& stream)
{
    const auto subtypeData = ReadValidatedStruct<EnvironmentMapEffectBoxRegionData>(stream);
    enableDist = subtypeData.enableDist;
    transitionDist = subtypeData.transitionDist;
    sUnk08 = subtypeData.unk08;
    sUnk09 = subtypeData.unk09;
    sUnk0A = subtypeData.unk0A;
    sUnk24 = subtypeData.unk24;
    sUnk28 = subtypeData.unk28;
    sUnk2C = subtypeData.unk2C;
    sUnk2E = subtypeData.unk2E;
    sUnk2f = subtypeData.unk2F;
    sUnk30 = subtypeData.unk30;
    sUnk33 = subtypeData.unk33;
    sUnk34 = subtypeData.unk34;
    sUnk36 = subtypeData.unk36;
    return true;
}


bool EnvironmentMapEffectBoxRegion::SerializeSubtypeData(ofstream& stream) const
{
    const auto subtypeData = EnvironmentMapEffectBoxRegionData
    {
        .enableDist = enableDist,
        .transitionDist = transitionDist,
        .unk08 = sUnk08,
        .unk09 = sUnk09,
        .unk0A = sUnk0A,
        .unk24 = sUnk24,
        .unk28 = sUnk28,
        .unk2C = sUnk2C,
        .unk2E = sUnk2E,
        .unk2F = sUnk2f,
        .unk30 = sUnk30,
        ._zero = 0,
        .unk33 = sUnk33,
        .unk34 = sUnk34,
        .unk36 = sUnk36,
    };
    WriteValidatedStruct(stream, subtypeData);
    return true;
}


struct ConnectionRegionData
{
    array<int8_t, 4> targetMapId;
    padding<12> _pad1;

    void Validate() const
    {
        AssertPadding("ConnectionRegionData", _pad1);
    }
};


bool ConnectionRegion::DeserializeSubtypeData(ifstream& stream)
{
    auto subtypeData = ReadValidatedStruct<ConnectionRegionData>(stream);
    ranges::copy(subtypeData.targetMapId, targetMapId.begin());
    return true;
}


bool ConnectionRegion::SerializeSubtypeData(ofstream& stream) const
{
    const ConnectionRegionData subtypeData =
    {
        .targetMapId = targetMapId,
    };
    WriteValidatedStruct(stream, subtypeData);
    return true;
}


struct PatrolRoute22RegionData
{
    int _minusOne;
    int _zero;

    void Validate() const
    {
        AssertValue("PatrolRoute22RegionData._minusOne", -1, _minusOne);
        AssertValue("PatrolRoute22RegionData._zero", 0, _zero);
    }
};


bool PatrolRoute22Region::DeserializeSubtypeData(ifstream& stream)
{
    ReadValidatedStruct<PatrolRoute22RegionData>(stream);
    return true;
}


bool PatrolRoute22Region::SerializeSubtypeData(ofstream& stream) const
{
    constexpr auto subtypeData = PatrolRoute22RegionData
    {
        ._minusOne = -1,
        ._zero = 0,
    };
    WriteValidatedStruct(stream, subtypeData);
    return true;
}


bool BuddySummonPointRegion::DeserializeSubtypeData(ifstream& stream)
{
    // Not bothering with struct.
    AssertReadPadBytes(stream, 16, "BuddySummonPointRegion subtype padding");
    return true;
}


bool BuddySummonPointRegion::SerializeSubtypeData(ofstream& stream) const
{
    // Not bothering with struct.
    WritePadBytes(stream, 16);
    return true;
}


struct MufflingBoxRegionData
{
    int unk00;
    padding<20> _pad1;
    int _x24_32;
    padding<8> _pad2;
    float unk24;
    padding<12> _pad3;
    float unk34;
    padding<4> _pad4;
    float unk3c;
    float unk40;
    float unk44;

    void Validate() const
    {
        AssertValue("MufflingBoxRegionData._x24_32", 32, _x24_32);
        AssertPadding("MufflingBoxRegionData", _pad1, _pad2, _pad3, _pad4);
    }
};


bool MufflingBoxRegion::DeserializeSubtypeData(ifstream& stream)
{
    const auto subtypeData = ReadValidatedStruct<MufflingBoxRegionData>(stream);
    sUnk00 = subtypeData.unk00;
    sUnk24 = subtypeData.unk24;
    sUnk34 = subtypeData.unk34;
    sUnk3c = subtypeData.unk3c;
    sUnk40 = subtypeData.unk40;
    sUnk44 = subtypeData.unk44;
    return true;
}


bool MufflingBoxRegion::SerializeSubtypeData(ofstream& stream) const
{
    const auto subtypeData = MufflingBoxRegionData
    {
        .unk00 = sUnk00,
        ._x24_32 = 32,
        .unk24 = sUnk24,
        .unk34 = sUnk34,
        .unk3c = sUnk3c,
        .unk40 = sUnk40,
        .unk44 = sUnk44,
    };
    WriteValidatedStruct(stream, subtypeData);
    return true;
}


struct MufflingPortalRegionData
{
    int unk00;
    padding<20> _pad1;
    int _x24_32;
    padding<24> _pad2;
    int _minusOne;

    void Validate() const
    {
        if (_x24_32 != 32)
            throw MSBFormatError("MufflingPortalRegionData._32 is not 32.");
        if (_minusOne != -1)
            throw MSBFormatError("MufflingPortalRegionData._minusOne is not -1.");
        AssertPadding("MufflingPortalRegionData", _pad1, _pad2);
    }
};


bool MufflingPortalRegion::DeserializeSubtypeData(ifstream& stream)
{
    const auto subtypeData = ReadValidatedStruct<MufflingPortalRegionData>(stream);
    sUnk00 = subtypeData.unk00;
    return true;
}


bool MufflingPortalRegion::SerializeSubtypeData(ofstream& stream) const
{
    const auto subtypeData = MufflingPortalRegionData
    {
        .unk00 = sUnk00,
        ._x24_32 = 32,
        ._minusOne = -1,
    };
    WriteValidatedStruct(stream, subtypeData);
    return true;
}


struct OtherSoundRegionData
{
    uint8_t unk00;
    uint8_t unk01;
    uint8_t unk02;
    uint8_t unk03;
    int unk04;
    short unk08;
    short unk0a;
    uint8_t unk0c;
    padding<19> _pad1;

    void Validate() const
    {
        AssertPadding("OtherSoundRegionData", _pad1);
    }
};


bool OtherSoundRegion::DeserializeSubtypeData(ifstream& stream)
{
    const auto subtypeData = ReadValidatedStruct<OtherSoundRegionData>(stream);
    sUnk00 = subtypeData.unk00;
    sUnk01 = subtypeData.unk01;
    sUnk02 = subtypeData.unk02;
    sUnk03 = subtypeData.unk03;
    sUnk04 = subtypeData.unk04;
    sUnk08 = subtypeData.unk08;
    sUnk0a = subtypeData.unk0a;
    sUnk0c = subtypeData.unk0c;
    return true;
}


bool OtherSoundRegion::SerializeSubtypeData(ofstream& stream) const
{
    const auto subtypeData = OtherSoundRegionData
    {
        .unk00 = sUnk00,
        .unk01 = sUnk01,
        .unk02 = sUnk02,
        .unk03 = sUnk03,
        .unk04 = sUnk04,
        .unk08 = sUnk08,
        .unk0a = sUnk0a,
        .unk0c = sUnk0c,
    };
    WriteValidatedStruct(stream, subtypeData);
    return true;
}


bool PatrolRouteRegion::DeserializeSubtypeData(ifstream& stream)
{
    // Not bothering with struct.
    sUnk00 = ReadValue<int>(stream);
    return true;
}


bool PatrolRouteRegion::SerializeSubtypeData(ofstream& stream) const
{
    // Not bothering with struct.
    WriteValue(stream, sUnk00);
    return true;
}


struct MapPointRegionData
{
    int unk00;
    int unk04;
    float unk08;
    float unk0C;
    int _minusOne;
    float unk14;
    float unk18;
    int _zero;

    void Validate() const
    {
        AssertValue("MapPointRegionData._minusOne", -1, _minusOne);
        AssertValue("MapPointRegionData._zero", 0, _zero);
    }
};


bool MapPointRegion::DeserializeSubtypeData(ifstream& stream)
{
    const auto subtypeData = ReadValidatedStruct<MapPointRegionData>(stream);
    sUnk00 = subtypeData.unk00;
    sUnk04 = subtypeData.unk04;
    sUnk08 = subtypeData.unk08;
    sUnk0C = subtypeData.unk0C;
    sUnk14 = subtypeData.unk14;
    sUnk18 = subtypeData.unk18;
    return true;
}


bool MapPointRegion::SerializeSubtypeData(ofstream& stream) const
{
    const auto subtypeData = MapPointRegionData
    {
        .unk00 = sUnk00,
        .unk04 = sUnk04,
        .unk08 = sUnk08,
        .unk0C = sUnk0C,
        ._minusOne = -1,
        .unk14 = sUnk14,
        .unk18 = sUnk18,
        ._zero = 0,
    };
    WriteValidatedStruct(stream, subtypeData);
    return true;
}


struct WeatherOverrideRegionData
{
    int weatherLotId;
    int _minusOne;
    padding<24> _pad1;

    void Validate() const
    {
        AssertValue("WeatherOverrideRegionData._minusOne", -1, _minusOne);
        AssertPadding("WeatherOverrideRegionData", _pad1);
    }
};


bool WeatherOverrideRegion::DeserializeSubtypeData(ifstream& stream)
{
    const auto subtypeData = ReadValidatedStruct<WeatherOverrideRegionData>(stream);
    weatherLotId = subtypeData.weatherLotId;
    return true;
}


bool WeatherOverrideRegion::SerializeSubtypeData(ofstream& stream) const
{
    const auto subtypeData = WeatherOverrideRegionData
    {
        .weatherLotId = weatherLotId,
        ._minusOne = -1,
    };
    WriteValidatedStruct(stream, subtypeData);
    return true;
}


struct AutoDrawGroupPointRegionData
{
    int unk00;
    padding<28> _pad1;

    void Validate() const
    {
        AssertPadding("AutoDrawGroupPointRegionData", _pad1);
    }
};


bool AutoDrawGroupPointRegion::DeserializeSubtypeData(ifstream& stream)
{
    const auto subtypeData = ReadValidatedStruct<AutoDrawGroupPointRegionData>(stream);
    sUnk00 = subtypeData.unk00;
    return true;
}


bool AutoDrawGroupPointRegion::SerializeSubtypeData(ofstream& stream) const
{
    const auto subtypeData = AutoDrawGroupPointRegionData
    {
        .unk00 = sUnk00,
    };
    WriteValidatedStruct(stream, subtypeData);
    return true;
}


struct GroupDefeatRewardRegionData
{
    int unk00;
    int unk04;
    int unk08;
    padding<8> _minusOnes0;
    array<int, 8> groupPartsIndices;
    int unk34;
    int unk38;
    padding<24> _minusOnes1;
    int unk54;
    padding<8> _pad1;

    void Validate() const
    {
        if (!IsPaddingValid(_minusOnes0, -1))
            throw MSBFormatError("GroupDefeatRewardRegionData._minusOnes0 is not all -1.");
        if (!IsPaddingValid(_minusOnes1, -1))
            throw MSBFormatError("GroupDefeatRewardRegionData._minusOnes1 is not all -1.");
        AssertPadding("GroupDefeatRewardRegionData", _pad1);
    }
};


bool GroupDefeatRewardRegion::DeserializeSubtypeData(ifstream& stream)
{
    auto subtypeData = ReadValidatedStruct<GroupDefeatRewardRegionData>(stream);
    sUnk00 = subtypeData.unk00;
    sUnk04 = subtypeData.unk04;
    sUnk08 = subtypeData.unk08;
    ranges::copy(subtypeData.groupPartsIndices, groupPartsIndices.begin());
    sUnk34 = subtypeData.unk34;
    sUnk38 = subtypeData.unk38;
    sUnk54 = subtypeData.unk54;
    return true;
}


bool GroupDefeatRewardRegion::SerializeSubtypeData(ofstream& stream) const
{
    auto subtypeData = GroupDefeatRewardRegionData
    {
        .unk00 = sUnk00,
        .unk04 = sUnk04,
        .unk08 = sUnk08,
        .groupPartsIndices = groupPartsIndices,
        .unk34 = sUnk34,
        .unk38 = sUnk38,
        .unk54 = sUnk54,
    };
    subtypeData._minusOnes0.fill(-1);
    subtypeData._minusOnes1.fill(-1);
    WriteValidatedStruct(stream, subtypeData);
    return true;
}


void GroupDefeatRewardRegion::DeserializeEntryReferences(const std::vector<Region*>& regions, const std::vector<Part*>& parts)
{
    Region::DeserializeEntryReferences(regions, parts);
    SetReferenceArray(groupParts, groupPartsIndices, parts);
}


void GroupDefeatRewardRegion::SerializeEntryIndices(const std::vector<Region*>& regions, const std::vector<Part*>& parts)
{
    Region::SerializeEntryIndices(regions, parts);
    SetIndexArray(this, groupPartsIndices,  groupParts, parts);
}


bool HitsetRegion::DeserializeSubtypeData(ifstream& stream)
{
    // Not bothering with struct.
    sUnk00 = ReadValue<int>(stream);
    return true;
}


bool HitsetRegion::SerializeSubtypeData(ofstream& stream) const
{
    // Not bothering with struct.
    WriteValue(stream, sUnk00);
    return true;
}


bool FastTravelRestrictionRegion::DeserializeSubtypeData(ifstream& stream)
{
    // Not bothering with struct.
    eventFlagId = ReadValue<int>(stream);
    AssertReadValue<int>(stream, 0, "FastTravelRestrictionRegion[0x4]");
    return true;
}


bool FastTravelRestrictionRegion::SerializeSubtypeData(ofstream& stream) const
{
    // Not bothering with struct.
    WriteValue(stream, eventFlagId);
    WriteValue(stream, 0);
    return true;
}


bool WeatherCreateAssetPointRegion::DeserializeSubtypeData(ifstream& stream)
{
    // Not bothering with struct.
    AssertReadValue<int>(stream, 0, "WeatherCreateAssetPointRegion[0x0]");
    return true;
}


bool WeatherCreateAssetPointRegion::SerializeSubtypeData(ofstream& stream) const
{
    // Not bothering with struct.
    WriteValue(stream, 0);
    return true;
}


bool PlayAreaRegion::DeserializeSubtypeData(ifstream& stream)
{
    // Not bothering with struct.
    sUnk00 = ReadValue<int>(stream);
    sUnk04 = ReadValue<int>(stream);
    return true;
}


bool PlayAreaRegion::SerializeSubtypeData(ofstream& stream) const
{
    // Not bothering with struct.
    WriteValue(stream, sUnk00);
    WriteValue(stream, sUnk04);
    return true;
}


bool MountJumpRegion::DeserializeSubtypeData(ifstream& stream)
{
    // Not bothering with struct.
    jumpHeight = ReadValue<float>(stream);
    sUnk04 = ReadValue<int>(stream);
    return true;
}


bool MountJumpRegion::SerializeSubtypeData(ofstream& stream) const
{
    // Not bothering with struct.
    WriteValue(stream, jumpHeight);
    WriteValue(stream, sUnk04);
    return true;
}


bool DummyRegion::DeserializeSubtypeData(ifstream& stream)
{
    // Not bothering with struct.
    sUnk00 = ReadValue<int>(stream);
    AssertReadValue<int>(stream, 0, "DummyRegion[0x4");
    return true;
}


bool DummyRegion::SerializeSubtypeData(ofstream& stream) const
{
    // Not bothering with struct.
    WriteValue(stream, sUnk00);
    WriteValue(stream, 0);
    return true;
}


bool FallPreventionRemovalRegion::DeserializeSubtypeData(ifstream& stream)
{
    // Not bothering with struct.
    AssertReadValue<int>(stream, 0, "FallPreventionRemovalRegion[0x0]");
    AssertReadValue<int>(stream, 0, "FallPreventionRemovalRegion[0x4]");
    return true;
}


bool FallPreventionRemovalRegion::SerializeSubtypeData(ofstream& stream) const
{
    // Not bothering with struct.
    WriteValue(stream, 0);
    WriteValue(stream, 0);
    return true;
}


bool NavmeshCuttingRegion::DeserializeSubtypeData(ifstream& stream)
{
    // Not bothering with struct.
    AssertReadValue<int>(stream, 0, "NavmeshCuttingRegion[0x0]");
    AssertReadValue<int>(stream, 0, "NavmeshCuttingRegion[0x4]");
    return true;
}


bool NavmeshCuttingRegion::SerializeSubtypeData(ofstream& stream) const
{
    // Not bothering with struct.
    WriteValue(stream, 0);
    WriteValue(stream, 0);
    return true;
}


bool MapNameOverrideRegion::DeserializeSubtypeData(ifstream& stream)
{
    // Not bothering with struct.
    mapNameId = ReadValue<int>(stream);
    AssertReadValue<int>(stream, 0, "MapNameOverrideRegion[0x4]");
    return true;
}


bool MapNameOverrideRegion::SerializeSubtypeData(ofstream& stream) const
{
    // Not bothering with struct.
    WriteValue(stream, mapNameId);
    WriteValue(stream, 0);
    return true;
}


bool MountJumpFallRegion::DeserializeSubtypeData(ifstream& stream)
{
    // Not bothering with struct.
    AssertReadValue<int>(stream, -1, "MountJumpFallRegion[0x0]");
    AssertReadValue<int>(stream, 0, "MountJumpFallRegion[0x4]");
    return true;
}


bool MountJumpFallRegion::SerializeSubtypeData(ofstream& stream) const
{
    // Not bothering with struct.
    WriteValue(stream, -1);
    WriteValue(stream, 0);
    return true;
}


bool HorseRideOverrideRegion::DeserializeSubtypeData(ifstream& stream)
{
    // Not bothering with struct.
    overrideType = static_cast<HorseRideOverrideType>(ReadValue<int>(stream));
    AssertReadValue<int>(stream, 0, "HorseRideOverrideRegion[0x4]");
    return true;
}


bool HorseRideOverrideRegion::SerializeSubtypeData(ofstream& stream) const
{
    // Not bothering with struct.
    WriteValue(stream, static_cast<int>(overrideType));
    WriteValue(stream, 0);
    return true;
}
