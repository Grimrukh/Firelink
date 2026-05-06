
#include "MSBBufferHelpers.h"
#include <FirelinkCore/BinaryReadWrite.h>
#include <FirelinkCore/BinaryValidation.h>
#include <FirelinkERMaps/MapStudio/Event.h>
#include <FirelinkERMaps/MapStudio/MSBFormatError.h>

using namespace Firelink::BinaryReadWrite;
using namespace Firelink::BinaryValidation;
using namespace Firelink::EldenRing::Maps::MapStudio::BufferHelpers;
using namespace Firelink::EldenRing::Maps::MapStudio;

struct EventHeader
{
    int64_t nameOffset;
    int32_t supertypeIndex;
    EventType subtype;
    int32_t subtypeIndex;
    int32_t unk14; // 0 or 1
    int64_t supertypeDataOffset;
    int64_t subtypeDataOffset;
    int64_t extraDataOffset;

    void Validate() const
    {
        if (unk14 != 0 && unk14 != 1)
            throw MSBFormatError("EventHeader: Expected unk14 to be 0 or 1, but got " + std::to_string(unk14));
    }
};

struct EventDataStruct
{
    int32_t attachedPartIndex;
    int32_t attachedRegionIndex;
    int32_t entityId;
    uint8_t unk0C;
    std::array<uint8_t, 3> _pad1;

    void Validate() const
    {
        if (!IsPaddingValid(_pad1))
            throw MSBFormatError("EventDataStruct: Expected pad1 to be all 0s.");
    }
};

struct EventExtraDataStruct
{
    int32_t mapId;
    int32_t unk04;
    int32_t unk08;
    int32_t unk0C;
    std::array<uint8_t, 16> _pad1;

    void Validate() const
    {
        if (!IsPaddingValid(_pad1))
            throw MSBFormatError("EventExtraDataStruct: Expected pad1 to be all 0s.");
    }
};

void Event::Deserialize(BufferReader& reader)
{
    const size_t start = reader.Position();
    const auto header = BufferHelpers::ReadValidatedStruct<EventHeader>(reader);

    if (header.nameOffset == 0)
        throw MSBFormatError("Event name offset is zero.");

    reader.Seek(start + header.nameOffset);
    m_name = BufferHelpers::ReadUTF16String(reader);

    // Firelink::Info(std::format("Deserializing Event '{}' at offset 0x{:X}.", GetNameUTF8(),
    // static_cast<int64_t>(start)));

    // Check type
    if (header.subtype != GetType())
        throw MSBFormatError(std::format("Event '{}' was loaded with incorrect subtype.", GetNameUTF8()));
    if (header.supertypeDataOffset == 0)
        throw MSBFormatError("Event supertype data offset is zero.");
    reader.Seek(start + header.supertypeDataOffset);
    const auto supertypeData = BufferHelpers::ReadValidatedStruct<EventDataStruct>(reader);
    attachedPartIndex = supertypeData.attachedPartIndex;
    attachedRegionIndex = supertypeData.attachedRegionIndex;
    entityId = supertypeData.entityId;
    dUnk0C = supertypeData.unk0C;

    if (header.subtypeDataOffset > 0)
    {
        reader.Seek(start + header.subtypeDataOffset);
        if (!DeserializeSubtypeData(reader))
            throw MSBFormatError(
                std::format("Event '{}' has non-zero subtype data offset, but no data was expected.", GetNameUTF8()));
    }
    else if (DeserializeSubtypeData(reader))
    {
        throw MSBFormatError(
            std::format("Event '{}' has zero subtype data offset, but data was expected.", GetNameUTF8()));
    }

    // Extra data always present.
    if (header.extraDataOffset == 0)
        throw MSBFormatError("Event extra data offset is zero.");
    reader.Seek(start + header.extraDataOffset);
    const auto extraData = BufferHelpers::ReadValidatedStruct<EventExtraDataStruct>(reader);
    mapId = extraData.mapId;
    eUnk04 = extraData.unk04;
    eUnk08 = extraData.unk08;
    eUnk0C = extraData.unk0C;
}

void Event::Serialize(BufferWriter& writer, const int supertypeIndex, const int subtypeIndex) const
{
    const size_t start = writer.Position();

    // Firelink::Info(std::format("Serializing Event '{}' at offset 0x{:X}.", GetNameUTF8(),
    // static_cast<int64_t>(start)));

        const void* scope = this;

    EventHeader header{
        .supertypeIndex = supertypeIndex,
        .subtype = GetType(),
        .subtypeIndex = subtypeIndexOverride != -1 ? subtypeIndexOverride : subtypeIndex,
        .unk14 = hUnk14,
    };
    writer.Reserve<EventHeader>("EventHeader", scope);

    header.nameOffset = static_cast<int64_t>(writer.Position() - start);
    BufferHelpers::WriteUTF16String(writer, m_name);
    writer.PadAlign(8);

    header.supertypeDataOffset = static_cast<int64_t>(writer.Position() - start);
    const EventDataStruct supertypeData{
        .attachedPartIndex = attachedPartIndex,
        .attachedRegionIndex = attachedRegionIndex,
        .entityId = entityId,
        .unk0C = dUnk0C,
    };
    BufferHelpers::WriteValidatedStruct(writer, supertypeData);

    const size_t subtypeDataOffset = writer.Position();
    if (SerializeSubtypeData(writer))
    { // yes subtype data
        header.subtypeDataOffset = static_cast<int64_t>(subtypeDataOffset - start);
    }
    else
    { // no subtype data
        header.subtypeDataOffset = 0;
    }

    header.extraDataOffset = static_cast<int64_t>(writer.Position() - start);
    const EventExtraDataStruct extraData{
        .mapId = mapId,
        .unk04 = eUnk04,
        .unk08 = eUnk08,
        .unk0C = eUnk0C,
    };
    BufferHelpers::WriteValidatedStruct(writer, extraData);

    writer.PadAlign(8);

    header.Validate();
    writer.Fill<EventHeader>("EventHeader", header, scope);

    
}

void Event::DeserializeEntryReferences(const std::vector<Part*>& parts, const std::vector<Region*>& regions)
{
    attachedPart.SetFromIndex(parts, attachedPartIndex);
    attachedRegion.SetFromIndex(regions, attachedRegionIndex);
}

void Event::SerializeEntryIndices(const std::vector<Part*>& parts, const std::vector<Region*>& regions)
{
    attachedPartIndex = attachedPart.ToIndex(this, parts);
    attachedRegionIndex = attachedRegion.ToIndex(this, regions);
}

struct TreasureEventData
{
    padding<8> pad1;
    int32_t treasurePartIndex;
    padding<4> pad2;
    int32_t itemLot;
    padding<0x24> pad3_xFF; // 0xFF
    int32_t actionButtonId;
    int32_t pickupAnimationId;
    bool inChest;
    bool startsDisabled;
    padding<14> pad4;

    void Validate() const
    {
        AssertPadding("TreasureEventData", pad1, pad2, pad4);
        if (!IsPaddingValid(pad3_xFF, 0xFF))
            throw MSBFormatError("TreasureEventData: Expected pad3_xFF to be all 0xFF.");
    }
};

bool TreasureEvent::DeserializeSubtypeData(BufferReader& reader)
{
    const auto data = BufferHelpers::ReadValidatedStruct<TreasureEventData>(reader);
    treasurePartIndex = data.treasurePartIndex;
    itemLot = data.itemLot;
    actionButtonId = data.actionButtonId;
    pickupAnimationId = data.pickupAnimationId;
    inChest = data.inChest;
    startsDisabled = data.startsDisabled;
    return true;
}

bool TreasureEvent::SerializeSubtypeData(BufferWriter& writer) const
{
    TreasureEventData data{
        .treasurePartIndex = treasurePartIndex,
        .itemLot = itemLot,
        .actionButtonId = actionButtonId,
        .pickupAnimationId = pickupAnimationId,
        .inChest = inChest,
        .startsDisabled = startsDisabled,
    };
    data.pad3_xFF.fill(0xFF);
    BufferHelpers::WriteValidatedStruct(writer, data);
    return true;
}

void TreasureEvent::DeserializeEntryReferences(const std::vector<Part*>& parts, const std::vector<Region*>& regions)
{
    Event::DeserializeEntryReferences(parts, regions);
    treasurePart.SetFromIndex(parts, treasurePartIndex);
}

void TreasureEvent::SerializeEntryIndices(const std::vector<Part*>& parts, const std::vector<Region*>& regions)
{
    Event::SerializeEntryIndices(parts, regions);
    treasurePartIndex = treasurePart.ToIndex(this, parts);
}

// Struct for binary representation of the event data
struct SpawnerEventData
{
    uint8_t maxCount;
    int8_t spawnerType;
    int16_t limitCount;
    int16_t minSpawnerCount;
    int16_t maxSpawnerCount;
    float minInterval;
    float maxInterval;
    uint8_t initialSpawnCount;
    padding<3> pad1;
    float spawnerUnk14;
    float spawnerUnk18;
    padding<0x14> pad2;
    int32_t spawnRegionsIndices[8];
    padding<0x10> pad3;
    int32_t spawnPartsIndices[32];
    padding<0x20> pad4;

    void Validate() const { AssertPadding("SpawnerEventData", pad1, pad2, pad3, pad4); }
};

// Read subtype-specific data from the stream
bool SpawnerEvent::DeserializeSubtypeData(BufferReader& reader)
{
    auto data = BufferHelpers::ReadValidatedStruct<SpawnerEventData>(reader);
    maxCount = data.maxCount;
    spawnerType = data.spawnerType;
    limitCount = data.limitCount;
    minSpawnerCount = data.minSpawnerCount;
    maxSpawnerCount = data.maxSpawnerCount;
    minInterval = data.minInterval;
    maxInterval = data.maxInterval;
    initialSpawnCount = data.initialSpawnCount;
    spawnerUnk14 = data.spawnerUnk14;
    spawnerUnk18 = data.spawnerUnk18;

    std::ranges::copy(data.spawnRegionsIndices, begin(spawnRegionsIndices));
    std::ranges::copy(data.spawnPartsIndices, begin(spawnPartsIndices));

    return true;
}

// Write subtype-specific data to the stream
bool SpawnerEvent::SerializeSubtypeData(BufferWriter& writer) const
{
    SpawnerEventData data{
        .maxCount = maxCount,
        .spawnerType = spawnerType,
        .limitCount = limitCount,
        .minSpawnerCount = minSpawnerCount,
        .maxSpawnerCount = maxSpawnerCount,
        .minInterval = minInterval,
        .maxInterval = maxInterval,
        .initialSpawnCount = initialSpawnCount,
        .spawnerUnk14 = spawnerUnk14,
        .spawnerUnk18 = spawnerUnk18,
    };
    std::ranges::copy(spawnRegionsIndices, std::begin(data.spawnRegionsIndices));
    std::ranges::copy(spawnPartsIndices, std::begin(data.spawnPartsIndices));

    BufferHelpers::WriteValidatedStruct(writer, data);
    return true;
}

void SpawnerEvent::DeserializeEntryReferences(const std::vector<Part*>& parts, const std::vector<Region*>& regions)
{
    Event::DeserializeEntryReferences(parts, regions);
    SetReferenceArray(spawnParts, spawnPartsIndices, parts);
    SetReferenceArray(spawnRegions, spawnRegionsIndices, regions);
}

void SpawnerEvent::SerializeEntryIndices(const std::vector<Part*>& parts, const std::vector<Region*>& regions)
{
    Event::SerializeEntryIndices(parts, regions);
    SetIndexArray(this, spawnPartsIndices, spawnParts, parts);
    SetIndexArray(this, spawnRegionsIndices, spawnRegions, regions);
}

struct NavigationEventData
{
    int32_t navigationRegionIndex;
    padding<12> pad1;

    void Validate() const { AssertPadding("NavigationEventData", pad1); }
};

bool NavigationEvent::DeserializeSubtypeData(BufferReader& reader)
{
    const auto data = BufferHelpers::ReadValidatedStruct<NavigationEventData>(reader);
    navigationRegionIndex = data.navigationRegionIndex;
    return true;
}

bool NavigationEvent::SerializeSubtypeData(BufferWriter& writer) const
{
    const NavigationEventData data{
        .navigationRegionIndex = navigationRegionIndex,
    };
    BufferHelpers::WriteValidatedStruct(writer, data);
    return true;
}

void NavigationEvent::DeserializeEntryReferences(const std::vector<Part*>& parts, const std::vector<Region*>& regions)
{
    Event::DeserializeEntryReferences(parts, regions);
    navigationRegion.SetFromIndex(regions, navigationRegionIndex);
}

void NavigationEvent::SerializeEntryIndices(const std::vector<Part*>& parts, const std::vector<Region*>& regions)
{
    Event::SerializeEntryIndices(parts, regions);
    navigationRegionIndex = navigationRegion.ToIndex(this, regions);
}

// Struct for binary representation of the event data
struct ObjActEventData
{
    int32_t objActEntityId;
    int32_t objActPartIndex;
    int32_t objActParamId;
    uint8_t objActState;
    padding<3> pad1;
    int32_t objActFlag;
    padding<12> pad2;

    void Validate() const { AssertPadding("ObjActEventData", pad1, pad2); }
};

// Read subtype-specific data from the stream
bool ObjActEvent::DeserializeSubtypeData(BufferReader& reader)
{
    const auto data = BufferHelpers::ReadValidatedStruct<ObjActEventData>(reader);
    objActEntityId = data.objActEntityId;
    objActPartIndex = data.objActPartIndex;
    objActParamId = data.objActParamId;
    objActState = data.objActState;
    objActFlag = data.objActFlag;
    return true;
}

// Write subtype-specific data to the stream
bool ObjActEvent::SerializeSubtypeData(BufferWriter& writer) const
{
    const ObjActEventData data{
        .objActEntityId = objActEntityId,
        .objActPartIndex = objActPartIndex,
        .objActParamId = objActParamId,
        .objActState = objActState,
        .objActFlag = objActFlag,
    };
    BufferHelpers::WriteValidatedStruct(writer, data);
    return true;
}

void ObjActEvent::DeserializeEntryReferences(const std::vector<Part*>& parts, const std::vector<Region*>& regions)
{
    Event::DeserializeEntryReferences(parts, regions);
    objActPart.SetFromIndex(parts, objActPartIndex);
}

void ObjActEvent::SerializeEntryIndices(const std::vector<Part*>& parts, const std::vector<Region*>& regions)
{
    Event::SerializeEntryIndices(parts, regions);
    objActPartIndex = objActPart.ToIndex(this, parts);
}

struct NPCInvasionEventData
{
    int32_t hostEntityId;
    int32_t invasionFlagId;
    int32_t activateGoodId;
    int32_t unk0c;
    int32_t unk10;
    int32_t unk14;
    int32_t unk18;
    int32_t unk1c;
    int32_t _minusOne;

    void Validate() const
    {
        if (_minusOne != -1)
            throw MSBFormatError("NPCInvasionEventData: Expected minusOne to be -1, but got " + std::to_string(_minusOne));
    }
};

// Read subtype-specific data from the stream
bool NPCInvasionEvent::DeserializeSubtypeData(BufferReader& reader)
{
    const auto data = BufferHelpers::ReadValidatedStruct<NPCInvasionEventData>(reader);
    hostEntityId = data.hostEntityId;
    invasionFlagId = data.invasionFlagId;
    activateGoodId = data.activateGoodId;
    unk0c = data.unk0c;
    unk10 = data.unk10;
    unk14 = data.unk14;
    unk18 = data.unk18;
    unk1c = data.unk1c;
    return true;
}

// Write subtype-specific data to the stream
bool NPCInvasionEvent::SerializeSubtypeData(BufferWriter& writer) const
{
    NPCInvasionEventData data = {};

    data.hostEntityId = hostEntityId;
    data.invasionFlagId = invasionFlagId;
    data.activateGoodId = activateGoodId;
    data.unk0c = unk0c;
    data.unk10 = unk10;
    data.unk14 = unk14;
    data.unk18 = unk18;
    data.unk1c = unk1c;
    data._minusOne = -1;

    BufferHelpers::WriteValidatedStruct(writer, data);
    return true;
}

struct PlatoonEventData
{
    int32_t platoonIdScriptActivate;
    int32_t state;
    padding<8> pad1;
    int32_t platoonPartsIndices[32];

    void Validate() const { AssertPadding("PlatoonEventData", pad1); }
};

// Read subtype-specific data from the stream
bool PlatoonEvent::DeserializeSubtypeData(BufferReader& reader)
{
    auto data = BufferHelpers::ReadValidatedStruct<PlatoonEventData>(reader);
    platoonIdScriptActivate = data.platoonIdScriptActivate;
    state = data.state;
    std::ranges::copy(data.platoonPartsIndices, begin(platoonPartsIndices));
    return true;
}

// Write subtype-specific data to the stream
bool PlatoonEvent::SerializeSubtypeData(BufferWriter& writer) const
{
    PlatoonEventData data = {
        .platoonIdScriptActivate = platoonIdScriptActivate,
        .state = state,
    };
    std::ranges::copy(platoonPartsIndices, std::begin(data.platoonPartsIndices));
    BufferHelpers::WriteValidatedStruct(writer, data);
    return true;
}

void PlatoonEvent::DeserializeEntryReferences(const std::vector<Part*>& parts, const std::vector<Region*>& regions)
{
    Event::DeserializeEntryReferences(parts, regions);
    SetReferenceArray(platoonParts, platoonPartsIndices, parts);
}

void PlatoonEvent::SerializeEntryIndices(const std::vector<Part*>& parts, const std::vector<Region*>& regions)
{
    Event::SerializeEntryIndices(parts, regions);
    SetIndexArray(this, platoonPartsIndices, platoonParts, parts);
}

struct PatrolRouteEventData
{
    uint8_t patrolType;
    padding<2> pad1;
    uint8_t _one;
    int32_t _minusOne;
    padding<8> pad2;
    std::array<int16_t, 64> patrolRegionsIndices;

    void Validate() const
    {
        if (_one != 1)
            throw MSBFormatError("PatrolRouteEventData: Expected 'one' to be 1, but got " + std::to_string(_one));
        if (_minusOne != -1)
            throw MSBFormatError("PatrolRouteEventData: Expected 'minusOne' to be -1, but got " + std::to_string(_minusOne));
        AssertPadding("PatrolRouteEventData", pad1, pad2);
    }
};

// Read subtype-specific data from the stream
bool PatrolRouteEvent::DeserializeSubtypeData(BufferReader& reader)
{
    auto data = BufferHelpers::ReadValidatedStruct<PatrolRouteEventData>(reader);
    patrolType = data.patrolType;
    std::ranges::copy(data.patrolRegionsIndices, begin(patrolRegionsIndices));
    return true;
}

// Write subtype-specific data to the stream
bool PatrolRouteEvent::SerializeSubtypeData(BufferWriter& writer) const
{
    PatrolRouteEventData data = {
        .patrolType = patrolType,
        ._one = 1,
        ._minusOne = -1,
    };
    std::ranges::copy(patrolRegionsIndices, begin(data.patrolRegionsIndices));

    BufferHelpers::WriteValidatedStruct(writer, data);
    return true;
}

void PatrolRouteEvent::DeserializeEntryReferences(const std::vector<Part*>& parts, const std::vector<Region*>& regions)
{
    Event::DeserializeEntryReferences(parts, regions);
    SetReferenceArray16(patrolRegions, patrolRegionsIndices, regions);
}

void PatrolRouteEvent::SerializeEntryIndices(const std::vector<Part*>& parts, const std::vector<Region*>& regions)
{
    Event::SerializeEntryIndices(parts, regions);
    SetIndexArray16(this, patrolRegionsIndices, patrolRegions, regions);
}

struct MountEventData
{
    int32_t riderPartIndex;
    int32_t mountPartIndex;

    // Nothing to validate.
    static void Validate() {}
};

bool MountEvent::DeserializeSubtypeData(BufferReader& reader)
{
    const auto data = BufferHelpers::ReadValidatedStruct<MountEventData>(reader);
    riderPartIndex = data.riderPartIndex;
    mountPartIndex = data.mountPartIndex;
    return true;
}

bool MountEvent::SerializeSubtypeData(BufferWriter& writer) const
{
    const MountEventData data = {
        .riderPartIndex = riderPartIndex,
        .mountPartIndex = mountPartIndex,
    };
    BufferHelpers::WriteValidatedStruct(writer, data);
    return true;
}

void MountEvent::DeserializeEntryReferences(const std::vector<Part*>& parts, const std::vector<Region*>& regions)
{
    Event::DeserializeEntryReferences(parts, regions);
    riderPart.SetFromIndex(parts, riderPartIndex);
    mountPart.SetFromIndex(parts, mountPartIndex);
}

void MountEvent::SerializeEntryIndices(const std::vector<Part*>& parts, const std::vector<Region*>& regions)
{
    Event::SerializeEntryIndices(parts, regions);
    riderPartIndex = riderPart.ToIndex(this, parts);
    mountPartIndex = mountPart.ToIndex(this, parts);
}

// SignPoolEvent
struct SignPoolEventData
{
    int32_t signPartIndex;
    int32_t signPoolUnk04;
    padding<8> pad1;

    void Validate() const { AssertPadding("SignPoolEventData", pad1); }
};

bool SignPoolEvent::DeserializeSubtypeData(BufferReader& reader)
{
    const auto data = BufferHelpers::ReadValidatedStruct<SignPoolEventData>(reader);
    signPartIndex = data.signPartIndex;
    signPoolUnk04 = data.signPoolUnk04;
    return true;
}

bool SignPoolEvent::SerializeSubtypeData(BufferWriter& writer) const
{
    const SignPoolEventData data = {
        .signPartIndex = signPartIndex,
        .signPoolUnk04 = signPoolUnk04,
    };
    BufferHelpers::WriteValidatedStruct(writer, data);
    return true;
}

void SignPoolEvent::DeserializeEntryReferences(const std::vector<Part*>& parts, const std::vector<Region*>& regions)
{
    Event::DeserializeEntryReferences(parts, regions);
    signPart.SetFromIndex(parts, signPartIndex);
}

void SignPoolEvent::SerializeEntryIndices(const std::vector<Part*>& parts, const std::vector<Region*>& regions)
{
    Event::SerializeEntryIndices(parts, regions);
    signPartIndex = signPart.ToIndex(this, parts);
}

struct RetryPointEventData
{
    int32_t retryPartIndex;
    int32_t eventFlagId;
    float retryPointUnk08;
    int16_t retryRegionIndex;
    int16_t _zero;

    void Validate() const
    {
        if (_zero != 0)
            throw MSBFormatError("RetryPointEventData: Expected 'zero' to be 0, but got " + std::to_string(_zero));
    }
};

bool RetryPointEvent::DeserializeSubtypeData(BufferReader& reader)
{
    const auto data = BufferHelpers::ReadValidatedStruct<RetryPointEventData>(reader);
    retryPartIndex = data.retryPartIndex;
    eventFlagId = data.eventFlagId;
    retryPointUnk08 = data.retryPointUnk08;
    retryRegionIndex = data.retryRegionIndex;
    return true;
}

bool RetryPointEvent::SerializeSubtypeData(BufferWriter& writer) const
{
    if (retryRegionIndex > 0xFFFF)
        throw MSBFormatError("RetryPointEvent: Retry region index is too large.");

    const RetryPointEventData data = {
        .retryPartIndex = retryPartIndex,
        .eventFlagId = eventFlagId,
        .retryPointUnk08 = retryPointUnk08,
        .retryRegionIndex = retryRegionIndex,
    };
    BufferHelpers::WriteValidatedStruct(writer, data);
    return true;
}

void RetryPointEvent::DeserializeEntryReferences(const std::vector<Part*>& parts, const std::vector<Region*>& regions)
{
    Event::DeserializeEntryReferences(parts, regions);
    retryPart.SetFromIndex(parts, retryPartIndex);
    retryRegion.SetFromIndex(regions, retryRegionIndex);
}

void RetryPointEvent::SerializeEntryIndices(const std::vector<Part*>& parts, const std::vector<Region*>& regions)
{
    Event::SerializeEntryIndices(parts, regions);
    retryPartIndex = retryPart.ToIndex(this, parts);
}

struct AreaTeamEventData
{
    int32_t leaderEntityId;
    int32_t sUnk04;
    int32_t sUnk08;
    int32_t sUnk0C;
    int32_t leaderRegionEntityId;
    int32_t guest1RegionEntityId;
    int32_t guest2RegionEntityId;
    int32_t sUnk1C;
    int32_t sUnk20;
    int32_t sUnk24;
    int32_t sUnk28;

    // Nothing to validate.
    static void Validate() {}
};

bool AreaTeamEvent::DeserializeSubtypeData(BufferReader& reader)
{
    const auto data = BufferHelpers::ReadValidatedStruct<AreaTeamEventData>(reader);
    leaderEntityId = data.leaderEntityId;
    sUnk04 = data.sUnk04;
    sUnk08 = data.sUnk08;
    sUnk0C = data.sUnk0C;
    leaderRegionEntityId = data.leaderRegionEntityId;
    guest1RegionEntityId = data.guest1RegionEntityId;
    guest2RegionEntityId = data.guest2RegionEntityId;
    sUnk1C = data.sUnk1C;
    sUnk20 = data.sUnk20;
    sUnk24 = data.sUnk24;
    sUnk28 = data.sUnk28;
    return true;
}

bool AreaTeamEvent::SerializeSubtypeData(BufferWriter& writer) const
{
    const AreaTeamEventData data = {
        .leaderEntityId = leaderEntityId,
        .sUnk04 = sUnk04,
        .sUnk08 = sUnk08,
        .sUnk0C = sUnk0C,
        .leaderRegionEntityId = leaderRegionEntityId,
        .guest1RegionEntityId = guest1RegionEntityId,
        .guest2RegionEntityId = guest2RegionEntityId,
        .sUnk1C = sUnk1C,
        .sUnk20 = sUnk20,
        .sUnk24 = sUnk24,
        .sUnk28 = sUnk28,
    };
    BufferHelpers::WriteValidatedStruct(writer, data);
    return true;
}
