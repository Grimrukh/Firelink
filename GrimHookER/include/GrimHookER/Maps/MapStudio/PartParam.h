#pragma once

#include <variant>
#include <vector>

#include "GrimHookER/Export.h"
#include "EntryParam.h"
#include "Part.h"


namespace GrimHookER::Maps::MapStudio
{
    class GRIMHOOKER_API PartParam final : public EntryParam<Part, PartVariantType>
    {
    public:
        PartParam() : EntryParam(73, "PARTS_PARAM_ST") {}

        /// @brief Create a new Part with the given name.
        template<typename T>
        [[nodiscard]] std::unique_ptr<T> Create()
        {
            static_assert(std::is_base_of_v<Part, T>, "T must derive from Part");
            return std::make_unique<T>();
        }

        /// @brief Get entries (as vector reference) of a specific Part subtype.
        template<typename T>
        [[nodiscard]] std::vector<T>& Get()
        {
            static_assert(std::is_base_of_v<Part, T>, "T must derive from Part");
            return std::get<std::vector<T>>(m_subtypeEntries[T::Type]);
        }
    };
}