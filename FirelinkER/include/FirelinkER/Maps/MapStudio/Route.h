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
    class FIRELINK_ER_API Route : public Entry
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

        explicit Route(const std::u16string& name) : Entry(name) {}

        [[nodiscard]] virtual RouteType GetType() const = 0;

        [[nodiscard]] int32_t GetHUnk0C() const { return m_hUnk0C; }
        void SetHUnk0C(const int32_t value) { m_hUnk0C = value; }

        [[nodiscard]] int32_t GetHUnk08() const { return m_hUnk08; }
        void SetHUnk08(const int32_t value) { m_hUnk08 = value; }

        [[nodiscard]] int32_t GetSubtypeIndexOverride() const { return subtypeIndexOverride; }
        void SetSubtypeIndexOverride(const int32_t value) { subtypeIndexOverride = value; }

        void Deserialize(std::ifstream& stream) override;
        void Serialize(std::ofstream& stream, int supertypeIndex, int subtypeIndex) const override;

        explicit operator std::string() const
        {
            return std::format("{}[\"{}\"]", GetTypeNames().at(GetType()), GetNameUTF8());
        }

        // No subtype data to read/write.
    protected:
        int32_t m_hUnk08 = 0;
        int32_t m_hUnk0C = 0;
        int32_t subtypeIndexOverride = -1;  // used by `OtherRoute` if non-negative
    };

    class FIRELINK_ER_API MufflingPortalLinkRoute final : public Route
    {
    public:
        MufflingPortalLinkRoute() = default;

        explicit MufflingPortalLinkRoute(const std::u16string& name) : Route(name) {}

        [[nodiscard]] RouteType GetType() const override { return RouteType::MufflingPortalLink; }
    };

    class FIRELINK_ER_API MufflingBoxLinkRoute final : public Route
    {
    public:
        MufflingBoxLinkRoute() = default;

        explicit MufflingBoxLinkRoute(const std::u16string& name) : Route(name) {}

        [[nodiscard]] RouteType GetType() const override { return RouteType::MufflingBoxLink; }
    };

    class FIRELINK_ER_API OtherRoute final : public Route
    {
    public:
        OtherRoute() = default;

        explicit OtherRoute(const std::u16string& name) : Route(name) {}

        [[nodiscard]] RouteType GetType() const override { return RouteType::Other; }
    };
}
