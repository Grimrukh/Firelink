#pragma once

#include <array>
#include <memory>
#include <vector>

#include "FirelinkER/Export.h"


namespace FirelinkER::Maps::MapStudio
{
    class Entry;

    /// @brief Base class of templated `EntryReference` for use with `Entry` referral management.
    class EntryReferenceBase
    {
    public:
        virtual ~EntryReferenceBase() = default;

        virtual void Clear() = 0;

        virtual void OnReferencedEntryDestroy() = 0;
    };

    /// @brief Non-owning reference from one MSB `Entry` (of any subtype) to another (subtype of `T`).
    /// References are registered in the target entry so that they can be cleared when that entry is removed.
    template <typename T>
    class FIRELINK_ER_API EntryReference final : public EntryReferenceBase
    {
    public:
        /// @brief Default constructor (empty reference).
        EntryReference() : m_destEntry(nullptr) {}

        /// @brief Constructor with dest entry.
        explicit EntryReference(T* destEntry) : m_destEntry(destEntry) {}

        /// @brief Destructor calls `Clear()` to unregister the source entry as a referrer in the dest entry.
        ~EntryReference() override;

        /// @brief Reference cannot be copy-constructed.
        EntryReference(const EntryReference&) = delete;

        /// @brief Copy assignment copies the destination entry.
        EntryReference& operator=(const EntryReference& otherReference);

        /// @brief Reference cannot be move-constructed.
        EntryReference(EntryReference&&) = delete;

        /// @brief Reference cannot be move-assigned.
        EntryReference& operator=(EntryReference&&) = delete;

        /// @brief Get the current (non-owning) destination `Entry` pointer.
        [[nodiscard]] T* Get() const { return m_destEntry; }

        /// @brief Set the current (non-owning) destination `Entry` pointer (clearing any previous one).
        void Set(T* destEntry);

        /// @brief Set the current (non-owning) destination `Entry` pointer (clearing any previous one).
        /// Convenient overload that just calls `Set(destEntry.get())`.
        void Set(const std::unique_ptr<T>& destEntry);

        /// @brief Clear the reference and remove it from the target Entry's referrers.
        void Clear() override;

        /// @brief Clone the reference to the destination with a new source entry.
        [[nodiscard]] EntryReference Clone() const;

        /// @brief Assignment to a pointer can be used to update the destination entry.
        EntryReference& operator=(T* destEntry);

        /// @brief Clear destination entry if it matches the given entry.
        /// Does not unregister the reference from the target entry, as it is the target entry's deletion that triggers
        /// this function.
        void OnReferencedEntryDestroy(const T* entry);

        // De/serialization methods are private, to be used by friend class `Entry` only.

        /// @brief Set the current reference from a 32-bit index into deserialized `entries`.
        void SetFromIndex(const std::vector<T*>& entries, int32_t index);

        /// @brief Set the current reference from a 16-bit index into deserialized `entries`.
        void SetFromIndex16(const std::vector<T*>& entries, int16_t index);

        /// @Brief Convert this reference to a 32-bit index into `entries` for serialization.
        ///
        /// `entries` may be a combined supertype list (most common) or, rarely, a specific subtype list.
        [[nodiscard]] int32_t ToIndex(const Entry* sourceEntry, const std::vector<T*>& entries) const;

        /// @Brief Convert this reference to a 16-bit index into `entries` for serialization.
        ///
        /// `entries` may be a combined supertype list (most common) or, rarely, a specific subtype list.
        [[nodiscard]] int16_t ToIndex16(const Entry* sourceEntry, const std::vector<T*>& entries) const;

        // `Entry` is a friend class so it can call the `OnReferenceEntryDestroy()` protected method.
        // This method clears the destination entry pointer without (recursively) removing this as a referrer.
        friend Entry;

    protected:
        /// @brief Called by friend class `Entry` to remove the pointer to the destination entry.
        void OnReferencedEntryDestroy() override { m_destEntry = nullptr; }

    private:
        T* m_destEntry = nullptr;
    };

    /// @brief Update all indices in `indices` to the index in `entries` of corresponding `references`.
    template<typename T, size_t N>
    void SetIndexArray(
        const Entry* sourceEntry,
        std::array<int32_t, N>& indices,
        std::array<EntryReference<T>, N>& references,
        const std::vector<T*>& entries)
    {
        for (size_t i = 0; i < N; i++)
            indices[i] = references[i].ToIndex(sourceEntry, entries);
    }

    /// @brief Update all 16-bit indices in `indices` to the index in `entries` of corresponding `references`.
    template<typename T, size_t N>
    void SetIndexArray16(
        const Entry* sourceEntry,
        std::array<int16_t, N>& indices,
        const std::array<EntryReference<T>, N>& references,
        const std::vector<T*>& entries)
    {
        for (size_t i = 0; i < N; i++)
            indices[i] = references[i].ToIndex16(sourceEntry, entries);
    }

    /// @brief Update all `references` to point to the corresponding entry from `indices` in `entries`.
    template<typename T, size_t N>
    void SetReferenceArray(
        std::array<EntryReference<T>, N>& references,
        const std::array<int32_t, N>& indices,
        const std::vector<T*>& entries)
    {
        for (size_t i = 0; i < N; i++)
            references[i].SetFromIndex(entries, indices[i]);
    }

    /// @brief Update all `references` to point to the corresponding entry from 16-bit `indices` in `entries`.
    template<typename T, size_t N>
    void SetReferenceArray16(
        std::array<EntryReference<T>, N>& references,
        std::array<int16_t, N> indices,
        const std::vector<T*>& entries)
    {
        for (size_t i = 0; i < N; i++)
            references[i].SetFromIndex16(entries, indices[i]);
    }

    /// @brief Clear all references inside the given array.
    template<typename T, size_t N>
    void ClearReferenceArray(std::array<EntryReference<T>, N>& references)
    {
        for (size_t i = 0; i < N; i++)
            references[i].Clear();
    }

    // class Model;
    // extern template class EntryReference<Model>;
    // class Event;
    // extern template class EntryReference<Event>;
    // class Region;
    // extern template class EntryReference<Region>;
    // class Part;
    // extern template class EntryReference<Part>;
    // class PatrolRouteEvent;
    // extern template class EntryReference<PatrolRouteEvent>;
    // class CollisionPart;
    // extern template class EntryReference<CollisionPart>;
}