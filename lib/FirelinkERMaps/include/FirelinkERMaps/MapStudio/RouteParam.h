#pragma once

#include "EntryParam.h"
#include "Route.h"

#include <FirelinkERMaps/Export.h>

#define CASE_MAKE_UNIQUE(ENUM_TYPE)                      \
    case static_cast<int>(RouteType::ENUM_TYPE):         \
    {                                                    \
        newRoute = std::make_unique<ENUM_TYPE##Route>(); \
        break;                                           \
    }

namespace Firelink::EldenRing::Maps::MapStudio
{
    class FIRELINK_ER_MAPS_API RouteParam final : public EntryParam<Route>
    {
    public:
        RouteParam()
        : EntryParam(73, u"ROUTE_PARAM_ST")
        {
        }

        /// @brief Create a new Route with no name.
        [[nodiscard]] Route* GetNewEntry(const int entrySubtype) override
        {
            std::unique_ptr<Route> newRoute;
            switch (entrySubtype)
            {
                CASE_MAKE_UNIQUE(MufflingPortalLink)
                CASE_MAKE_UNIQUE(MufflingBoxLink)
                CASE_MAKE_UNIQUE(Other)
                default:
                    throw MSBFormatError(std::format("Invalid Route subtype: {}", static_cast<int>(entrySubtype)));
            }

            std::vector<std::unique_ptr<Route>>& subtypeVector = m_entriesBySubtype.at(static_cast<int>(entrySubtype));
            subtypeVector.push_back(std::move(newRoute));
            return subtypeVector.back().get();
        }
    };
} // namespace Firelink::EldenRing::Maps::MapStudio

#undef CASE_MAKE_UNIQUE