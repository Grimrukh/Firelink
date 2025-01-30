#include <typeindex>
#include <typeinfo>
#include <sstream>
#include <utility>
#include <variant>

#include "GrimHookER/Maps/MapStudio/Entry.h"
#include "GrimHookER/Maps/MapStudio/EntryParam.h"
#include "GrimHookER/Maps/MapStudio/Model.h"
#include "GrimHookER/Maps/MapStudio/Event.h"
#include "GrimHookER/Maps/MapStudio/Region.h"
#include "GrimHookER/Maps/MapStudio/Part.h"
#include "GrimHookER/Maps/MapStudio/Route.h"
#include "GrimHookER/Maps/MapStudio/Layer.h"

#include "GrimHookER/Maps/MapStudio/MSBFormatError.h"
#include "GrimHook/BinaryReadWrite.h"
#include "GrimHook/BinaryValidation.h"

using namespace std;
using namespace GrimHook::BinaryReadWrite;
using namespace GrimHook::BinaryValidation;
using namespace GrimHookER::Maps::MapStudio;


// Constructor
template <EntryType T, typename VariantType>
EntryParam<T, VariantType>::EntryParam(const int version, string name)
    : m_version(version), m_name(move(name)) {}


// Base read method: sets `version` and `name`, and returns list of entries for subtype sorting (in subclass).
template <EntryType T, typename VariantType>
vector<T*> EntryParam<T, VariantType>::Deserialize(ifstream& stream)
{
    m_version = ReadValue<int>(stream);
    const auto entryCount = ReadValue<int32_t>(stream);
    const auto listNameOffset = ReadValue<int64_t>(stream);

    // Final offset is for next `EntryParam` in MSB.
    vector<int64_t> entryOffsets;
    entryOffsets.reserve(entryCount - 1);
    for (int i = 0; i < entryCount - 1; ++i)
        entryOffsets.push_back(ReadValue<int64_t>(stream));
    const auto nextEntryParamOffset = ReadValue<int64_t>(stream);

    // Read and validate name.
    u16string nameWide = ReadUTF16String(stream, listNameOffset);
    if (const string readName(nameWide.begin(), nameWide.end()); readName != m_name)
        throw MSBFormatError("Expected MSB entry list name: \"" + m_name + "\", got: \"" + readName + "\"");

    // Read and collect pointers to all entries.
    vector<T*> entries;
    entries.reserve(entryCount - 1);  // allocate vector capacity now
    for (const int64_t offset : entryOffsets)
    {
        stream.seekg(offset);
        T* entry = DeserializeEntry(stream);
        if (!entry)
            throw MSBFormatError("Failed to read entry from MSB entry list (returned nullptr).");
        entries.push_back(entry);
    }

    // Final entry list must have this as 0.
    stream.seekg(nextEntryParamOffset);
    return entries;
}


// Write method
template <EntryType T, typename VariantType>
streampos EntryParam<T, VariantType>::Serialize(ofstream& stream, const vector<T*>& entries)
{
    WriteValue(stream, m_version);
    WriteValue(stream, static_cast<int>(entries.size() + 1));

    Reserver reserver(stream, true);

    reserver.ReserveOffset("entryParamNameOffset");
    for (size_t i = 0; i < entries.size(); ++i)
    {
        reserver.ReserveOffset("entryNameOffset" + to_string(i));
    }
    // Rather than reserving `nextEntryParamOffset`, we return it later.
    const streampos nextEntryParamOffset = stream.tellp();

    reserver.FillOffsetWithPosition("entryParamNameOffset");
    WriteUTF16String(stream, m_name);
    AlignStream(stream, 8);

    // Write Entries. We reset subtype index to zero whenever a new subtype is encountered.
    type_index currentType(typeid(void));
    int supertypeIndex = 0;  // never resets
    int subtypeIndex = 0;
    for (size_t i = 0; i < entries.size(); ++i)
    {
        if (currentType != typeid(*entries[i]))
        {
            // New subtype started. Reset subtype index.
            currentType = typeid(*entries[i]);
            subtypeIndex = 0;
        }

        reserver.FillOffsetWithPosition("entryNameOffset" + to_string(i));
        entries[i]->Serialize(stream, supertypeIndex, subtypeIndex);
        ++supertypeIndex;
        ++subtypeIndex;
    }

    reserver.Finish();

    // Caller (MSB) writes `nextEntryParamOffset` before writing next entry list.
    return nextEntryParamOffset;
}

template<EntryType T>
struct DeserializeVisitor
{
    std::ifstream& stream;
    T* rawPtr;

    template<typename S>
    void operator()(std::vector<std::unique_ptr<S>>& subtypeVector)
    {
        static_assert(std::is_base_of_v<T, S>, "Variant type must be a vector of unique pointers to subtypes of this Param.");  // sanity
        auto entry = std::make_unique<S>("");
        entry->Deserialize(stream);
        subtypeVector.push_back(std::move(entry));
        rawPtr = subtypeVector.back().get();
    }

    // Absorbs and ignores monostate variant contents (`LayerParam`).
    void operator()(std::monostate) const {}
};

template<EntryType T, typename VariantType>
T* EntryParam<T, VariantType>::DeserializeEntry(std::ifstream& stream)
{
    // TODO: Static assert to exclude `T = Layer`.

    auto entrySubtype = ReadEnum32<typename T::EnumType>(stream, stream.tellg() + static_cast<streamoff>(12));

    const auto it = m_subtypeEntries.find(entrySubtype);
    if (it == m_subtypeEntries.end())
        throw MSBFormatError("Unknown Entry type: " + to_string(static_cast<int>(entrySubtype)));

    DeserializeVisitor<T> visitor{stream, nullptr};
    std::visit(visitor, it->second);

    if (!visitor.rawPtr)
        throw MSBFormatError("Failed to deserialize Entry.");

    return visitor.rawPtr;
}

template<EntryType T>
struct GetAllVisitor
{
    vector<T*> allEntries;

    template<typename S>
    void operator()(const std::vector<std::unique_ptr<S>>& subtypeVector)
    {
        static_assert(std::is_base_of_v<T, S>, "Variant type must be a vector of unique pointers to subtypes of this Param.");  // sanity
        for (const auto& entryPtr : subtypeVector)
            allEntries.push_back(entryPtr.get());
    }

    // Absorbs and ignores monostate variant contents (`LayerParam`).
    void operator()(std::monostate) const {}
};

template<EntryType T, typename VariantType>
vector<T*> EntryParam<T, VariantType>::GetAllEntries()
{
    vector<T*> allEntries;
    allEntries.reserve(GetSize());
    GetAllVisitor<T> visitor{allEntries};

    for (const auto& [entrySubtype, subtypeVector] : m_subtypeEntries)
    {
        std::visit(visitor, subtypeVector);
    }

    return allEntries;
}

template<EntryType T>
struct GetSizeVisitor
{
    int sizeSum;

    template<typename S>
    void operator()(const std::vector<std::unique_ptr<S>>& subtypeVector)
    {
        static_assert(std::is_base_of_v<T, S>, "Variant type must be a vector of unique pointers to subtypes of this Param.");  // sanity
        sizeSum += subtypeVector.size();
    }

    // Absorbs and ignores monostate variant contents (`LayerParam`).
    void operator()(std::monostate) const {}
};

template<EntryType T, typename VariantType>
size_t EntryParam<T, VariantType>::GetSize() const
{
    GetSizeVisitor<T> visitor{0};
    for (const auto&[_, subtypeVector] : m_subtypeEntries)
        std::visit(visitor, subtypeVector);
    return visitor.sizeSum;
}

template<EntryType T>
struct ClearVisitor
{
    template<typename S>
    void operator()(std::vector<std::unique_ptr<S>>& subtypeVector) const
    {
        static_assert(std::is_base_of_v<T, S>, "Variant type must be a vector of unique pointers to subtypes of this Param.");  // sanity
        subtypeVector.clear();
    }

    // Absorbs and ignores monostate variant contents (`LayerParam`).
    void operator()(std::monostate) const {}
};

template<EntryType T, typename VariantType>
void EntryParam<T, VariantType>::ClearAllEntries()
{
    ClearVisitor<T> visitor;
    for (auto& [_, subtypeVector] : m_subtypeEntries)
        std::visit(visitor, subtypeVector);
}

template<EntryType T, typename VariantType>
EntryParam<T, VariantType>::operator string() const
{
    ostringstream oss;
    oss << "0x" << hex << uppercase << m_version << " " << m_name;
    return oss.str();
}


// Explicit specializations:
template class EntryParam<Model, ModelVariantType>;
template class EntryParam<Event, EventVariantType>;
template class EntryParam<Layer, LayerVariantType>;
template class EntryParam<Part, PartVariantType>;
template class EntryParam<Region, RegionVariantType>;
template class EntryParam<Route, RouteVariantType>;
