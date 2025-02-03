#include <format>
#include <ranges>
#include <sstream>
#include <utility>

#include "FirelinkER/Maps/MapStudio/EntryParam.h"
#include "Firelink/BinaryReadWrite.h"
#include "Firelink/BinaryValidation.h"
#include "FirelinkER/Export.h"
#include "FirelinkER/Maps/MapStudio/Entry.h"
#include "FirelinkER/Maps/MapStudio/Event.h"
#include "FirelinkER/Maps/MapStudio/Layer.h"
#include "FirelinkER/Maps/MapStudio/Model.h"
#include "FirelinkER/Maps/MapStudio/MSBFormatError.h"
#include "FirelinkER/Maps/MapStudio/Part.h"
#include "FirelinkER/Maps/MapStudio/Region.h"
#include "FirelinkER/Maps/MapStudio/Route.h"

using namespace std;
using namespace Firelink;
using namespace Firelink::BinaryReadWrite;
using namespace Firelink::BinaryValidation;
using namespace FirelinkER::Maps::MapStudio;


template <typename T>
EntryParam<T>::EntryParam(const int version, u16string name)
    : m_version(version), m_name(move(name)), m_nameUtf8(UTF16ToUTF8(m_name))
{
    static_assert(std::is_base_of_v<Entry, T>, "EntryParam type must be an `Entry` subclass.");
    // Initialize empty vectors for all `T` subtypes.
    for (const auto& enumType : std::views::keys(T::GetTypeNames()))
    {
        m_entriesBySubtype.try_emplace(static_cast<int>(enumType));
        m_subtypeOrder.push_back(static_cast<int>(enumType));
    }
}

template<typename T>
void EntryParam<T>::AddEntry(unique_ptr<T> entry)
{
    static_assert(std::is_base_of_v<Entry, T>, "EntryParam type must be an `Entry` subclass.");
    typename T::EnumType subtype = entry->GetType();
    int subtypeInt = static_cast<int>(subtype);
    if (!m_entriesBySubtype.contains(subtypeInt))
        throw MSBFormatError(
            format("Cannot add Entry subtype {} to Param '{}'.", static_cast<int>(subtype), m_nameUtf8));
    auto& subtypeVector = m_entriesBySubtype.at(subtypeInt);
    subtypeVector.push_back(std::move(entry));
}

template<typename T>
void EntryParam<T>::RemoveEntry(T* entry)
{
    static_assert(std::is_base_of_v<Entry, T>, "EntryParam type must be an `Entry` subclass.");
    typename T::EnumType subtype = entry->GetType();
    int subtypeInt = static_cast<int>(subtype);
    if (!m_entriesBySubtype.contains(subtypeInt))
        throw MSBFormatError(
            format("Cannot remove Entry subtype {} from Param '{}'.", static_cast<int>(subtype), m_nameUtf8));
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
    static_assert(std::is_base_of_v<Entry, T>, "EntryParam type must be an `Entry` subclass.");

    auto pos = static_cast<int64_t>(stream.tellg());
    Info(format("Reading EntryParam '{}' at offset 0x{:X}.", m_nameUtf8, pos));

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
    if (const u16string name = ReadUTF16String(stream, listNameOffset); name != m_name)
        throw MSBFormatError(format(
            "Expected MSB entry list name '{}', but read '{}'.",
            m_nameUtf8, UTF16ToUTF8(name)));

    // Read and collect pointers to all entries.
    vector<T*> entries;
    entries.reserve(entryOffsetCount - 1);  // allocate vector capacity now
    for (const int64_t offset : entryOffsets)
    {
        stream.seekg(offset);
        T* entry = DeserializeEntry(stream);
        if (!entry)
            throw MSBFormatError("Failed to read entry from MSB entry list (returned nullptr).");
        Debug(
            format("Deserialized entry {} at offset 0x{:X} for Param '{}'.",
                string(*entry), offset, m_nameUtf8));
        // std::cout << "Deserialized entry " << string(*entry) << " at offset 0x" << std::hex << offset << " for Param '" << m_name << "'." << std::endl;
        entries.push_back(entry);
    }

    // Final entry list must have this as 0.
    stream.seekg(nextEntryParamOffset);
    return entries;
}


template <typename T>
streampos EntryParam<T>::Serialize(ofstream& stream)
{
    static_assert(std::is_base_of_v<Entry, T>, "EntryParam type must be an `Entry` subclass.");
    Reserver reserver(stream, true);
    const size_t entryCount = GetSize();

    Info(format("Writing EntryParam '{}' at offset 0x{:X}.", m_nameUtf8, static_cast<int64_t>(stream.tellp())));

    WriteValue(stream, m_version);
    WriteValue(stream, static_cast<int>(entryCount + 1));  // +1 for `nextEntryParamOffset`
    reserver.ReserveOffset("entryParamNameOffset");
    // Reserve entry offsets.
    for (size_t i = 0; i < entryCount; ++i)
        reserver.ReserveOffset("entryNameOffset" + to_string(i));

    // Rather than reserving `nextEntryParamOffset`, we return it for the caller to write.
    const streampos nextEntryParamOffset = stream.tellp();
    // Write dummy value for now.
    WriteValue(stream, static_cast<int64_t>(0));
    reserver.FillOffsetWithPosition("entryParamNameOffset");
    WriteUTF16String(stream, m_name);
    AlignStream(stream, 8);

    // Write Entries in ascending subtype order.
    int supertypeIndex = 0;  // never resets
    for (const auto& subtypeInt : m_subtypeOrder)
    {
        int subtypeIndex = 0;
        for (auto& subtypeVector = m_entriesBySubtype.at(subtypeInt); const auto& entry : subtypeVector)
        {
            reserver.FillOffsetWithPosition("entryNameOffset" + to_string(supertypeIndex));
            entry->Serialize(stream, supertypeIndex, subtypeIndex);
            ++supertypeIndex;
            ++subtypeIndex;
        }
    }

    reserver.Finish();

    // Caller (MSB) writes `nextEntryParamOffset` before writing next entry list.
    return nextEntryParamOffset;
}

template<typename T>
T* EntryParam<T>::DeserializeEntry(ifstream& stream)
{
    static_assert(std::is_base_of_v<Entry, T>, "EntryParam type must be an `Entry` subclass.");
    auto entrySubtype = ReadEnum32<typename T::EnumType>(
        stream, stream.tellg() + static_cast<streamoff>(T::SubtypeEnumOffset));

    int subtypeInt = static_cast<int>(entrySubtype);
    const auto it = m_entriesBySubtype.find(subtypeInt);
    if (it == m_entriesBySubtype.end())
        throw MSBFormatError(
            std::format("Unknown Entry type {} for Param '{}'.", subtypeInt, m_nameUtf8));
    auto& subtypeVector = it->second;
    auto entry = GetNewEntry(subtypeInt);
    entry->Deserialize(stream);
    return entry;
}

template<typename T>
vector<T*> EntryParam<T>::GetAllEntries() const
{
    static_assert(std::is_base_of_v<Entry, T>, "EntryParam type must be an `Entry` subclass.");
    vector<T*> allEntries;
    allEntries.reserve(GetSize());
    for (const auto& subtypeInt : m_subtypeOrder)
    {
        for (const auto& subtypeVector = m_entriesBySubtype.at(subtypeInt);
            const auto& entry : subtypeVector)
        {
            allEntries.push_back(entry.get());
        }
    }
    return allEntries;
}

template<typename T>
size_t EntryParam<T>::GetSize() const
{
    static_assert(std::is_base_of_v<Entry, T>, "EntryParam type must be an `Entry` subclass.");
    size_t size = 0;
    for (const auto&[_, subtypeVector] : m_entriesBySubtype)  // doesn't have to be in subtype order
        size += subtypeVector.size();
    return size;
}

template<typename T>
void EntryParam<T>::ClearAllEntries()
{
    static_assert(std::is_base_of_v<Entry, T>, "EntryParam type must be an `Entry` subclass.");
    for (auto& [_, subtypeVector] : m_entriesBySubtype)  // doesn't have to be in subtype order
        subtypeVector.clear();
}

template<typename T>
EntryParam<T>::operator string() const
{
    static_assert(std::is_base_of_v<Entry, T>, "EntryParam type must be an `Entry` subclass.");
    ostringstream oss;
    oss << "0x" << hex << uppercase << m_version << " " << m_nameUtf8;
    return oss.str();
}


// Explicit instantiations.
template class FIRELINK_ER_API EntryParam<Model>;
template class FIRELINK_ER_API EntryParam<Event>;
template class FIRELINK_ER_API EntryParam<Layer>;
template class FIRELINK_ER_API EntryParam<Part>;
template class FIRELINK_ER_API EntryParam<Region>;
template class FIRELINK_ER_API EntryParam<Route>;
