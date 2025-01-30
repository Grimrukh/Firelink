#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "GrimHookER/Export.h"
#include "EntryType.h"


namespace GrimHookER::Maps::MapStudio
{
    /// @brief MSB entry supertype list, which maps subtypes of that supertype (by enum) to their unique instances.
    ///
    /// Use `AddEntry()` and `RemoveEntry()` at this level to manage the underlying mapped entries.
    template <EntryType T, typename VariantType>  // Model, Event, Part, Region, Route, Layer
    class GRIMHOOKER_API EntryParam
    {
    public:
        using SubtypeMap = std::unordered_map<typename T::EnumType, VariantType>;

        EntryParam(int version, std::string name);

        [[nodiscard]] int GetVersion() const { return m_version; }
        void SetVersion(const int newVersion) { m_version = newVersion; }

        /// @brief Get name of this MSB Param (not mutable).
        [[nodiscard]] const std::string& GetName() const { return m_name; }

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
        std::vector<T*> GetAllEntries();

        /// @brief Get total number of entries across all subtypes in this supertype.
        [[nodiscard]] size_t GetSize() const;

        /// @brief Clear all entries from all subtypes in this supertype.
        void ClearAllEntries();

        /// @brief Convert this `EntryParam` to a string.
        explicit operator std::string() const;

    protected:
        /// @brief All subclasses use `unique_ptr` for their entries, so no explicit deletion needed.
        ~EntryParam() = default;

        SubtypeMap m_subtypeEntries;

    private:
        int m_version;
        const std::string m_name;
    };

    // Explicit specialization declarations for all MSB entry supertypes.
    // extern template class EntryParam<Model, ModelVariantType>;
    // extern template class EntryParam<Event, EventVariantType>;
    // extern template class EntryParam<Layer, LayerVariantType>;
    // extern template class EntryParam<Part, PartVariantType>;
    // extern template class EntryParam<Region, RegionVariantType>;
    // extern template class EntryParam<Route, RouteVariantType>;
}
