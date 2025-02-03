#pragma once

#include <cstdint>
#include <iostream>
#include <string>
#include <unordered_set>
#include <utility>

#include "FirelinkER/Export.h"
#include "EntryReference.h"
#include "Firelink/MemoryUtils.h"

namespace FirelinkER::Maps::MapStudio
{
    /// @brief Base class for any entry in any MSB entry list. Only base field is `name`.
    class FIRELINK_ER_API Entry
    {
    public:
        /// @brief Create a completely empty `Entry` with an empty name.
        Entry() = default;

        /// @brief Create an `Entry` with a given name.
        explicit Entry(std::u16string name) : m_name(std::move(name)) {}

        virtual ~Entry()
        {
            // Clear all incoming references to this entry.
            for (EntryReferenceBase* referrer : m_incomingReferences)
                referrer->OnReferencedEntryDestroy();
            m_incomingReferences.clear();

            // Note that `EntryReference` pointers clear their references on destruction (calling `RemoveReferrer()`
            // on their destination `Entry`), so Entry destructors do not need to manually do this.
        }

        [[nodiscard]] const std::u16string& GetName() const { return m_name; }
        void SetName(const std::u16string& newName) { m_name = newName; }
        [[nodiscard]] std::string GetNameUTF8() const { return Firelink::UTF16ToUTF8(m_name); }

        /// @brief Deserialize an entry from a filestream.
        virtual void Deserialize(std::ifstream& stream) = 0;
        // Supertype and/or subtype indices may not be used by all subclasses.
        virtual void Serialize(std::ofstream& stream, int supertypeIndex, int subtypeIndex) const = 0;

        /// @brief Register an incoming reference from another `Entry`.
        void AddReferrer(EntryReferenceBase* referrer)
        {
            if (m_incomingReferences.contains(referrer))
                throw std::runtime_error("Referrer already exists in Entry. Cannot remove it.");
            m_incomingReferences.insert(referrer);
        }

        /// @brief Unregister an incoming reference from another `Entry`.
        /// Called when the referrer changes or clears its reference, or is removed from the MSB.
        void RemoveReferrer(EntryReferenceBase* referrer)
        {
            if (!m_incomingReferences.contains(referrer))
                throw std::runtime_error("Referrer does not exist in Entry. Cannot remove it.");
            m_incomingReferences.erase(referrer);
        }

    protected:
        std::u16string m_name;

        /// @brief Virtual method to clear all references to this `Entry` by the referrer.
        /// Does nothing by default (though this base method will never feature in an incoming reference, by design).
        virtual void OnReferencedEntryDestroy(const Entry* entry) {}

    private:
        /// @brief Stores non-owning pointers to all entries that reference this one.
        std::unordered_set<EntryReferenceBase*> m_incomingReferences{};
    };

    /// @brief Extended base class for entries with entity IDs, which frequently get special treatment as types.
    /// NOTE: Not used by certain subtypes whose `entityId` field is not a real ID that EMEVD event scripts can use.
    class EntityEntry : public Entry
    {
    public:
        EntityEntry() = default;

        explicit EntityEntry(std::u16string name) : Entry(std::move(name)) {}

        [[nodiscard]] uint32_t GetEntityId() const { return m_entityId; }
        void SetEntityId(const uint32_t entityId) { this->m_entityId = entityId; }

    protected:
        uint32_t m_entityId = 0;  // note unsigned
    };
}
