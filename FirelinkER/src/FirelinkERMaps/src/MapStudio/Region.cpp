#include "MSBBufferHelpers.h"
#include <FirelinkCore/BinaryReadWrite.h>
#include <FirelinkCore/BinaryValidation.h>
#include <FirelinkCore/Collections.h>
#include <FirelinkERMaps/MapStudio/MSBFormatError.h>
#include <FirelinkERMaps/MapStudio/Region.h>
#include <FirelinkERMaps/MapStudio/Shape.h>

#include <cstdint>
#include <string>

using namespace Firelink;
using namespace Firelink::BinaryReadWrite;
using namespace Firelink::BinaryValidation;
using namespace Firelink::EldenRing::Maps::MapStudio::BufferHelpers;
using namespace Firelink::EldenRing::Maps;
using namespace Firelink::EldenRing::Maps::MapStudio;

struct RegionHeader
{
    int64_t nameOffset;
    RegionType subtype;
    int32_t subtypeIndex;
    ShapeType shapeType;
    Vector3 translate;
    Vector3 rotate; // Euler angles in radians
    int32_t supertypeIndex;
    int64_t unkShortsAOffset;
    int64_t unkShortsBOffset;
    int32_t unk40;
    int32_t eventLayer; // Map ceremony layer
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
        AssertNotValue("RegionHeaderStruct.extraDataOffset", 0, extraDataOffset);
    }
};

struct RegionDataStruct
{
    int32_t attachedPartIndex;
    uint32_t entityId;
    uint8_t unk08; // usually 0xFF
    padding<7> _pad1;

    void Validate() const { AssertPadding("RegionDataStruct", _pad1); }
};

struct RegionExtraDataStruct
{
    int32_t mapId; // usually 0xFFFFFFFF
    int32_t unk04;
    padding<4> _pad1;
    int32_t unk0C; // usually 0xFFFFFFFF
    padding<16> _pad2;

    void Validate() const { AssertPadding("RegionExtraDataStruct", _pad1, _pad2); }
};

std::unique_ptr<Shape>& Region::SetShapeType(const ShapeType shapeType)
{
    switch (shapeType)
    {
        case ShapeType::NoShape:
            // Same effect as Point.
            m_shape = nullptr;
            return m_shape;
        case ShapeType::PointShape:
            m_shape = std::make_unique<Point>();
            return m_shape;
        case ShapeType::CircleShape:
            m_shape = std::make_unique<Circle>();
            return m_shape;
        case ShapeType::SphereShape:
            m_shape = std::make_unique<Sphere>();
            return m_shape;
        case ShapeType::CylinderShape:
            m_shape = std::make_unique<Cylinder>();
            return m_shape;
        case ShapeType::RectangleShape:
            m_shape = std::make_unique<Rectangle>();
            return m_shape;
        case ShapeType::BoxShape:
            m_shape = std::make_unique<Box>();
            return m_shape;
        case ShapeType::CompositeShape:
            m_shape = std::make_unique<Composite>();
            m_compositeChildren = std::make_unique<CompositeShapeReferences>();
            return m_shape;
    }

    throw std::invalid_argument("Invalid shape type.");
}

void Region::Deserialize(BufferReader& reader)
{
    const size_t start = reader.Position();

    const auto header = BufferHelpers::ReadValidatedStruct<RegionHeader>(reader);
    if (header.subtype != GetType())
        throw std::invalid_argument("Region header/class subtype mismatch.");

    SetShapeType(header.shapeType);
    m_translate = header.translate;
    m_rotate = header.rotate;
    this->m_hUnk40 = header.unk40;

    reader.Seek(start + header.nameOffset);
    m_name = BufferHelpers::ReadUTF16String(reader);

    reader.Seek(start + header.unkShortsAOffset);
    const auto unkShortsACount = reader.Read<int16_t>();
    for (int i = 0; i < unkShortsACount; i++)
        m_unkShortsA.push_back(reader.Read<int16_t>());

    reader.Seek(start + header.unkShortsBOffset);
    const auto unkShortsBCount = reader.Read<int16_t>();
    for (int i = 0; i < unkShortsBCount; i++)
        m_unkShortsB.push_back(reader.Read<int16_t>());

    // Info(std::format("Region {} has name, unkA, unkB, supertype offsets at: 0x{:X}, 0x{:X}, 0x{:X}, 0x{:X}.",
    // GetNameUTF8(), header.nameOffset, header.unkShortsAOffset, header.unkShortsBOffset, header.supertypeDataOffset));

    if (header.shapeDataOffset == 0)
    {
        if (header.shapeType != ShapeType::PointShape)
            throw MSBFormatError("Region shape data offset is zero.");
        // Info(std::format("   No shape offset (point)."));
    }
    else if (header.shapeType == ShapeType::PointShape)
        throw MSBFormatError("Region shape data offset is non-zero for Point shape.");
    else
    {
        reader.Seek(start + header.shapeDataOffset);
        // Info(std::format("   Shape offset: 0x{:X}.", header.shapeDataOffset));
        m_shape->DeserializeShapeData(reader);

        if (header.shapeType == ShapeType::CompositeShape)
        {
            // Region stores child references for composite shape. (Deserialize method above did nothing.)
            // The references are resolved along with all other MSB references.
            m_compositeChildren = std::make_unique<CompositeShapeReferences>();
            for (int i = 0; i < 8; i++)
            {
                m_compositeChildren->regionIndices[i] = reader.Read<int32_t>();
                m_compositeChildren->unk04s[i] = reader.Read<int32_t>();
            }
        }
    }

    reader.Seek(start + header.supertypeDataOffset);
    const auto supertypeData = BufferHelpers::ReadValidatedStruct<RegionDataStruct>(reader);
    m_attachedPartIndex = supertypeData.attachedPartIndex;
    m_entityId = supertypeData.entityId;
    m_dUnk08 = supertypeData.unk08;

    if (header.subtypeDataOffset > 0)
    {
        reader.Seek(start + header.subtypeDataOffset);
        if (!DeserializeSubtypeData(reader))
        {
            throw MSBFormatError(
                "Region " + GetNameUTF8() + " has non-zero subtype data offset, but no data was expected.");
        }
    }
    else if (DeserializeSubtypeData(reader))
    {
        throw MSBFormatError("Region " + GetNameUTF8() + " has no subtype data offset, but data was expected.");
    }

    if (header.extraDataOffset == 0)
        throw MSBFormatError("Region " + GetNameUTF8() + " has zero extra data offset.");

    reader.Seek(start + header.extraDataOffset);
    const auto extraData = BufferHelpers::ReadValidatedStruct<RegionExtraDataStruct>(reader);
    m_mapId = extraData.mapId;
    m_eUnk04 = extraData.unk04;
    m_eUnk0C = extraData.unk0C;
}

void Region::Serialize(BufferWriter& writer, const int supertypeIndex, const int subtypeIndex) const
{
    const size_t start = writer.Position();

    const void* scope = this;
    writer.Reserve<RegionHeader>("RegionHeader", scope);
    auto header = RegionHeader{
        .subtype = GetType(),
        .subtypeIndex = subtypeIndex,
        .shapeType = GetShapeType(),
        .translate = m_translate,
        .rotate = m_rotate,
        .supertypeIndex = supertypeIndex,
        .unk40 = m_hUnk40,
        .eventLayer = m_eventLayer,
    };

    header.nameOffset = static_cast<int64_t>(writer.Position() - start);
    BufferHelpers::WriteUTF16String(writer, m_name);
    writer.PadAlign(4);

    header.unkShortsAOffset = static_cast<int64_t>(writer.Position() - start);
    writer.Write(static_cast<int16_t>(m_unkShortsA.size()));
    for (int16_t unkShort : m_unkShortsA)
        writer.Write(unkShort);
    writer.PadAlign(4);

    header.unkShortsBOffset = static_cast<int64_t>(writer.Position() - start);
    writer.Write(static_cast<int16_t>(m_unkShortsB.size()));
    for (int16_t unkShort : m_unkShortsB)
        writer.Write(unkShort);
    writer.PadAlign(4);

    // Align to 8 before next data (be it shape or supertype).
    writer.PadAlign(8);

    // Check if Shape is not `nullptr` and not a `Point`.
    if (m_shape && m_shape->GetType() != ShapeType::PointShape)
    {
        header.shapeDataOffset = static_cast<int64_t>(writer.Position() - start);
        if (m_shape->GetType() == ShapeType::CompositeShape)
        {
            // Write composite child shape references/unknowns.
            // (`Shape::SerializeShapeData()` does nothing for Composite shapes.)
            for (int i = 0; i < 8; i++)
            {
                writer.Write(m_compositeChildren->regionIndices[i]);
                writer.Write(m_compositeChildren->unk04s[i]);
            }
        }
        else
        {
            m_shape->SerializeShapeData(writer);
        }
    }
    else
    {
        header.shapeDataOffset = 0;
    }

    header.supertypeDataOffset = static_cast<int64_t>(writer.Position() - start);
    // Info(std::format("Writing Region '{}' supertype data at offset 0x{:X}.", string(*this),
    // static_cast<int64_t>(stream.tellp())));
    const RegionDataStruct supertypeData{
        .attachedPartIndex = m_attachedPartIndex,
        .entityId = m_entityId,
        .unk08 = m_dUnk08,
    };
    BufferHelpers::WriteValidatedStruct(writer, supertypeData);

    // Padding varies across subtypes here, unfortunately. Later (newer?) subtypes align to 8 here.
    if (GetType() > RegionType::BuddySummonPoint && GetType() != RegionType::Other)
    {
        writer.PadAlign(8);
    }

    const size_t subtypeDataOffset = writer.Position();
    // Info(std::format("  Writing Region '{}' subtype data at offset 0x{:X}.", string(*this),
    // static_cast<int64_t>(writer.Position())));
    if (SerializeSubtypeData(writer))
        header.subtypeDataOffset = static_cast<int64_t>(subtypeDataOffset - start);
    else
        header.subtypeDataOffset = 0;

    // Older region types align here instead.
    if (GetType() <= RegionType::BuddySummonPoint || GetType() == RegionType::Other)
        writer.PadAlign(8);

    header.extraDataOffset = static_cast<int64_t>(writer.Position() - start);
    // Info(std::format("  Writing Region '{}' extra data at offset 0x{:X}.", string(*this),
    // static_cast<int64_t>(stream.tellp()))); Info(std::format("  m_mapId = {}, m_eUnk04 = {}, m_eUnk0C = {}", m_mapId,
    // m_eUnk04, m_eUnk0C));
    const RegionExtraDataStruct extraData{
        .mapId = m_mapId,
        .unk04 = m_eUnk04,
        .unk0C = m_eUnk0C,
    };
    BufferHelpers::WriteValidatedStruct(writer, extraData);

    header.Validate();
    writer.Fill<RegionHeader>("RegionHeader", header, scope);

    writer.PadAlign(8);

    
}

void Region::DeserializeEntryReferences(const std::vector<Region*>& regions, const std::vector<Part*>& parts)
{
    if (m_compositeChildren)
        m_compositeChildren->SetReferences(regions);
    m_attachedPart.SetFromIndex(parts, m_attachedPartIndex);
}

void Region::SerializeEntryIndices(const std::vector<Region*>& regions, const std::vector<Part*>& parts)
{
    if (m_compositeChildren)
        m_compositeChildren->SetIndices(this, regions);
    m_attachedPartIndex = m_attachedPart.ToIndex(this, parts);
}

bool InvasionPointRegion::DeserializeSubtypeData(BufferReader& reader)
{
    // Not bothering with struct.
    m_priority = reader.Read<int>();
    return true;
}

bool InvasionPointRegion::SerializeSubtypeData(BufferWriter& writer) const
{
    // Not bothering with struct.
    writer.Write(m_priority);
    return true;
}

struct EnvironmentMapPointRegionData
{
    float unk00;
    int unk04;
    int _minusOne;
    uint8_t _zero;
    bool unk0D;
    bool unk0E;
    bool unk0F;
    float unk10;
    float unk14;
    int mapId;
    padding<4> _pad1;
    int unk20;
    int unk24;
    int unk28;
    uint8_t unk2C;
    uint8_t unk2D;
    padding<2> _pad2;

    void Validate() const
    {
        AssertValue("EnvironmentMapPointRegionData._minusOne", -1, _minusOne);
        AssertValue("EnvironmentMapPointRegionData._zero", 0, _zero);
        AssertPadding("EnvironmentMapPointRegionData", _pad1, _pad2);
    }
};

bool EnvironmentMapPointRegion::DeserializeSubtypeData(BufferReader& reader)
{
    const auto subtypeData = BufferHelpers::ReadValidatedStruct<EnvironmentMapPointRegionData>(reader);
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

bool EnvironmentMapPointRegion::SerializeSubtypeData(BufferWriter& writer) const
{
    const auto subtypeData = EnvironmentMapPointRegionData{
        .unk00 = sUnk00,
        .unk04 = sUnk04,
        ._minusOne = -1,
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
    BufferHelpers::WriteValidatedStruct(writer, subtypeData);
    return true;
}

struct SoundRegionData
{
    int soundType;
    int soundId;
    std::array<int, 16> childRegionsIndices;
    uint8_t _zero;
    bool unk49;
    padding<2> _pad1;

    void Validate() const
    {
        AssertValue("SoundRegionStruct._zero", 0, _zero);
        AssertPadding("SoundRegionStruct", _pad1);
    }
};

bool SoundRegion::DeserializeSubtypeData(BufferReader& reader)
{
    auto subtypeData = BufferHelpers::ReadValidatedStruct<SoundRegionData>(reader);
    soundType = subtypeData.soundType;
    soundId = subtypeData.soundId;
    std::ranges::copy(subtypeData.childRegionsIndices, childRegionsIndices.begin());
    sUnk49 = subtypeData.unk49;
    return true;
}

bool SoundRegion::SerializeSubtypeData(BufferWriter& writer) const
{
    const SoundRegionData subtypeData = {
        .soundType = soundType,
        .soundId = soundId,
        .childRegionsIndices = childRegionsIndices,
        ._zero = 0,
        .unk49 = sUnk49,
    };
    BufferHelpers::WriteValidatedStruct(writer, subtypeData);
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

bool VFXRegion::DeserializeSubtypeData(BufferReader& reader)
{
    effectId = reader.Read<int>();
    sUnk04 = reader.Read<int>();
    return true;
}

bool VFXRegion::SerializeSubtypeData(BufferWriter& writer) const
{
    writer.Write(effectId);
    writer.Write(sUnk04);
    return true;
}

bool WindVFXRegion::DeserializeSubtypeData(BufferReader& reader)
{
    effectId = reader.Read<int>();
    windRegionIndex = reader.Read<int>();
    sUnk08 = reader.Read<float>();
    return true;
}

bool WindVFXRegion::SerializeSubtypeData(BufferWriter& writer) const
{
    writer.Write(effectId);
    writer.Write(windRegionIndex);
    writer.Write(sUnk08);
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
    padding<12> _pad1;

    void Validate() const
    {
        AssertValue("SpawnPointRegionData._minusOne", -1, _minusOne);
        AssertPadding("SpawnPointRegionData", _pad1);
    }
};

bool SpawnPointRegion::DeserializeSubtypeData(BufferReader& reader)
{
    BufferHelpers::ReadValidatedStruct<SpawnPointRegionData>(reader);
    return true;
}

bool SpawnPointRegion::SerializeSubtypeData(BufferWriter& writer) const
{
    constexpr auto subtypeData = SpawnPointRegionData{
        ._minusOne = -1,
    };
    BufferHelpers::WriteValidatedStruct(writer, subtypeData);
    return true;
}

struct MessageRegionData
{
    int16_t messageId;
    int16_t unk02;
    int hidden; // 32-bit bool
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

bool MessageRegion::DeserializeSubtypeData(BufferReader& reader)
{
    const auto subtypeData = BufferHelpers::ReadValidatedStruct<MessageRegionData>(reader);
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

bool MessageRegion::SerializeSubtypeData(BufferWriter& writer) const
{
    const auto subtypeData = MessageRegionData{
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
    BufferHelpers::WriteValidatedStruct(writer, subtypeData);
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

bool EnvironmentMapEffectBoxRegion::DeserializeSubtypeData(BufferReader& reader)
{
    const auto subtypeData = BufferHelpers::ReadValidatedStruct<EnvironmentMapEffectBoxRegionData>(reader);
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

bool EnvironmentMapEffectBoxRegion::SerializeSubtypeData(BufferWriter& writer) const
{
    const auto subtypeData = EnvironmentMapEffectBoxRegionData{
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
    BufferHelpers::WriteValidatedStruct(writer, subtypeData);
    return true;
}

struct ConnectionRegionData
{
    std::array<int8_t, 4> targetMapId;
    padding<12> _pad1;

    void Validate() const { AssertPadding("ConnectionRegionData", _pad1); }
};

bool ConnectionRegion::DeserializeSubtypeData(BufferReader& reader)
{
    auto subtypeData = BufferHelpers::ReadValidatedStruct<ConnectionRegionData>(reader);
    std::ranges::copy(subtypeData.targetMapId, targetMapId.begin());
    return true;
}

bool ConnectionRegion::SerializeSubtypeData(BufferWriter& writer) const
{
    const ConnectionRegionData subtypeData = {
        .targetMapId = targetMapId,
    };
    BufferHelpers::WriteValidatedStruct(writer, subtypeData);
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

bool PatrolRoute22Region::DeserializeSubtypeData(BufferReader& reader)
{
    BufferHelpers::ReadValidatedStruct<PatrolRoute22RegionData>(reader);
    return true;
}

bool PatrolRoute22Region::SerializeSubtypeData(BufferWriter& writer) const
{
    constexpr auto subtypeData = PatrolRoute22RegionData{
        ._minusOne = -1,
        ._zero = 0,
    };
    BufferHelpers::WriteValidatedStruct(writer, subtypeData);
    return true;
}

bool BuddySummonPointRegion::DeserializeSubtypeData(BufferReader& reader)
{
    // Not bothering with struct.
    reader.AssertPad(16);
    return true;
}

bool BuddySummonPointRegion::SerializeSubtypeData(BufferWriter& writer) const
{
    // Not bothering with struct.
    writer.WritePad(16);
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

bool MufflingBoxRegion::DeserializeSubtypeData(BufferReader& reader)
{
    const auto subtypeData = BufferHelpers::ReadValidatedStruct<MufflingBoxRegionData>(reader);
    sUnk00 = subtypeData.unk00;
    sUnk24 = subtypeData.unk24;
    sUnk34 = subtypeData.unk34;
    sUnk3c = subtypeData.unk3c;
    sUnk40 = subtypeData.unk40;
    sUnk44 = subtypeData.unk44;
    return true;
}

bool MufflingBoxRegion::SerializeSubtypeData(BufferWriter& writer) const
{
    const auto subtypeData = MufflingBoxRegionData{
        .unk00 = sUnk00,
        ._x24_32 = 32,
        .unk24 = sUnk24,
        .unk34 = sUnk34,
        .unk3c = sUnk3c,
        .unk40 = sUnk40,
        .unk44 = sUnk44,
    };
    BufferHelpers::WriteValidatedStruct(writer, subtypeData);
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

bool MufflingPortalRegion::DeserializeSubtypeData(BufferReader& reader)
{
    const auto subtypeData = BufferHelpers::ReadValidatedStruct<MufflingPortalRegionData>(reader);
    sUnk00 = subtypeData.unk00;
    return true;
}

bool MufflingPortalRegion::SerializeSubtypeData(BufferWriter& writer) const
{
    const auto subtypeData = MufflingPortalRegionData{
        .unk00 = sUnk00,
        ._x24_32 = 32,
        ._minusOne = -1,
    };
    BufferHelpers::WriteValidatedStruct(writer, subtypeData);
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

    void Validate() const { AssertPadding("OtherSoundRegionData", _pad1); }
};

bool OtherSoundRegion::DeserializeSubtypeData(BufferReader& reader)
{
    const auto subtypeData = BufferHelpers::ReadValidatedStruct<OtherSoundRegionData>(reader);
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

bool OtherSoundRegion::SerializeSubtypeData(BufferWriter& writer) const
{
    const auto subtypeData = OtherSoundRegionData{
        .unk00 = sUnk00,
        .unk01 = sUnk01,
        .unk02 = sUnk02,
        .unk03 = sUnk03,
        .unk04 = sUnk04,
        .unk08 = sUnk08,
        .unk0a = sUnk0a,
        .unk0c = sUnk0c,
    };
    BufferHelpers::WriteValidatedStruct(writer, subtypeData);
    return true;
}

bool PatrolRouteRegion::DeserializeSubtypeData(BufferReader& reader)
{
    // Not bothering with struct.
    sUnk00 = reader.Read<int>();
    return true;
}

bool PatrolRouteRegion::SerializeSubtypeData(BufferWriter& writer) const
{
    // Not bothering with struct.
    writer.Write(sUnk00);
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

bool MapPointRegion::DeserializeSubtypeData(BufferReader& reader)
{
    const auto subtypeData = BufferHelpers::ReadValidatedStruct<MapPointRegionData>(reader);
    sUnk00 = subtypeData.unk00;
    sUnk04 = subtypeData.unk04;
    sUnk08 = subtypeData.unk08;
    sUnk0C = subtypeData.unk0C;
    sUnk14 = subtypeData.unk14;
    sUnk18 = subtypeData.unk18;
    return true;
}

bool MapPointRegion::SerializeSubtypeData(BufferWriter& writer) const
{
    const auto subtypeData = MapPointRegionData{
        .unk00 = sUnk00,
        .unk04 = sUnk04,
        .unk08 = sUnk08,
        .unk0C = sUnk0C,
        ._minusOne = -1,
        .unk14 = sUnk14,
        .unk18 = sUnk18,
        ._zero = 0,
    };
    BufferHelpers::WriteValidatedStruct(writer, subtypeData);
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

bool WeatherOverrideRegion::DeserializeSubtypeData(BufferReader& reader)
{
    const auto subtypeData = BufferHelpers::ReadValidatedStruct<WeatherOverrideRegionData>(reader);
    weatherLotId = subtypeData.weatherLotId;
    return true;
}

bool WeatherOverrideRegion::SerializeSubtypeData(BufferWriter& writer) const
{
    const auto subtypeData = WeatherOverrideRegionData{
        .weatherLotId = weatherLotId,
        ._minusOne = -1,
    };
    BufferHelpers::WriteValidatedStruct(writer, subtypeData);
    return true;
}

struct AutoDrawGroupPointRegionData
{
    int unk00;
    padding<28> _pad1;

    void Validate() const { AssertPadding("AutoDrawGroupPointRegionData", _pad1); }
};

bool AutoDrawGroupPointRegion::DeserializeSubtypeData(BufferReader& reader)
{
    const auto subtypeData = BufferHelpers::ReadValidatedStruct<AutoDrawGroupPointRegionData>(reader);
    sUnk00 = subtypeData.unk00;
    return true;
}

bool AutoDrawGroupPointRegion::SerializeSubtypeData(BufferWriter& writer) const
{
    const auto subtypeData = AutoDrawGroupPointRegionData{
        .unk00 = sUnk00,
    };
    BufferHelpers::WriteValidatedStruct(writer, subtypeData);
    return true;
}

struct GroupDefeatRewardRegionData
{
    int unk00;
    int unk04;
    int unk08;
    padding<8> _minusOnes0;
    std::array<int, 8> groupPartsIndices;
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

bool GroupDefeatRewardRegion::DeserializeSubtypeData(BufferReader& reader)
{
    auto subtypeData = BufferHelpers::ReadValidatedStruct<GroupDefeatRewardRegionData>(reader);
    sUnk00 = subtypeData.unk00;
    sUnk04 = subtypeData.unk04;
    sUnk08 = subtypeData.unk08;
    std::ranges::copy(subtypeData.groupPartsIndices, groupPartsIndices.begin());
    sUnk34 = subtypeData.unk34;
    sUnk38 = subtypeData.unk38;
    sUnk54 = subtypeData.unk54;
    return true;
}

bool GroupDefeatRewardRegion::SerializeSubtypeData(BufferWriter& writer) const
{
    auto subtypeData = GroupDefeatRewardRegionData{
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
    BufferHelpers::WriteValidatedStruct(writer, subtypeData);
    return true;
}

void GroupDefeatRewardRegion::DeserializeEntryReferences(
    const std::vector<Region*>& regions, const std::vector<Part*>& parts)
{
    Region::DeserializeEntryReferences(regions, parts);
    SetReferenceArray(groupParts, groupPartsIndices, parts);
}

void GroupDefeatRewardRegion::SerializeEntryIndices(const std::vector<Region*>& regions, const std::vector<Part*>& parts)
{
    Region::SerializeEntryIndices(regions, parts);
    SetIndexArray(this, groupPartsIndices, groupParts, parts);
}

bool HitsetRegion::DeserializeSubtypeData(BufferReader& reader)
{
    // Not bothering with struct.
    sUnk00 = reader.Read<int>();
    return true;
}

bool HitsetRegion::SerializeSubtypeData(BufferWriter& writer) const
{
    // Not bothering with struct.
    writer.Write(sUnk00);
    return true;
}

bool FastTravelRestrictionRegion::DeserializeSubtypeData(BufferReader& reader)
{
    // Not bothering with struct.
    eventFlagId = reader.Read<int>();
    reader.AssertValue<int>(0, "FastTravelRestrictionRegion[0x4]");
    return true;
}

bool FastTravelRestrictionRegion::SerializeSubtypeData(BufferWriter& writer) const
{
    // Not bothering with struct.
    writer.Write(eventFlagId);
    writer.Write(0);
    return true;
}

bool WeatherCreateAssetPointRegion::DeserializeSubtypeData(BufferReader& reader)
{
    // Not bothering with struct.
    reader.AssertValue<int>(0, "WeatherCreateAssetPointRegion[0x0]");
    return true;
}

bool WeatherCreateAssetPointRegion::SerializeSubtypeData(BufferWriter& writer) const
{
    // Not bothering with struct.
    writer.Write(0);
    return true;
}

bool PlayAreaRegion::DeserializeSubtypeData(BufferReader& reader)
{
    // Not bothering with struct.
    sUnk00 = reader.Read<int>();
    sUnk04 = reader.Read<int>();
    return true;
}

bool PlayAreaRegion::SerializeSubtypeData(BufferWriter& writer) const
{
    // Not bothering with struct.
    writer.Write(sUnk00);
    writer.Write(sUnk04);
    return true;
}

bool MountJumpRegion::DeserializeSubtypeData(BufferReader& reader)
{
    // Not bothering with struct.
    jumpHeight = reader.Read<float>();
    sUnk04 = reader.Read<int>();
    return true;
}

bool MountJumpRegion::SerializeSubtypeData(BufferWriter& writer) const
{
    // Not bothering with struct.
    writer.Write(jumpHeight);
    writer.Write(sUnk04);
    return true;
}

bool DummyRegion::DeserializeSubtypeData(BufferReader& reader)
{
    // Not bothering with struct.
    sUnk00 = reader.Read<int>();
    reader.AssertValue<int>(0, "DummyRegion[0x4");
    return true;
}

bool DummyRegion::SerializeSubtypeData(BufferWriter& writer) const
{
    // Not bothering with struct.
    writer.Write(sUnk00);
    writer.Write(0);
    return true;
}

bool FallPreventionRemovalRegion::DeserializeSubtypeData(BufferReader& reader)
{
    // Not bothering with struct.
    reader.AssertValue<int>(0, "FallPreventionRemovalRegion[0x0]");
    reader.AssertValue<int>(0, "FallPreventionRemovalRegion[0x4]");
    return true;
}

bool FallPreventionRemovalRegion::SerializeSubtypeData(BufferWriter& writer) const
{
    // Not bothering with struct.
    writer.Write(0);
    writer.Write(0);
    return true;
}

bool NavmeshCuttingRegion::DeserializeSubtypeData(BufferReader& reader)
{
    // Not bothering with struct.
    reader.AssertValue<int>(0, "NavmeshCuttingRegion[0x0]");
    reader.AssertValue<int>(0, "NavmeshCuttingRegion[0x4]");
    return true;
}

bool NavmeshCuttingRegion::SerializeSubtypeData(BufferWriter& writer) const
{
    // Not bothering with struct.
    writer.Write(0);
    writer.Write(0);
    return true;
}

bool MapNameOverrideRegion::DeserializeSubtypeData(BufferReader& reader)
{
    // Not bothering with struct.
    mapNameId = reader.Read<int>();
    reader.AssertValue<int>(0, "MapNameOverrideRegion[0x4]");
    return true;
}

bool MapNameOverrideRegion::SerializeSubtypeData(BufferWriter& writer) const
{
    // Not bothering with struct.
    writer.Write(mapNameId);
    writer.Write(0);
    return true;
}

bool MountJumpFallRegion::DeserializeSubtypeData(BufferReader& reader)
{
    // Not bothering with struct.
    reader.AssertValue<int>(-1, "MountJumpFallRegion[0x0]");
    reader.AssertValue<int>(0, "MountJumpFallRegion[0x4]");
    return true;
}

bool MountJumpFallRegion::SerializeSubtypeData(BufferWriter& writer) const
{
    // Not bothering with struct.
    writer.Write(-1);
    writer.Write(0);
    return true;
}

bool HorseRideOverrideRegion::DeserializeSubtypeData(BufferReader& reader)
{
    // Not bothering with struct.
    overrideType = static_cast<HorseRideOverrideType>(reader.Read<int>());
    reader.AssertValue<int>(0, "HorseRideOverrideRegion[0x4]");
    return true;
}

bool HorseRideOverrideRegion::SerializeSubtypeData(BufferWriter& writer) const
{
    // Not bothering with struct.
    writer.Write(static_cast<int>(overrideType));
    writer.Write(0);
    return true;
}
