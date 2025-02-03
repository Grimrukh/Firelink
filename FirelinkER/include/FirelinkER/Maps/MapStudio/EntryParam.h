#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "FirelinkER/Export.h"


namespace FirelinkER::Maps::MapStudio
{
    /// @brief MSB entry supertype list, which maps subtypes of that supertype (by enum) to their unique instances.
    ///
    /// Use `AddEntry()` and `RemoveEntry()` at this level to manage the underlying mapped entries.
    template <typename T>  // Model, Event, Part, Region, Route, Layer
    class FIRELINK_ER_API EntryParam
    {
    public:
        EntryParam(int version, std::u16string name);

        // Delete copy constructor.
        EntryParam(const EntryParam&) = delete;
        // Delete copy assignment operator.
        EntryParam& operator=(const EntryParam&) = delete;

        [[nodiscard]] int GetVersion() const { return m_version; }
        void SetVersion(const int newVersion) { m_version = newVersion; }

        /// @brief Get name of this MSB Param (not mutable).
        [[nodiscard]] const std::u16string& GetParamName() const { return m_name; }
        [[nodiscard]] const std::string& GetParamNameUTF8() const { return m_nameUtf8; }

        /// @brief Get a const reference to the underlying unordered map from subtype to `T` vector.
        [[nodiscard]] const std::unordered_map<int, std::vector<std::unique_ptr<T>>>& GetEntriesBySubtype() const
        {
            return m_entriesBySubtype;
        }

        template<typename ST>
        [[nodiscard]] std::vector<ST*> GetSubtypeEntries() const
        {
            static_assert(std::is_base_of_v<T, ST>, "Specified subtype is not derived from Param entry supertype.");
            const std::vector<std::unique_ptr<T>>& subtypeEntries = m_entriesBySubtype.at(static_cast<int>(ST::Type));
            std::vector<ST*> result;
            result.reserve(subtypeEntries.size());
            for (const auto& entry : subtypeEntries)
                result.push_back(static_cast<ST*>(entry.get()));
            return result;
        }

        /// @brief Detects subtype of `entry`, takes ownership, and moves it to the appropriate mapped vector.
        void AddEntry(std::unique_ptr<T> entry);

        /// @brief Remove the given entry from its appropriate mapped vector.
        void RemoveEntry(T* entry);

        /// @brief Create an `T` of subtype `entrySubtype`. Should typically be used with `AddEntry()` immediately.
        [[nodiscard]] virtual T* GetNewEntry(int entrySubtype) = 0;

        /// @brief Read fixed Param header data and create all subtypes entries in their owning `EntrySubParam`s.
        ///
        /// @return List of non-owning pointers to all entries, in order, for reference deserialization.
        std::vector<T*> Deserialize(std::ifstream& stream);

        /// @brief Write fixed Param header and all subtype lists, in subtype enum order.
        ///
        /// @return Offset at which next entry list offset should be written.
        std::streampos Serialize(std::ofstream& stream);

        /// @brief Deserialize and create a new entry of this list's supertype.
        ///
        /// @return A non-owning pointer to the created `Entry`.
        T* DeserializeEntry(std::ifstream& stream);

        /// @brief Retrieve non-owning pointers to all entries of all subtypes in MSB order.
        ///
        /// Does NOT do any automatic within-subtype sorting.
        [[nodiscard]] std::vector<T*> GetAllEntries() const;

        /// @brief Get total number of entries across all subtypes in this supertype.
        [[nodiscard]] size_t GetSize() const;

        /// @brief Clear all entries from all subtypes in this supertype.
        void ClearAllEntries();

        /// @brief Convert this `EntryParam` to a string.
        explicit operator std::string() const;

        /// @brief Iterator definition. Iterates over all `(subtypeInt, subtypeVector)` pairs in subtype order.
        class Iterator
        {
        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type = std::pair<int, std::vector<std::unique_ptr<T>>&>;
            using difference_type = std::ptrdiff_t;
            using pointer = value_type*;
            using reference = value_type&;

            Iterator(
                std::vector<int>::const_iterator keyIt,
                std::unordered_map<int, std::vector<std::unique_ptr<T>>>& container)
                : m_keyIt(std::move(keyIt)), m_container(container) {}

            value_type operator*()
            {
                return { *m_keyIt, m_container.at(*m_keyIt) };
            }

            Iterator& operator++()
            {
                ++m_keyIt;
                return *this;
            }

            bool operator==(const Iterator& other) const { return m_keyIt == other.m_keyIt; }
            bool operator!=(const Iterator& other) const { return m_keyIt != other.m_keyIt; }

        private:
            std::vector<int>::const_iterator m_keyIt;
            std::unordered_map<int, std::vector<std::unique_ptr<T>>>& m_container;
        };

        /// @brief Begin iterator for iterating over all `(subtypeInt, subtypeVector)` pairs in subtype order.
        [[nodiscard]] Iterator begin()
        {
            return Iterator(m_subtypeOrder.begin(), m_entriesBySubtype);
        }

        /// @brief End iterator for iterating over all `(subtypeInt, subtypeVector)` pairs in subtype order.
        [[nodiscard]] Iterator end()
        {
            return Iterator(m_subtypeOrder.end(), m_entriesBySubtype);
        }

    protected:
        int m_version = 0;
        const std::u16string m_name;
        const std::string m_nameUtf8;  // for logging, etc.

        // Initialized with all valid enum types in constructor.
        std::unordered_map<int, std::vector<std::unique_ptr<T>>> m_entriesBySubtype;
        // Key order for ordered subtype vector retrieval from above.
        std::vector<int> m_subtypeOrder;

        /// @brief Uses `unique_ptr` for entries, so no explicit deletion needed.
        ~EntryParam() = default;
    };

    // class Model;
    // extern template class EntryParam<Model>;
    // class Event;
    // extern template class EntryParam<Event>;
    // class Region;
    // extern template class EntryParam<Region>;
    // class Part;
    // extern template class EntryParam<Part>;
    // class Layer;
    // extern template class EntryParam<Layer>;
    // class Route;
    // extern template class EntryParam<Route>;
}
