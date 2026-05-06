#pragma once

#include <FirelinkERMaps/Export.h>
#include <FirelinkERMaps/MapStudio/EventParam.h>
#include <FirelinkERMaps/MapStudio/ModelParam.h>
#include <FirelinkERMaps/MapStudio/PartParam.h>
#include <FirelinkERMaps/MapStudio/RegionParam.h>
#include <FirelinkERMaps/MapStudio/RouteParam.h>

#include <FirelinkCore/DCX.h>
#include <FirelinkCore/Endian.h>
#include <FirelinkCore/GameFile.h>

namespace Firelink::EldenRing::Maps::MapStudio
{
    namespace Firelink::BinaryReadWrite
    {
        class BufferReader;
        class BufferWriter;
    }

    class FIRELINK_ER_MAPS_API MSB : public GameFile<MSB>
    {
    public:
        /// @brief Create an empty MSB. All entry lists will be initialized to empty.
        MSB() = default;

        [[nodiscard]] static BinaryReadWrite::Endian GetEndian() noexcept
        {
            return BinaryReadWrite::Endian::Little;
        }

        /// @brief Deserialize an MSB into this instance from a BufferReader.
        void Deserialize(BinaryReadWrite::BufferReader& reader);

        /// @brief Serialize this MSB to a BufferWriter.
        void Serialize(BinaryReadWrite::BufferWriter& writer) const;

        // Getters for references to supertype entry params (cannot be copied).
        [[nodiscard]] ModelParam& GetModelParam() { return m_modelParam; }
        [[nodiscard]] EventParam& GetEventParam() { return m_eventParam; }
        [[nodiscard]] RegionParam& GetRegionParam() { return m_regionParam; }
        [[nodiscard]] RouteParam& GetRouteParam() { return m_routeParam; }
        // NOTE: `LayerParam` goes here in file order (always asserted empty).
        [[nodiscard]] PartParam& GetPartParam() { return m_partParam; }

        // Getters for const references to supertype entry params (cannot be copied).
        [[nodiscard]] const ModelParam& GetModelParamConst() const { return m_modelParam; }
        [[nodiscard]] const EventParam& GetEventParamConst() const { return m_eventParam; }
        [[nodiscard]] const RegionParam& GetRegionParamConst() const { return m_regionParam; }
        [[nodiscard]] const RouteParam& GetRouteParamConst() const { return m_routeParam; }
        // NOTE: `LayerParam` goes here in file order (always asserted empty).
        [[nodiscard]] const PartParam& GetPartParamConst() const { return m_partParam; }

        [[nodiscard]] size_t GetEntryCount() const;

    protected:
        static void ReadHeader(BinaryReadWrite::BufferReader& reader);
        static void WriteHeader(BinaryReadWrite::BufferWriter& writer);

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
} // namespace Firelink::EldenRing::Maps::MapStudio