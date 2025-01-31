#pragma once

#include <cstdint>
#include <format>
#include <map>
#include <string>

#include "FirelinkER/Export.h"
#include "Entry.h"
#include "Enums.h"


namespace FirelinkER::Maps::MapStudio
{
    // Minimal MSB type related to muffling box/portal Regions. Has two subtypes, neither of which has any subtype data.
    class FIRELINKER_API Route : public Entry
    {
    public:
        using EnumType = RouteType;

        static constexpr int SubtypeEnumOffset = 16;

        [[nodiscard]] static const std::map<RouteType, std::string>& GetTypeNames()
        {
            static const std::map<RouteType, std::string> data =
            {
                {RouteType::MufflingPortalLink, "MufflingPortalLink"},
                {RouteType::MufflingBoxLink, "MufflingBoxLink"},
                {RouteType::Other, "Other"},
            };
            return data;
        }

        Route() = default;

        explicit Route(const std::string& name) : Entry(name) {}

        [[nodiscard]] virtual RouteType GetType() const = 0;

        void Deserialize(std::ifstream& stream) override;
        void Serialize(std::ofstream& stream, int supertypeIndex, int subtypeIndex) const override;

        explicit operator std::string() const
        {
            return std::format("{}[\"{}\"]", GetTypeNames().at(GetType()), m_name);
        }

        // No subtype data to read/write.
    protected:
        int32_t unk08 = 0;
        int32_t unk0C = 0;
        int32_t subtypeIndexOverride = -1;  // used by `OtherRoute` if non-negative
    };

    class FIRELINKER_API MufflingPortalLinkRoute final : public Route
    {
    public:
        MufflingPortalLinkRoute() = default;

        explicit MufflingPortalLinkRoute(const std::string& name) : Route(name) {}

        [[nodiscard]] RouteType GetType() const override { return RouteType::MufflingPortalLink; }
    };

    class FIRELINKER_API MufflingBoxLinkRoute final : public Route
    {
    public:
        MufflingBoxLinkRoute() = default;

        explicit MufflingBoxLinkRoute(const std::string& name) : Route(name) {}

        [[nodiscard]] RouteType GetType() const override { return RouteType::MufflingBoxLink; }
    };

    class FIRELINKER_API OtherRoute final : public Route
    {
    public:
        OtherRoute() = default;

        explicit OtherRoute(const std::string& name) : Route(name) {}

        [[nodiscard]] RouteType GetType() const override { return RouteType::Other; }
    };
}
