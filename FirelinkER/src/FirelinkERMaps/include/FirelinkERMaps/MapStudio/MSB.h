#pragma once

#include <FirelinkERMaps/MapStudio/EventParam.h>
#include <FirelinkERMaps/MapStudio/ModelParam.h>
#include <FirelinkERMaps/MapStudio/PartParam.h>
#include <FirelinkERMaps/MapStudio/RegionParam.h>
#include <FirelinkERMaps/MapStudio/RouteParam.h>

#include <FirelinkERMaps/Export.h>

#include <FirelinkCore/DCX.h>

#include <filesystem>

namespace Firelink::BinaryReadWrite { class BufferReader; class BufferWriter; }

namespace Firelink::EldenRing::Maps::MapStudio
{
    class FIRELINK_ER_MAPS_API MSB
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


        /// @brief Deserialize an MSB into this instance from a BufferReader.
        /// @todo: Replace with FromBytes.
        void Deserialize(Firelink::BinaryReadWrite::BufferReader& reader);

        /// @brief Serialize this MSB to a BufferWriter.
        /// @todo: Replace with ToBytes.
        void Serialize(Firelink::BinaryReadWrite::BufferWriter& writer);

        /// @brief Heap-allocate and deserialize a new MSB from a file.
        static std::unique_ptr<MSB> FromPath(const std::filesystem::path& path);

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
        static void ReadHeader(Firelink::BinaryReadWrite::BufferReader& reader);
        static void WriteHeader(Firelink::BinaryReadWrite::BufferWriter& writer);

    private:
        DCXType m_dcxType = DCXType::Null;

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
} // namespace Firelink::EldenRing::Maps::MapStudio