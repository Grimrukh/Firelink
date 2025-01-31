#include <format>
#include <ranges>
#include <sstream>
#include <typeindex>
#include <typeinfo>
#include <utility>

#include "GrimHookER/Maps/MapStudio/EntryParam.h"
#include "GrimHook/BinaryReadWrite.h"
#include "GrimHook/BinaryValidation.h"
#include "GrimHook/MemoryUtils.h"
#include "GrimHookER/Export.h"
#include "GrimHookER/Maps/MapStudio/Entry.h"
#include "GrimHookER/Maps/MapStudio/Event.h"
#include "GrimHookER/Maps/MapStudio/Layer.h"
#include "GrimHookER/Maps/MapStudio/Model.h"
#include "GrimHookER/Maps/MapStudio/MSBFormatError.h"
#include "GrimHookER/Maps/MapStudio/Part.h"
#include "GrimHookER/Maps/MapStudio/Region.h"
#include "GrimHookER/Maps/MapStudio/Route.h"

using namespace std;
using namespace GrimHook::BinaryReadWrite;
using namespace GrimHook::BinaryValidation;
using namespace GrimHookER::Maps::MapStudio;


template <typename T>
EntryParam<T>::EntryParam(const int version, string name) : m_version(version), m_name(move(name))
{
    static_assert(std::is_base_of_v<Entry, T>, "EntryParam type must be an `Entry` subclass.");
    // Initialize empty vectors for all `T` subtypes.
    for (const auto& enumType : std::views::keys(T::GetTypeNames()))
        m_entriesBySubtype.try_emplace(static_cast<int>(enumType));
}

template<typename T>
void EntryParam<T>::AddEntry(unique_ptr<T> entry)
{
    typename T::EnumType subtype = entry->GetType();
    int subtypeInt = static_cast<int>(subtype);
    if (!m_entriesBySubtype.contains(subtypeInt))
        throw MSBFormatError(
            format("Cannot add Entry subtype {} to Param '{}'.", static_cast<int>(subtype), m_name));
    auto& subtypeVector = m_entriesBySubtype.at(subtypeInt);
    subtypeVector.push_back(std::move(entry));
}

template<typename T>
void EntryParam<T>::RemoveEntry(T* entry)
{
    typename T::EnumType subtype = entry->GetType();
    int subtypeInt = static_cast<int>(subtype);
    if (!m_entriesBySubtype.contains(subtypeInt))
        throw MSBFormatError(
            format("Cannot remove Entry subtype {} from Param '{}'.", static_cast<int>(subtype), m_name));
    auto& subtypeVector = m_entriesBySubtype.at(subtypeInt);
    auto it = subtypeVector.erase(std::remove_if(
        subtypeVector.begin(), subtypeVector.end(),
             [entry](const unique_ptr<T>& e) { return e.get() == entry; }),
        subtypeVector.end());

    if (it == subtypeVector.end())
        throw MSBFormatError("Failed to remove entry from MSB entry list (not present).");
}

// Base read method: sets `version` and `name`, and returns list of entries for subtype sorting (in subclass).
template <typename T>
vector<T*> EntryParam<T>::Deserialize(ifstream& stream)
{
    m_version = ReadValue<int>(stream);
    const auto entryOffsetCount = ReadValue<int32_t>(stream);
    const auto listNameOffset = ReadValue<int64_t>(stream);

    // Final offset is for next `EntryParam` in MSB.
    vector<int64_t> entryOffsets;
    entryOffsets.reserve(entryOffsetCount - 1);
    for (int i = 0; i < entryOffsetCount - 1; ++i)
        entryOffsets.push_back(ReadValue<int64_t>(stream));
    const auto nextEntryParamOffset = ReadValue<int64_t>(stream);

    // Read and validate name.
    const u16string nameWide = ReadUTF16String(stream, listNameOffset);
    if (const string readName = GrimHook::UTF16ToUTF8(nameWide); readName != m_name)
        throw MSBFormatError("Expected MSB entry list name: \"" + m_name + "\", got: \"" + readName + "\"");

    // Read and collect pointers to all entries.
    vector<T*> entries;
    entries.reserve(entryOffsetCount - 1);  // allocate vector capacity now
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
template <typename T>
streampos EntryParam<T>::Serialize(ofstream& stream, const vector<T*>& entries)
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

template<typename T>
T* EntryParam<T>::DeserializeEntry(ifstream& stream)
{
    auto entrySubtype = ReadEnum32<typename T::EnumType>(
        stream, stream.tellg() + static_cast<streamoff>(T::SubtypeEnumOffset));

    int subtypeInt = static_cast<int>(entrySubtype);
    const auto it = m_entriesBySubtype.find(subtypeInt);
    if (it == m_entriesBySubtype.end())
        throw MSBFormatError(
            std::format("Unknown Entry type {} for Param '{}'.", subtypeInt, m_name));
    auto& subtypeVector = it->second;
    auto entry = GetNewEntry(subtypeInt);
    entry->Deserialize(stream);
    return entry;
}

template<typename T>
vector<T*> EntryParam<T>::GetAllEntries() const
{
    vector<T*> allEntries;
    allEntries.reserve(GetSize());
    for (const auto& [entrySubtype, subtypeVector] : m_entriesBySubtype)
    {
        for (const auto& entry : subtypeVector)
            allEntries.push_back(entry.get());
    }
    return allEntries;
}

template<typename T>
size_t EntryParam<T>::GetSize() const
{
    size_t size = 0;
    for (const auto&[_, subtypeVector] : m_entriesBySubtype)
        size += subtypeVector.size();
    return size;
}

template<typename T>
void EntryParam<T>::ClearAllEntries()
{
    for (auto& [_, subtypeVector] : m_entriesBySubtype)
        subtypeVector.clear();
}

template<typename T>
EntryParam<T>::operator string() const
{
    ostringstream oss;
    oss << "0x" << hex << uppercase << m_version << " " << m_name;
    return oss.str();
}


// Explicit instantiations.
template class GRIMHOOKER_API EntryParam<Model>;
template class GRIMHOOKER_API EntryParam<Event>;
template class GRIMHOOKER_API EntryParam<Layer>;
template class GRIMHOOKER_API EntryParam<Part>;
template class GRIMHOOKER_API EntryParam<Region>;
template class GRIMHOOKER_API EntryParam<Route>;
