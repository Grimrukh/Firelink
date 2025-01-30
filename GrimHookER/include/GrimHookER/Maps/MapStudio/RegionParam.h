#pragma once

#include <variant>
#include <vector>

#include "GrimHookER/Export.h"
#include "EntryParam.h"
#include "Region.h"


namespace GrimHookER::Maps::MapStudio
{
    class GRIMHOOKER_API RegionParam final : public EntryParam<Region, RegionVariantType>
    {
    public:
        RegionParam() : EntryParam(73, "POINT_PARAM_ST") {}

        /// @brief Create a new Region with the given name.
        template<typename T>
        [[nodiscard]] std::unique_ptr<T> Create()
        {
            static_assert(std::is_base_of_v<Region, T>, "T must derive from Region");
            return std::make_unique<T>();
        }

        /// @brief Get entries (as vector reference) of a specific Region subtype.
        template<typename T>
        [[nodiscard]] std::vector<T>& Get()
        {
            static_assert(std::is_base_of_v<Region, T>, "T must derive from Region");
            return std::get<std::vector<T>>(m_subtypeEntries[T::Type]);
        }
    };
}