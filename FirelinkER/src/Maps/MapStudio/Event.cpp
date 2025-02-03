
#include "FirelinkER/Maps/MapStudio/Event.h"
#include "Firelink/BinaryReadWrite.h"
#include "Firelink/BinaryValidation.h"
#include "FirelinkER/Maps/MapStudio/MSBFormatError.h"

using namespace std;
using namespace Firelink::BinaryReadWrite;
using namespace Firelink::BinaryValidation;
using namespace FirelinkER::Maps::MapStudio;


struct EventHeader
{
    int64_t nameOffset;
    int32_t supertypeIndex;
    EventType subtype;
    int32_t subtypeIndex;
    int32_t unk14;  // 0 or 1
    int64_t supertypeDataOffset;
    int64_t subtypeDataOffset;
    int64_t extraDataOffset;

    void Validate() const
    {
        if (unk14 != 0 && unk14 != 1)
            throw MSBFormatError("EventHeader: Expected unk14 to be 0 or 1, but got " + to_string(unk14));
    }
};

struct EventDataStruct
{
    int32_t attachedPartIndex;
    int32_t attachedRegionIndex;
    int32_t entityId;
    uint8_t unk0C;
    array<uint8_t, 3> _pad1;

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
    array<uint8_t, 16> _pad1;

    void Validate() const
    {
        if (!IsPaddingValid(_pad1))
            throw MSBFormatError("EventExtraDataStruct: Expected pad1 to be all 0s.");
    }
};


void Event::Deserialize(ifstream& stream)
{
    const streampos start = stream.tellg();
    const auto header = ReadValidatedStruct<EventHeader>(stream);

    if (header.nameOffset == 0)
        throw MSBFormatError("Event name offset is zero.");

    stream.seekg(start + header.nameOffset);
    m_name = ReadUTF16String(stream);

    // Firelink::Info(format("Deserializing Event '{}' at offset 0x{:X}.", GetNameUTF8(), static_cast<int64_t>(start)));

    // Check type
    if (header.subtype != GetType())
        throw MSBFormatError(format("Event '{}' was loaded with incorrect subtype.", GetNameUTF8()));
    if (header.supertypeDataOffset == 0)
        throw MSBFormatError("Event supertype data offset is zero.");
    stream.seekg(start + header.supertypeDataOffset);
    const auto supertypeData = ReadValidatedStruct<EventDataStruct>(stream);
    attachedPartIndex = supertypeData.attachedPartIndex;
    attachedRegionIndex = supertypeData.attachedRegionIndex;
    entityId = supertypeData.entityId;
    dUnk0C = supertypeData.unk0C;

    if (header.subtypeDataOffset > 0)
    {
        stream.seekg(start + header.subtypeDataOffset);
        if (!DeserializeSubtypeData(stream))
            throw MSBFormatError(format(
                "Event '{}' has non-zero subtype data offset, but no data was expected.", GetNameUTF8()));
    }
    else if (DeserializeSubtypeData(stream))
    {
        throw MSBFormatError(format(
        "Event '{}' has zero subtype data offset, but data was expected.", GetNameUTF8()));
    }

    // Extra data always present.
    if (header.extraDataOffset == 0)
        throw MSBFormatError("Event extra data offset is zero.");
    stream.seekg(start + header.extraDataOffset);
    const auto extraData = ReadValidatedStruct<EventExtraDataStruct>(stream);
    mapId = extraData.mapId;
    eUnk04 = extraData.unk04;
    eUnk08 = extraData.unk08;
    eUnk0C = extraData.unk0C;
}


void Event::Serialize(ofstream &stream, const int supertypeIndex, const int subtypeIndex) const
{
    const streampos start = stream.tellp();

    // Firelink::Info(format("Serializing Event '{}' at offset 0x{:X}.", GetNameUTF8(), static_cast<int64_t>(start)));

    Reserver reserver(stream, true);

    EventHeader header
    {
        .supertypeIndex = supertypeIndex,
        .subtype = GetType(),
        .subtypeIndex = subtypeIndexOverride != -1 ? subtypeIndexOverride : subtypeIndex,
        .unk14 = hUnk14,
    };
    reserver.ReserveValidatedStruct("EventHeader", sizeof(EventHeader));

    header.nameOffset = stream.tellp() - start;
    WriteUTF16String(stream, m_name);
    AlignStream(stream, 8);

    header.supertypeDataOffset = stream.tellp() - start;
    const EventDataStruct supertypeData
    {
        .attachedPartIndex = attachedPartIndex,
        .attachedRegionIndex = attachedRegionIndex,
        .entityId = entityId,
        .unk0C = dUnk0C,
    };
    WriteValidatedStruct(stream, supertypeData);

    const streampos subtypeDataOffset = stream.tellp();
    if (SerializeSubtypeData(stream)) {  // yes subtype data
        header.subtypeDataOffset = subtypeDataOffset - start;
    }
    else {  // no subtype data
        header.subtypeDataOffset = 0;
    }

    header.extraDataOffset = stream.tellp() - start;
    const EventExtraDataStruct extraData
    {
        .mapId = mapId,
        .unk04 = eUnk04,
        .unk08 = eUnk08,
        .unk0C = eUnk0C,
    };
    WriteValidatedStruct(stream, extraData);

    AlignStream(stream, 8);

    reserver.FillValidatedStruct("EventHeader", header);

    reserver.Finish();
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
    padding<0x24> pad3_xFF;  // 0xFF
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


bool TreasureEvent::DeserializeSubtypeData(ifstream &stream)
{
    const auto data = ReadValidatedStruct<TreasureEventData>(stream);
    treasurePartIndex = data.treasurePartIndex;
    itemLot = data.itemLot;
    actionButtonId = data.actionButtonId;
    pickupAnimationId = data.pickupAnimationId;
    inChest = data.inChest;
    startsDisabled = data.startsDisabled;
    return true;
}


bool TreasureEvent::SerializeSubtypeData(ofstream &stream) const
{
    TreasureEventData data
    {
        .treasurePartIndex = treasurePartIndex,
        .itemLot = itemLot,
        .actionButtonId = actionButtonId,
        .pickupAnimationId = pickupAnimationId,
        .inChest = inChest,
        .startsDisabled = startsDisabled,
    };
    data.pad3_xFF.fill(0xFF);
    WriteValidatedStruct(stream, data);
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

    void Validate() const
    {
        AssertPadding("SpawnerEventData", pad1, pad2, pad3, pad4);
    }
};

// Read subtype-specific data from the stream
bool SpawnerEvent::DeserializeSubtypeData(ifstream& stream)
{
    auto data = ReadValidatedStruct<SpawnerEventData>(stream);
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

    ranges::copy(data.spawnRegionsIndices, begin(spawnRegionsIndices));
    ranges::copy(data.spawnPartsIndices, begin(spawnPartsIndices));

    return true;
}

// Write subtype-specific data to the stream
bool SpawnerEvent::SerializeSubtypeData(ofstream& stream) const
{
    SpawnerEventData data
    {
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
    ranges::copy(spawnRegionsIndices, begin(data.spawnRegionsIndices));
    ranges::copy(spawnPartsIndices, begin(data.spawnPartsIndices));

    WriteValidatedStruct(stream, data);
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

    void Validate() const
    {
        AssertPadding("NavigationEventData", pad1);
    }
};


bool NavigationEvent::DeserializeSubtypeData(ifstream &stream)
{
    const auto data = ReadValidatedStruct<NavigationEventData>(stream);
    navigationRegionIndex = data.navigationRegionIndex;
    return true;
}


bool NavigationEvent::SerializeSubtypeData(ofstream &stream) const
{
    const NavigationEventData data
    {
        .navigationRegionIndex = navigationRegionIndex,
    };
    WriteValidatedStruct(stream, data);
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

    void Validate() const
    {
        AssertPadding("ObjActEventData", pad1, pad2);
    }
};

// Read subtype-specific data from the stream
bool ObjActEvent::DeserializeSubtypeData(ifstream& stream)
{
    const auto data = ReadValidatedStruct<ObjActEventData>(stream);
    objActEntityId = data.objActEntityId;
    objActPartIndex = data.objActPartIndex;
    objActParamId = data.objActParamId;
    objActState = data.objActState;
    objActFlag = data.objActFlag;
    return true;
}

// Write subtype-specific data to the stream
bool ObjActEvent::SerializeSubtypeData(ofstream& stream) const
{
    const ObjActEventData data
    {
        .objActEntityId = objActEntityId,
        .objActPartIndex = objActPartIndex,
        .objActParamId = objActParamId,
        .objActState = objActState,
        .objActFlag = objActFlag,
    };
    WriteValidatedStruct(stream, data);
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
            throw MSBFormatError("NPCInvasionEventData: Expected minusOne to be -1, but got " + to_string(_minusOne));
    }
};

// Read subtype-specific data from the stream
bool NPCInvasionEvent::DeserializeSubtypeData(ifstream& stream)
{
    const auto data = ReadValidatedStruct<NPCInvasionEventData>(stream);
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
bool NPCInvasionEvent::SerializeSubtypeData(ofstream& stream) const
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

    WriteValidatedStruct(stream, data);
    return true;
}



struct PlatoonEventData
{
    int32_t platoonIdScriptActivate;
    int32_t state;
    padding<8> pad1;
    int32_t platoonPartsIndices[32];

    void Validate() const
    {
        AssertPadding("PlatoonEventData", pad1);
    }
};

// Read subtype-specific data from the stream
bool PlatoonEvent::DeserializeSubtypeData(ifstream& stream)
{
    auto data = ReadValidatedStruct<PlatoonEventData>(stream);
    platoonIdScriptActivate = data.platoonIdScriptActivate;
    state = data.state;
    ranges::copy(data.platoonPartsIndices, begin(platoonPartsIndices));
    return true;
}

// Write subtype-specific data to the stream
bool PlatoonEvent::SerializeSubtypeData(ofstream& stream) const
{
    PlatoonEventData data =
    {
        .platoonIdScriptActivate = platoonIdScriptActivate,
        .state = state,
    };
    ranges::copy(platoonPartsIndices, begin(data.platoonPartsIndices));
    WriteValidatedStruct(stream, data);
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
    array<int16_t, 64> patrolRegionsIndices;

    void Validate() const
    {
        if (_one != 1)
            throw MSBFormatError("PatrolRouteEventData: Expected 'one' to be 1, but got " + to_string(_one));
        if (_minusOne != -1)
            throw MSBFormatError("PatrolRouteEventData: Expected 'minusOne' to be -1, but got " + to_string(_minusOne));
        AssertPadding("PatrolRouteEventData", pad1, pad2);
    }
};

// Read subtype-specific data from the stream
bool PatrolRouteEvent::DeserializeSubtypeData(ifstream& stream)
{
    auto data = ReadValidatedStruct<PatrolRouteEventData>(stream);
    patrolType = data.patrolType;
    ranges::copy(data.patrolRegionsIndices, begin(patrolRegionsIndices));
    return true;
}

// Write subtype-specific data to the stream
bool PatrolRouteEvent::SerializeSubtypeData(ofstream& stream) const
{
    PatrolRouteEventData data =
    {
        .patrolType = patrolType,
        ._one = 1,
        ._minusOne = -1,
    };
    ranges::copy(patrolRegionsIndices, begin(data.patrolRegionsIndices));

    WriteValidatedStruct(stream, data);
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

bool MountEvent::DeserializeSubtypeData(ifstream& stream)
{
    const auto data = ReadValidatedStruct<MountEventData>(stream);
    riderPartIndex = data.riderPartIndex;
    mountPartIndex = data.mountPartIndex;
    return true;
}

bool MountEvent::SerializeSubtypeData(ofstream& stream) const
{
    const MountEventData data =
    {
        .riderPartIndex = riderPartIndex,
        .mountPartIndex = mountPartIndex,
    };
    WriteValidatedStruct(stream, data);
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

    void Validate() const
    {
        AssertPadding("SignPoolEventData", pad1);
    }
};

bool SignPoolEvent::DeserializeSubtypeData(ifstream& stream)
{
    const auto data = ReadValidatedStruct<SignPoolEventData>(stream);
    signPartIndex = data.signPartIndex;
    signPoolUnk04 = data.signPoolUnk04;
    return true;
}

bool SignPoolEvent::SerializeSubtypeData(ofstream& stream) const
{
    const SignPoolEventData data =
    {
        .signPartIndex = signPartIndex,
        .signPoolUnk04 = signPoolUnk04,
    };
    WriteValidatedStruct(stream, data);
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
            throw MSBFormatError("RetryPointEventData: Expected 'zero' to be 0, but got " + to_string(_zero));
    }
};

bool RetryPointEvent::DeserializeSubtypeData(ifstream& stream)
{
    const auto data = ReadValidatedStruct<RetryPointEventData>(stream);
    retryPartIndex = data.retryPartIndex;
    eventFlagId = data.eventFlagId;
    retryPointUnk08 = data.retryPointUnk08;
    retryRegionIndex = data.retryRegionIndex;
    return true;
}

bool RetryPointEvent::SerializeSubtypeData(ofstream& stream) const
{
    if (retryRegionIndex > 0xFFFF)
        throw MSBFormatError("RetryPointEvent: Retry region index is too large.");

    const RetryPointEventData data =
    {
        .retryPartIndex = retryPartIndex,
        .eventFlagId = eventFlagId,
        .retryPointUnk08 = retryPointUnk08,
        .retryRegionIndex = retryRegionIndex,
    };
    WriteValidatedStruct(stream, data);
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


bool AreaTeamEvent::DeserializeSubtypeData(ifstream& stream)
{
    const auto data = ReadValidatedStruct<AreaTeamEventData>(stream);
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


bool AreaTeamEvent::SerializeSubtypeData(ofstream& stream) const
{
    const AreaTeamEventData data =
    {
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
    WriteValidatedStruct(stream, data);
    return true;
}
