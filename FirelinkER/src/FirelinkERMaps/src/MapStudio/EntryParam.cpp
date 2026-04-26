#include "MSBBufferHelpers.h"
#include <FirelinkCore/BinaryReadWrite.h>
#include <FirelinkCore/BinaryValidation.h>
#include <FirelinkERMaps/Export.h>
#include <FirelinkERMaps/MapStudio/Entry.h>
#include <FirelinkERMaps/MapStudio/EntryParam.h>
#include <FirelinkERMaps/MapStudio/Event.h>
#include <FirelinkERMaps/MapStudio/Layer.h>
#include <FirelinkERMaps/MapStudio/Model.h>
#include <FirelinkERMaps/MapStudio/MSBFormatError.h>
#include <FirelinkERMaps/MapStudio/Part.h>
#include <FirelinkERMaps/MapStudio/Region.h>
#include <FirelinkERMaps/MapStudio/Route.h>

#include <format>
#include <ranges>
#include <sstream>
#include <utility>

using namespace Firelink;
using namespace Firelink::BinaryReadWrite;
using namespace Firelink::BinaryValidation;
using namespace Firelink::EldenRing::Maps::MapStudio;
using namespace Firelink::EldenRing::Maps::MapStudio::BufferHelpers;

template <typename T>
EntryParam<T>::EntryParam(const int version, std::u16string name)
: m_version(version)
, m_name(std::move(name))
, m_nameUtf8(UTF16ToUTF8(m_name))
{
    static_assert(std::is_base_of_v<Entry, T>, "EntryParam type must be an `Entry` subclass.");
    // Initialize empty vectors for all `T` subtypes.
    for (const auto& enumType : std::views::keys(T::GetTypeNames()))
    {
        m_entriesBySubtype.try_emplace(static_cast<int>(enumType));
        m_subtypeOrder.push_back(static_cast<int>(enumType));
    }
}

template <typename T>
void EntryParam<T>::AddEntry(std::unique_ptr<T> entry)
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

template <typename T>
void EntryParam<T>::RemoveEntry(T* entry)
{
    static_assert(std::is_base_of_v<Entry, T>, "EntryParam type must be an `Entry` subclass.");
    typename T::EnumType subtype = entry->GetType();
    int subtypeInt = static_cast<int>(subtype);
    if (!m_entriesBySubtype.contains(subtypeInt))
        throw MSBFormatError(
            format("Cannot remove Entry subtype {} from Param '{}'.", static_cast<int>(subtype), m_nameUtf8));
    auto& subtypeVector = m_entriesBySubtype.at(subtypeInt);
    auto it = subtypeVector.erase(
        std::remove_if(
            subtypeVector.begin(), subtypeVector.end(), [entry](const std::unique_ptr<T>& e) { return e.get() == entry; }),
        subtypeVector.end());

    if (it == subtypeVector.end())
        throw MSBFormatError("Failed to remove entry from MSB entry list (not present).");
}

// Base read method: sets `version` and `name`, and returns list of entries for subtype sorting (in subclass).
template <typename T>
std::vector<T*> EntryParam<T>::Deserialize(BufferReader& reader)
{
    static_assert(std::is_base_of_v<Entry, T>, "EntryParam type must be an `Entry` subclass.");

    auto pos = static_cast<int64_t>(reader.Position());
    Debug(std::format("Reading EntryParam '{}' at offset 0x{:X}.", m_nameUtf8, pos));

    m_version = reader.Read<int>();
    const auto entryOffsetCount = reader.Read<int32_t>();
    const auto listNameOffset = reader.Read<int64_t>();

    // Final offset is for next `EntryParam` in MSB.
    std::vector<int64_t> entryOffsets;
    entryOffsets.reserve(entryOffsetCount - 1);
    for (int i = 0; i < entryOffsetCount - 1; ++i)
        entryOffsets.push_back(reader.Read<int64_t>());
    const auto nextEntryParamOffset = reader.Read<int64_t>();

    // Read and validate name.
    {
        auto guard = reader.TempOffset(static_cast<size_t>(listNameOffset));
        if (const std::u16string name = BufferHelpers::ReadUTF16String(reader); name != m_name)
            throw MSBFormatError(
                std::format("Expected MSB entry list name '{}', but read '{}'.", m_nameUtf8, UTF16ToUTF8(name)));
    }

    // Read and collect pointers to all entries.
    std::vector<T*> entries;
    entries.reserve(entryOffsetCount - 1); // allocate vector capacity now
    for (const int64_t offset : entryOffsets)
    {
        reader.Seek(static_cast<size_t>(offset));
        T* entry = DeserializeEntry(reader);
        if (!entry)
            throw MSBFormatError("Failed to read entry from MSB entry list (returned nullptr).");
        Debug(format("Deserialized entry {} at offset 0x{:X} for Param '{}'.", std::string(*entry), offset, m_nameUtf8));
        entries.push_back(entry);
    }

    // Final entry list must have this as 0.
    reader.Seek(static_cast<size_t>(nextEntryParamOffset));
    return entries;
}

template <typename T>
std::string EntryParam<T>::Serialize(BufferWriter& writer) const
{
    static_assert(std::is_base_of_v<Entry, T>, "EntryParam type must be an `Entry` subclass.");
    const size_t entryCount = GetSize();

    Debug(std::format("Writing EntryParam '{}' at offset 0x{:X}.", m_nameUtf8, writer.Position()));

    writer.Write(m_version);
    writer.Write(static_cast<int>(entryCount + 1)); // +1 for `nextEntryParamOffset`
    writer.Reserve<int64_t>("entryParamNameOffset");
    // Reserve entry offsets.
    for (size_t i = 0; i < entryCount; ++i)
        writer.Reserve<int64_t>("entryNameOffset" + std::to_string(i));

    // Reserve `nextEntryParamOffset` with a label; return the label for the caller (MSB) to fill.
    const std::string nextOffsetLabel = m_nameUtf8 + "_nextParamOffset";
    writer.Reserve<int64_t>(nextOffsetLabel);

    writer.FillWithPosition<int64_t>("entryParamNameOffset");
    BufferHelpers::WriteUTF16String(writer, m_name);
    writer.PadAlign(8);

    // Write Entries in ascending subtype order.
    int supertypeIndex = 0; // never resets
    for (const auto& subtypeInt : m_subtypeOrder)
    {
        int subtypeIndex = 0;
        for (auto& subtypeVector = m_entriesBySubtype.at(subtypeInt); const auto& entry : subtypeVector)
        {
            writer.FillWithPosition<int64_t>("entryNameOffset" + std::to_string(supertypeIndex));
            entry->Serialize(writer, supertypeIndex, subtypeIndex);
            ++supertypeIndex;
            ++subtypeIndex;
        }
    }

    // Caller (MSB) fills `nextOffsetLabel` before writing next entry list.
    return nextOffsetLabel;
}

template <typename T>
T* EntryParam<T>::DeserializeEntry(BufferReader& reader)
{
    static_assert(std::is_base_of_v<Entry, T>, "EntryParam type must be an `Entry` subclass.");
    // Read subtype enum at the appropriate offset without advancing cursor
    auto entrySubtype = BufferHelpers::ReadEnum32At<typename T::EnumType>(
        reader, reader.Position() + T::SubtypeEnumOffset);

    int subtypeInt = static_cast<int>(entrySubtype);
    const auto it = m_entriesBySubtype.find(subtypeInt);
    if (it == m_entriesBySubtype.end())
        throw MSBFormatError(std::format("Unknown Entry type {} for Param '{}'.", subtypeInt, m_nameUtf8));
    auto& subtypeVector = it->second;
    auto entry = GetNewEntry(subtypeInt);
    entry->Deserialize(reader);
    return entry;
}

template <typename T>
std::vector<T*> EntryParam<T>::GetAllEntries() const
{
    static_assert(std::is_base_of_v<Entry, T>, "EntryParam type must be an `Entry` subclass.");
    std::vector<T*> allEntries;
    allEntries.reserve(GetSize());
    for (const auto& subtypeInt : m_subtypeOrder)
    {
        for (const auto& subtypeVector = m_entriesBySubtype.at(subtypeInt); const auto& entry : subtypeVector)
        {
            allEntries.push_back(entry.get());
        }
    }
    return allEntries;
}

template <typename T>
size_t EntryParam<T>::GetSize() const
{
    static_assert(std::is_base_of_v<Entry, T>, "EntryParam type must be an `Entry` subclass.");
    size_t size = 0;
    for (const auto& [_, subtypeVector] : m_entriesBySubtype) // doesn't have to be in subtype order
        size += subtypeVector.size();
    return size;
}

template <typename T>
void EntryParam<T>::ClearAllEntries()
{
    static_assert(std::is_base_of_v<Entry, T>, "EntryParam type must be an `Entry` subclass.");
    for (auto& [_, subtypeVector] : m_entriesBySubtype) // doesn't have to be in subtype order
        subtypeVector.clear();
}

template <typename T>
EntryParam<T>::operator std::string() const
{
    static_assert(std::is_base_of_v<Entry, T>, "EntryParam type must be an `Entry` subclass.");
    std::ostringstream oss;
    oss << "0x" << std::hex << std::uppercase << m_version << " " << m_nameUtf8;
    return oss.str();
}

// Explicit instantiations.
template class FIRELINK_ER_MAPS_API EntryParam<Model>;
template class FIRELINK_ER_MAPS_API EntryParam<Event>;
template class FIRELINK_ER_MAPS_API EntryParam<Layer>;
template class FIRELINK_ER_MAPS_API EntryParam<Part>;
template class FIRELINK_ER_MAPS_API EntryParam<Region>;
template class FIRELINK_ER_MAPS_API EntryParam<Route>;
