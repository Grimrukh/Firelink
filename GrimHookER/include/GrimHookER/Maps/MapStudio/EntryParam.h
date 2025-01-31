#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "GrimHookER/Export.h"


namespace GrimHookER::Maps::MapStudio
{
    /// @brief MSB entry supertype list, which maps subtypes of that supertype (by enum) to their unique instances.
    ///
    /// Use `AddEntry()` and `RemoveEntry()` at this level to manage the underlying mapped entries.
    template <typename T, typename EnumType>  // Model, Event, Part, Region, Route, Layer
    class GRIMHOOKER_API EntryParam
    {
    public:
        EntryParam(int version, std::string name);

        [[nodiscard]] int GetVersion() const { return m_version; }
        void SetVersion(const int newVersion) { m_version = newVersion; }

        /// @brief Get name of this MSB Param (not mutable).
        [[nodiscard]] const std::string& GetParamName() const { return m_name; }

        /// @brief Detects subtype of `entry`, takes ownership, and moves it to the appropriate mapped vector.
        void AddEntry(std::unique_ptr<T> entry);

        /// @brief Remove the given entry from its appropriate mapped vector.
        void RemoveEntry(T* entry);

        /// @brief Create an `T` of subtype `entrySubtype`. Should typically be used with `AddEntry()` immediately.
        [[nodiscard]] virtual T* GetNewEntry(EnumType entrySubtype) = 0;

        /// @brief Read fixed Param header data and create all subtypes entries in their owning `EntrySubParam`s.
        ///
        /// @return List of non-owning pointers to all entries, in order, for reference deserialization.
        std::vector<T*> Deserialize(std::ifstream& stream);

        /// @brief Write fixed Param header and all subtype lists, in order.
        ///
        /// Takes in the full list of non-owning entry pointers, as this is needed in advance for reference
        /// serialization anyway.
        ///
        /// @return Offset at which next entry list offset should be written.
        std::streampos Serialize(std::ofstream& stream, const std::vector<T*>& entries);

        /// @brief Deserialize and create a new entry of this list's supertype.
        ///
        /// @return A non-owning pointer to the created `Entry`.
        T* DeserializeEntry(std::ifstream& stream);

        /// @brief Retrieve non-owning pointers to all entries of all subtypes in MSB order.
        ///
        /// Does NOT do any automatic within-subtype sorting.
        std::vector<T*> GetAllEntries() const;

        /// @brief Get total number of entries across all subtypes in this supertype.
        [[nodiscard]] size_t GetSize() const;

        /// @brief Clear all entries from all subtypes in this supertype.
        void ClearAllEntries();

        /// @brief Convert this `EntryParam` to a string.
        explicit operator std::string() const;

    protected:
        int m_version;
        const std::string m_name;

        // Initialized with all valid enum types in constructor.
        std::unordered_map<EnumType, std::vector<std::unique_ptr<T>>> m_entriesBySubtype;

        /// @brief Uses `unique_ptr` for entries, so no explicit deletion needed.
        ~EntryParam() = default;
    };

    // class Model;
    // extern template class EntryParam<Model, ModelType>;
    // class Event;
    // extern template class EntryParam<Event, EventType>;
    // class Region;
    // extern template class EntryParam<Region, RegionType>;
    // class Part;
    // extern template class EntryParam<Part, PartType>;
    // class Layer;
    // extern template class EntryParam<Layer, LayerType>;
    // class Route;
    // extern template class EntryParam<Route, RouteType>;
}
