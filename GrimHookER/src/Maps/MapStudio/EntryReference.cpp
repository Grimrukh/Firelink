#include <format>

#include "GrimHook/Logging.h"
#include "GrimHookER/Maps/MapStudio/Entry.h"
#include "GrimHookER/Maps/MapStudio/EntryReference.h"
#include "GrimHookER/Maps/MapStudio/Model.h"
#include "GrimHookER/Maps/MapStudio/Part.h"
#include "GrimHookER/Maps/MapStudio/Event.h"
#include "GrimHookER/Maps/MapStudio/Region.h"

using namespace std;
using namespace GrimHook;
using namespace GrimHookER::Maps::MapStudio;


template<typename T>
EntryReference<T>::~EntryReference()
{
    static_assert(is_base_of_v<Entry, T>, "EntryReference destination type must be an `Entry` subclass.");
    if (m_destEntry)
    {
        // NOTE: We can't call virtual `Clear()` method here, but this is the same.
        m_destEntry->RemoveReferrer(this);
        m_destEntry = nullptr;
    }
}

template<typename T>
EntryReference<T>& EntryReference<T>::operator=(const EntryReference& otherReference)
{
    static_assert(is_base_of_v<Entry, T>, "EntryReference destination type must be an `Entry` subclass.");
    if (this == &otherReference)
        return *this;
    // Update destination entry.
    m_destEntry = otherReference.m_destEntry;
    return *this;
}

template<typename T>
void EntryReference<T>::Set(T* destEntry)
{
    static_assert(is_base_of_v<Entry, T>, "EntryReference destination type must be an `Entry` subclass.");
    Clear();
    m_destEntry = destEntry;
    if (m_destEntry)
        m_destEntry->AddReferrer(this);
}

template<typename T>
void EntryReference<T>::Set(const unique_ptr<T>& destEntry)
{
    static_assert(is_base_of_v<Entry, T>, "EntryReference destination type must be an `Entry` subclass.");
    Set(destEntry.get());
}

template<typename T>
void EntryReference<T>::Clear()
{
    static_assert(is_base_of_v<Entry, T>, "EntryReference destination type must be an `Entry` subclass.");
    if (m_destEntry)
    {
        m_destEntry->RemoveReferrer(this);
        m_destEntry = nullptr;
    }
}

template<typename T>
EntryReference<T> EntryReference<T>::Clone() const
{
    static_assert(is_base_of_v<Entry, T>, "EntryReference destination type must be an `Entry` subclass.");
    return EntryReference(m_destEntry);
}

template<typename T>
int32_t EntryReference<T>::ToIndex(const Entry* sourceEntry, const vector<T*>& entries) const
{
    static_assert(is_base_of_v<Entry, T>, "EntryReference destination type must be an `Entry` subclass.");
    if (!m_destEntry)
        return -1;  // null reference

    // Cast is required for template iteration (rather than overriding vector Compare template).
    // auto it = std::find(entries.begin(), entries.end(), m_destEntry);
    // if (it != entries.end())
    //     return static_cast<int>(distance(entries.begin(), it));

    int i = 0;
    for (auto& entry : entries)
    {
        if (entry == m_destEntry)
            return i;
        i++;
    }

    string sourceName = sourceEntry->GetName();
    string destName = m_destEntry->GetName();
    Error(format("EntryReference '{}' references an invalid entry: '{}'", sourceName, destName));
    return -1;
}

template<typename T>
int16_t EntryReference<T>::ToIndex16(const Entry* sourceEntry, const vector<T*>& entries) const
{
    static_assert(is_base_of_v<Entry, T>, "EntryReference destination type must be an `Entry` subclass.");
    if (!m_destEntry)
        return -1;  // null reference

    int i = 0;
    for (auto& entry : entries)
    {
        if (entry == m_destEntry)
            try
            {
                return static_cast<int16_t>(i);
            } catch (const std::out_of_range& _)
            {
                string sourceName = sourceEntry->GetName();
                string destName = m_destEntry->GetName();
                Error(format("EntryReference '{}' references entry '{}' with an index that is too large "
                             "(index field is 16-bit).", sourceName, destName));
                return -1;
            }
        i++;
    }

    // auto it = std::find(entries.begin(), entries.end(), m_destEntry);
    // if (it != entries.end())
    // {
    //     // Try to cast the distance to int16_t. Catch an out-of-bounds error.
    //     try
    //     {
    //         return static_cast<int16_t>(distance(entries.begin(), it));
    //     }
    //     catch (const std::out_of_range& _)
    //     {
    //         string sourceName = m_destEntry->GetName();
    //         string destName = m_destEntry->GetName();
    //         Error(format("EntryReference '{}' references entry '{}' with an index that is too large "
    //                      "(index field is 16-bit).", sourceName, destName));
    //         return -1;
    //     }
    // }

    string sourceName = sourceEntry->GetName();
    string destName = m_destEntry->GetName();
    Error(format("EntryReference '{}' references an invalid entry: '{}'", sourceName, destName));
    return -1;
}

template<typename T>
EntryReference<T>& EntryReference<T>::operator=(T* destEntry)
{
    static_assert(is_base_of_v<Entry, T>, "EntryReference destination type must be an `Entry` subclass.");
    Set(destEntry);
    return *this;
}

template<typename T>
void EntryReference<T>::OnReferencedEntryDestroy(const T* entry)
{
    static_assert(is_base_of_v<Entry, T>, "EntryReference destination type must be an `Entry` subclass.");
    if (m_destEntry == entry)
        // We do NOT unregister (destination entry is being destroyed).
        m_destEntry = nullptr;
}

template<typename T>
void EntryReference<T>::SetFromIndex(const vector<T*>& entries, int32_t index)
{
    static_assert(is_base_of_v<Entry, T>, "EntryReference destination type must be an `Entry` subclass.");
    Set(entries[index]);
}

template<typename T>
void EntryReference<T>::SetFromIndex16(const vector<T*>& entries, int16_t index)
{
    static_assert(is_base_of_v<Entry, T>, "EntryReference destination type must be an `Entry` subclass.");
    Set(entries[index]);
}


// Explicit template specifications:
template class EntryReference<Part>;
template class EntryReference<Event>;
template class EntryReference<Model>;
template class EntryReference<Region>;
template class EntryReference<PatrolRouteEvent>;
template class EntryReference<CollisionPart>;

// // Basic templates instantiated for all MSB Entry types:
// ENTRY_REF_32(Model);
// ENTRY_REF_32(Event);
// ENTRY_REF_32(Region);
// ENTRY_REF_32(Part);
// Bespoke templates as needed across all MSB Entry fields:
// // ENTRY_REF_32(CollisionPart);  // ConnectCollision.collision
// ENTRY_REF_16(PatrolRouteEvent);  // CharacterPart.patrolRouteEvent
// ENTRY_REF_16(Region);  // RetryPointEvent.retryRegion, PatrolRouteEvent.patrolRegions
// ENTRY_REF_ARRAY_16(Region, 64);  // PatrolRouteEvent.patrolRegions
// ENTRY_REF_ARRAY_32(Part, 32);  // SpawnerEvent.spawnParts, PlatoonEvent.platoonParts
// ENTRY_REF_ARRAY_32(Part, 8);  // GroupDefeatRewardRegion. groupParts
// ENTRY_REF_ARRAY_32(Part, 6);  // AssetPart.drawParentParts
// ENTRY_REF_ARRAY_32(Region, 8)  // SpawnerEvent.spawnRegions
// ENTRY_REF_ARRAY_32(Region, 16)  // SoundRegion.childRegions
