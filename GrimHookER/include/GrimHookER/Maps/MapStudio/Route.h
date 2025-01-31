#pragma once

#include <cstdint>
#include <map>
#include <string>

#include "GrimHookER/Export.h"
#include "Entry.h"
#include "Enums.h"


namespace GrimHookER::Maps::MapStudio
{
    // Minimal MSB type related to muffling box/portal Regions. Has two subtypes, neither of which has any subtype data.
    class GRIMHOOKER_API Route : public Entry
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

        explicit Route(const std::string& name) : Entry(name) {}

        [[nodiscard]] virtual RouteType GetType() const = 0;

        void Deserialize(std::ifstream& stream) override;
        void Serialize(std::ofstream& stream, int supertypeIndex, int subtypeIndex) const override;

        explicit operator std::string() const
        {
            return GetTypeNames().at(GetType()) + " " + m_name;
        }

        // No subtype data to read/write.
    protected:
        int32_t unk08 = 0;
        int32_t unk0C = 0;
        int32_t subtypeIndexOverride = -1;  // used by `OtherRoute` if non-negative
    };

    class GRIMHOOKER_API MufflingPortalLinkRoute final : public Route
    {
    public:
        explicit MufflingPortalLinkRoute(const std::string& name) : Route(name) {}

        [[nodiscard]] RouteType GetType() const override { return RouteType::MufflingPortalLink; }
    };

    class GRIMHOOKER_API MufflingBoxLinkRoute final : public Route
    {
    public:
        explicit MufflingBoxLinkRoute(const std::string& name) : Route(name) {}

        [[nodiscard]] RouteType GetType() const override { return RouteType::MufflingBoxLink; }
    };

    class GRIMHOOKER_API OtherRoute final : public Route
    {
    public:
        explicit OtherRoute(const std::string& name) : Route(name) {}

        [[nodiscard]] RouteType GetType() const override { return RouteType::Other; }
    };
}
