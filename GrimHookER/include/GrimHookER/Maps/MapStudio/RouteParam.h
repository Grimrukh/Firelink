#pragma once

#include "GrimHookER/Export.h"
#include "EntryParam.h"
#include "Route.h"

#define CASE_MAKE_UNIQUE(ENUM_TYPE) \
    case RouteType::ENUM_TYPE: \
        newRoute = std::make_unique<ENUM_TYPE##Route>(""); \
        break;


namespace GrimHookER::Maps::MapStudio
{
    class GRIMHOOKER_API RouteParam final : public EntryParam<Route, RouteType>
    {
    public:
        RouteParam() : EntryParam(73, "ROUTE_PARAM_ST") {}

        /// @brief Create a new Route with no name.
        [[nodiscard]] Route* GetNewEntry(const RouteType entrySubtype) override
        {
            std::unique_ptr<Route> newRoute;
            switch (entrySubtype)
            {
                CASE_MAKE_UNIQUE(MufflingPortalLink)
                CASE_MAKE_UNIQUE(MufflingBoxLink)
                CASE_MAKE_UNIQUE(Other)
                default:
                    throw MSBFormatError(std::format(
                        "Invalid Route subtype: {}", static_cast<int>(entrySubtype)));
            }

            m_entriesBySubtype.at(entrySubtype).push_back(std::move(newRoute));
            return m_entriesBySubtype.at(entrySubtype).back().get();
        }
    };
}

#undef CASE_MAKE_UNIQUE