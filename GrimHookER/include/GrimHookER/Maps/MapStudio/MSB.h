#pragma once

#include <filesystem>

#include "GrimHookER/Export.h"
#include "EventParam.h"
#include "ModelParam.h"
#include "PartParam.h"
#include "RegionParam.h"
#include "RouteParam.h"

namespace GrimHookER::Maps::MapStudio
{
    class GRIMHOOKER_API MSB
    {
    public:
        /// @brief Create an empty MSB. All entry lists will be initialized to empty.
        MSB() = default;

        /// @brief MSB cannot be copy-constructed.
        MSB(const MSB&) = delete;

        /// @brief MSB cannot be copy-assigned.
        MSB& operator=(const MSB&) = delete;

        /// @brief MSB cannot be move-constructed.
        MSB(MSB&&) = delete;

        /// @brief MSB cannot be move-assigned.
        MSB& operator=(MSB&&) = delete;

        /// @Brief Deserialize an MSB into this instance from a filestream.
        void Deserialize(std::ifstream& stream);

        /// @brief Serialize this MSB to a filestream.
        void Serialize(std::ofstream& stream);

        /// @brief Heap-allocate and deserialize a new MSB from a file.
        static std::unique_ptr<MSB> NewFromFilePath(const std::filesystem::path& path);

        /// @brief Serialize this MSB and write it to a file path.
        void WriteToFilePath(const std::filesystem::path& path);

        // Getters for references to supertype entry params (cannot be copied).
        [[nodiscard]] ModelParam& GetModelParam() { return m_modelParam; }
        [[nodiscard]] EventParam& GetEventParam() { return m_eventParam; }
        [[nodiscard]] RegionParam& GetRegionParam() { return m_regionParam; }
        [[nodiscard]] RouteParam& GetRouteParam() { return m_routeParam; }
        // NOTE: `LayerParam` goes here in file order (always asserted empty).
        [[nodiscard]] PartParam& GetPartParam() { return m_partParam; }

    protected:
        static void ReadHeader(std::ifstream& stream);
        static void WriteHeader(std::ofstream& stream);

    private:
        ModelParam m_modelParam;
        EventParam m_eventParam;
        RegionParam m_regionParam;
        RouteParam m_routeParam;
        // `LayerParam` goes here in file order.
        PartParam m_partParam;

        /// @brief Update all indices of referenced MSB entries.
        static void SerializeEntryIndices(
            const std::vector<Model*>& models,
            const std::vector<Event*>& events,
            const std::vector<Region*>& regions,
            const std::vector<Part*>& parts);

        /// @brief Update all referenced MSB entries based on indices. (-1 -> nullptr)
        static void DeserializeEntryReferences(
            const std::vector<Model*>& models,
            const std::vector<Event*>& events,
            const std::vector<Region*>& regions,
            const std::vector<Part*>& parts);
    };
}