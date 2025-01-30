#pragma once

#include <variant>
#include <vector>

#include "GrimHookER/Export.h"
#include "EntryParam.h"
#include "Route.h"


namespace GrimHookER::Maps::MapStudio
{
    class GRIMHOOKER_API RouteParam final : public EntryParam<Route, RouteVariantType>
    {
    public:
        RouteParam() : EntryParam(73, "ROUTE_PARAM_ST") {}

        /// @brief Create a new Route with the given name.
        template<typename T>
        [[nodiscard]] std::unique_ptr<T> Create()
        {
            static_assert(std::is_base_of_v<Route, T>, "T must derive from Route");
            return std::make_unique<T>();
        }

        /// @brief Get entries (as vector reference) of a specific Route subtype.
        template<typename T>
        [[nodiscard]] std::vector<T>& Get()
        {
            static_assert(std::is_base_of_v<Route, T>, "T must derive from Route");
            return std::get<std::vector<T>>(m_subtypeEntries[T::Type]);
        }
    };
}
