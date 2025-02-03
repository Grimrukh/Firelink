#include <cstdint>
#include <cstring>
#include <filesystem>
#include <ranges>
#include <string>
#include <vector>

#include "FirelinkER/Maps/MapStudio/MSB.h"
#include "Firelink/BinaryReadWrite.h"
#include "FirelinkER/Maps/MapStudio/EntryParam.h"
#include "FirelinkER/Maps/MapStudio/EventParam.h"
#include "FirelinkER/Maps/MapStudio/LayerParam.h"
#include "FirelinkER/Maps/MapStudio/ModelParam.h"
#include "FirelinkER/Maps/MapStudio/MSBFormatError.h"
#include "FirelinkER/Maps/MapStudio/PartParam.h"
#include "FirelinkER/Maps/MapStudio/RegionParam.h"
#include "FirelinkER/Maps/MapStudio/RouteParam.h"

using namespace std;
using namespace Firelink::BinaryReadWrite;
using namespace FirelinkER::Maps::MapStudio;


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


void MSB::ReadHeader(ifstream& stream)
{
    MSBHeader header;
    stream.read(reinterpret_cast<char*>(&header), sizeof(MSBHeader));
    if (strncmp(header.signature, "MSB ", 4) != 0)
        throw MSBFormatError("Invalid MSB signature.");
    if (header.version != 1)
        throw MSBFormatError("Unsupported MSB version: " + to_string(header.version));
    if (header.headerSize != 16)
        throw MSBFormatError("Invalid MSB header size: " + to_string(header.headerSize));
    if (header.unicode != 1)
        throw MSBFormatError("MSB must be encoded in UTF-16.");
    if (header.reserved != 255)
        throw MSBFormatError("Invalid MSB reserved value: " + to_string(header.reserved));
    if (header.bigEndian)
        throw MSBFormatError("MSB must be little-endian.");
    if (header.bitBigEndian)
        throw MSBFormatError("MSB must be little-endian.");
    if (header.unicode != 1)
        throw MSBFormatError("MSB must be encoded in UTF-16.");
    if (header.reserved != 255)
        throw MSBFormatError("Invalid MSB reserved value: " + to_string(header.reserved));
}

void MSB::WriteHeader(ofstream& stream)
{
    // No variation in header.
    static constexpr MSBHeader header;
    stream.write(reinterpret_cast<const char*>(&header), sizeof(MSBHeader));
}


void MSB::Deserialize(ifstream& stream)
{
    // Any existing entries are discarded.
    m_modelParam.ClearAllEntries();
    m_eventParam.ClearAllEntries();
    m_regionParam.ClearAllEntries();
    m_routeParam.ClearAllEntries();
    m_partParam.ClearAllEntries();

    ReadHeader(stream);

    const vector<Model*> models = m_modelParam.Deserialize(stream);
    const vector<Event*> events = m_eventParam.Deserialize(stream);
    const vector<Region*> regions = m_regionParam.Deserialize(stream);
    m_routeParam.Deserialize(stream);  // no need to collect Routes (no references to/from)
    LayerParam().Deserialize(stream);  // read header of empty LayerParam
    const vector<Part*> parts = m_partParam.Deserialize(stream);

    if (stream.tellg() != 0)
        throw MSBFormatError("Next-list offset of final list (Parts) was not 0.");

    // TODO: Make all entry names unique with temporary suffixes that can be automatically removed on `Write()`.
    //  I prefer Blender-style suffixes like '.001', '.002', ...

    DeserializeEntryReferences(models, events, regions, parts);
}


// NOTE: Not a `const` method  because it updates entry indices from current pointers (and model instance counts, etc.).
void MSB::Serialize(ofstream& stream)
{
    const vector<Model*> models = m_modelParam.GetAllEntries();
    const vector<Event*> events = m_eventParam.GetAllEntries();
    const vector<Region*> regions = m_regionParam.GetAllEntries();
    const vector<Route*> routes = m_routeParam.GetAllEntries();
    // No Layers.
    const vector<Part*> parts = m_partParam.GetAllEntries();

    SerializeEntryIndices(models, events, regions, parts);

    // Count and set model part instance counts.
    map<Model*, int> modelInstanceCounts{};
    for (const Part* part : parts)
    {
        if (Model* model = part->GetModel())
            modelInstanceCounts[model]++;
    }
    for (Model* model : models)
        model->SetInstanceCount(modelInstanceCounts[model]);

    WriteHeader(stream);

    // All Entry data is up to date (indices, instance counts). Supertype and subtype indices will be passed in by the
    // EntryParam write calls. We just have to write the final offsets ourselves.
    streampos nextEntryParamOffset = m_modelParam.Serialize(stream);
    WriteValue(stream, nextEntryParamOffset, static_cast<int64_t>(stream.tellp()));

    nextEntryParamOffset = m_eventParam.Serialize(stream);
    WriteValue(stream, nextEntryParamOffset, static_cast<int64_t>(stream.tellp()));

    nextEntryParamOffset = m_regionParam.Serialize(stream);
    WriteValue(stream, nextEntryParamOffset, static_cast<int64_t>(stream.tellp()));

    nextEntryParamOffset = m_routeParam.Serialize(stream);
    WriteValue(stream, nextEntryParamOffset, static_cast<int64_t>(stream.tellp()));

    nextEntryParamOffset = LayerParam().Serialize(stream);  // no Layers
    WriteValue(stream, nextEntryParamOffset, static_cast<int64_t>(stream.tellp()));

    nextEntryParamOffset = m_partParam.Serialize(stream);
    WriteValue(stream, nextEntryParamOffset, static_cast<int64_t>(0));  // last offset is 0

    // Nothing extra at the end of the MSB file.
}

unique_ptr<MSB> MSB::NewFromFilePath(const filesystem::path& path)
{
    // Load the MSB file.
    ifstream stream(path, ios::binary);
    if (!stream)
        throw MSBFormatError("Could not open file: " + path.string());
    auto msb = make_unique<MSB>();
    msb->Deserialize(stream);
    return msb;
}

void MSB::WriteToFilePath(const filesystem::path& path)
{
    // Open the MSB file for writing.
    ofstream stream(path, ios::binary);
    if (!stream)
        throw MSBFormatError("Could not open file for writing: " + path.string());
    try
    {
        Serialize(stream);
    }
    catch (...)
    {
        stream.close();
        throw;
    }
    stream.close();
}


void MSB::DeserializeEntryReferences(
    const vector<Model*>& models,
    const vector<Event*>& events,
    const vector<Region*>& regions,
    const vector<Part*>& parts)
{

    // Get CollisionParts and PatrolRouteEvents vectors for subtype-specific indexing by certain Parts.
    vector<CollisionPart*> collisionParts;  // unknown count
    for (Part* part : parts)
    {
        if (auto collisionPart = dynamic_cast<CollisionPart*>(part))
            collisionParts.push_back(collisionPart);
    }
    vector<PatrolRouteEvent*> patrolRouteEvents;  // unknown count
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
    const vector<Model*>& models,
    const vector<Event*>& events,
    const vector<Region*>& regions,
    const vector<Part*>& parts)
{
    // Get CollisionParts and PatrolRouteEvents vectors for subtype-specific indexing by certain Parts.
    vector<CollisionPart*> collisionParts{};
    for (Part* part : parts)
    {
        if (auto collisionPart = dynamic_cast<CollisionPart*>(part))
            collisionParts.push_back(collisionPart);
    }
    vector<PatrolRouteEvent*> patrolRouteEvents{};
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
