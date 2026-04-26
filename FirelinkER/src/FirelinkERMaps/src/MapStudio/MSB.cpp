#include <FirelinkERMaps/MapStudio/MSB.h>

#include <FirelinkERMaps/MapStudio/EntryParam.h>
#include <FirelinkERMaps/MapStudio/EventParam.h>
#include <FirelinkERMaps/MapStudio/LayerParam.h>
#include <FirelinkERMaps/MapStudio/ModelParam.h>
#include <FirelinkERMaps/MapStudio/MSBFormatError.h>
#include <FirelinkERMaps/MapStudio/PartParam.h>
#include <FirelinkERMaps/MapStudio/RegionParam.h>
#include <FirelinkERMaps/MapStudio/RouteParam.h>

#include <FirelinkCore/BinaryReadWrite.h>

#include <cstdint>
#include <string>
#include <vector>

using namespace Firelink::BinaryReadWrite;
using namespace Firelink::EldenRing::Maps::MapStudio;

struct MSBHeader
{
    char signature[4] = {'M', 'S', 'B', ' '};
    int version = 1;
    int headerSize = 16;
    bool bigEndian = false;
    bool bitBigEndian = false;
    uint8_t unicode = 1;
    uint8_t reserved = 255;
};

size_t MSB::GetEntryCount() const
{
    size_t count = 0;
    count += m_modelParam.GetSize();
    count += m_eventParam.GetSize();
    count += m_regionParam.GetSize();
    count += m_partParam.GetSize();
    count += m_routeParam.GetSize();
    return count;
}

void MSB::ReadHeader(BufferReader& reader)
{
    const auto header = reader.Read<MSBHeader>();
    if (strncmp(header.signature, "MSB ", 4) != 0)
        throw MSBFormatError("Invalid MSB signature.");
    if (header.version != 1)
        throw MSBFormatError("Unsupported MSB version: " + std::to_string(header.version));
    if (header.headerSize != 16)
        throw MSBFormatError("Invalid MSB header size: " + std::to_string(header.headerSize));
    if (header.unicode != 1)
        throw MSBFormatError("MSB must be encoded in UTF-16.");
    if (header.reserved != 255)
        throw MSBFormatError("Invalid MSB reserved value: " + std::to_string(header.reserved));
    if (header.bigEndian)
        throw MSBFormatError("MSB must be little-endian.");
    if (header.bitBigEndian)
        throw MSBFormatError("MSB must be little-endian.");
}

void MSB::WriteHeader(BufferWriter& writer)
{
    // No variation in header.
    static constexpr MSBHeader header;
    writer.Write(header);
}

void MSB::Deserialize(BufferReader& reader)
{
    // Any existing entries are discarded.
    m_modelParam.ClearAllEntries();
    m_eventParam.ClearAllEntries();
    m_regionParam.ClearAllEntries();
    m_routeParam.ClearAllEntries();
    m_partParam.ClearAllEntries();

    ReadHeader(reader);

    const std::vector<Model*> models = m_modelParam.Deserialize(reader);
    const std::vector<Event*> events = m_eventParam.Deserialize(reader);
    const std::vector<Region*> regions = m_regionParam.Deserialize(reader);
    m_routeParam.Deserialize(reader); // no need to collect Routes (no references to/from)
    LayerParam().Deserialize(reader); // read header of empty LayerParam
    const std::vector<Part*> parts = m_partParam.Deserialize(reader);

    if (reader.Position() != 0)
        throw MSBFormatError("Next-list offset of final list (Parts) was not 0.");

    // TODO: Make all entry names unique with temporary suffixes that can be automatically removed on `Write()`.
    //  I prefer Blender-style suffixes like '.001', '.002', ...

    DeserializeEntryReferences(models, events, regions, parts);
}

// NOTE: Not a `const` method because it updates entry indices from current pointers (and model instance counts, etc.).
void MSB::Serialize(BufferWriter& writer) const
{
    const std::vector<Model*> models = m_modelParam.GetAllEntries();
    const std::vector<Event*> events = m_eventParam.GetAllEntries();
    const std::vector<Region*> regions = m_regionParam.GetAllEntries();
    const std::vector<Route*> routes = m_routeParam.GetAllEntries();
    // No Layers.
    const std::vector<Part*> parts = m_partParam.GetAllEntries();

    SerializeEntryIndices(models, events, regions, parts);

    // Count and set model part instance counts.
    std::map<Model*, int> modelInstanceCounts{};
    for (const Part* part : parts)
    {
        if (Model* model = part->GetModel())
            modelInstanceCounts[model]++;
    }
    for (Model* model : models)
        model->SetInstanceCount(modelInstanceCounts[model]);

    WriteHeader(writer);

    // All Entry data is up to date (indices, instance counts). Supertype and subtype indices will be passed in by the
    // EntryParam write calls. We just have to fill the final offsets ourselves.
    std::string nextLabel = m_modelParam.Serialize(writer);
    writer.FillWithPosition<int64_t>(nextLabel);

    nextLabel = m_eventParam.Serialize(writer);
    writer.FillWithPosition<int64_t>(nextLabel);

    nextLabel = m_regionParam.Serialize(writer);
    writer.FillWithPosition<int64_t>(nextLabel);

    nextLabel = m_routeParam.Serialize(writer);
    writer.FillWithPosition<int64_t>(nextLabel);

    nextLabel = LayerParam().Serialize(writer); // no Layers
    writer.FillWithPosition<int64_t>(nextLabel);

    nextLabel = m_partParam.Serialize(writer);
    writer.Fill<int64_t>(nextLabel, static_cast<int64_t>(0)); // last offset is 0

    // Nothing extra at the end of the MSB file.
}

void MSB::DeserializeEntryReferences(
    const std::vector<Model*>& models,
    const std::vector<Event*>& events,
    const std::vector<Region*>& regions,
    const std::vector<Part*>& parts)
{
    // Get CollisionParts and PatrolRouteEvents std::vectors for subtype-specific indexing by certain Parts.
    std::vector<CollisionPart*> collisionParts; // unknown count
    for (Part* part : parts)
    {
        if (auto collisionPart = dynamic_cast<CollisionPart*>(part))
            collisionParts.push_back(collisionPart);
    }
    std::vector<PatrolRouteEvent*> patrolRouteEvents; // unknown count
    for (Event* event : events)
    {
        if (auto patrolRouteEvent = dynamic_cast<PatrolRouteEvent*>(event))
            patrolRouteEvents.push_back(patrolRouteEvent);
    }

    for (Event* event : events)
        event->DeserializeEntryReferences(parts, regions);

    for (Region* region : regions)
        region->DeserializeEntryReferences(regions, parts);

    for (Part* part : parts)
    {
        part->DeserializeEntryReferences(models, parts, regions);
        if (const auto connectCollisionPart = dynamic_cast<ConnectCollisionPart*>(part))
            connectCollisionPart->DeserializeCollisionReference(collisionParts);
        if (const auto characterPart = dynamic_cast<CharacterPart*>(part))
            characterPart->DeserializePatrolRouteEventReference(patrolRouteEvents);
    }
}

void MSB::SerializeEntryIndices(
    const std::vector<Model*>& models,
    const std::vector<Event*>& events,
    const std::vector<Region*>& regions,
    const std::vector<Part*>& parts)
{
    // Get CollisionParts and PatrolRouteEvents std::vectors for subtype-specific indexing by certain Parts.
    std::vector<CollisionPart*> collisionParts{};
    for (Part* part : parts)
    {
        if (auto collisionPart = dynamic_cast<CollisionPart*>(part))
            collisionParts.push_back(collisionPart);
    }
    std::vector<PatrolRouteEvent*> patrolRouteEvents{};
    for (Event* event : events)
    {
        if (auto patrolRouteEvent = dynamic_cast<PatrolRouteEvent*>(event))
            patrolRouteEvents.push_back(patrolRouteEvent);
    }

    // Models contain no references.

    for (Event* event : events)
        event->SerializeEntryIndices(parts, regions);

    for (Region* region : regions)
        region->SerializeEntryIndices(regions, parts);

    for (Part* part : parts)
    {
        part->SerializeEntryIndices(models, parts, regions);
        if (const auto connectCollisionPart = dynamic_cast<ConnectCollisionPart*>(part))
            connectCollisionPart->SerializeCollisionIndex(collisionParts);
        if (const auto characterPart = dynamic_cast<CharacterPart*>(part))
            characterPart->SerializePatrolRouteEventIndex(patrolRouteEvents);
    }
}
