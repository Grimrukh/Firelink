#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>

#include "GrimHookER/Export.h"
#include "Entry.h"

namespace GrimHookER::Maps::MapStudio
{
    enum class RouteType : uint32_t
    {
        MufflingPortalLink = 3,
        MufflingBoxLink = 4,
        Other = 0xFFFFFFFF,
    };

    inline std::map<RouteType, std::string> routeTypeNames =
    {
        {RouteType::MufflingPortalLink, "MufflingPortalLink"},
        {RouteType::MufflingBoxLink, "MufflingBoxLink"},
        {RouteType::Other, "Other"},
    };

    // Minimal MSB type related to muffling box/portal Regions. Has two subtypes, neither of which has any subtype data.
    class GRIMHOOKER_API Route : public Entry
    {
    public:
        using EnumType = RouteType;

        explicit Route(const std::string& name) : Entry(name) {}

        [[nodiscard]] virtual RouteType GetType() const = 0;

        void Deserialize(std::ifstream& stream) override;
        void Serialize(std::ofstream& stream, int supertypeIndex, int subtypeIndex) const override;

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


    using RouteVariantType = std::variant<
        std::vector<std::unique_ptr<MufflingPortalLinkRoute>>,
        std::vector<std::unique_ptr<MufflingBoxLinkRoute>>,
        std::vector<std::unique_ptr<OtherRoute>>
    >;
}
