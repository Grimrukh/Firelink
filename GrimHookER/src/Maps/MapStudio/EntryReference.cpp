#include <format>

#include "GrimHook/Logging.h"
#include "GrimHookER/Maps/MapStudio/Entry.h"
#include "GrimHookER/Maps/MapStudio/Model.h"
#include "GrimHookER/Maps/MapStudio/Event.h"
#include "GrimHookER/Maps/MapStudio/Region.h"
#include "GrimHookER/Maps/MapStudio/Part.h"
#include "GrimHookER/Maps/MapStudio/EntryReference.h"

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
    if (index == -1)
        Clear();
    else
        Set(entries[index]);
}

template<typename T>
void EntryReference<T>::SetFromIndex16(const vector<T*>& entries, int16_t index)
{
    static_assert(is_base_of_v<Entry, T>, "EntryReference destination type must be an `Entry` subclass.");
    if (index == -1)
        Clear();
    else
        Set(entries[index]);
}


// Explicit template instantiations.
template class EntryReference<Part>;
template class EntryReference<Event>;
template class EntryReference<Model>;
template class EntryReference<Region>;
template class EntryReference<PatrolRouteEvent>;
template class EntryReference<CollisionPart>;
