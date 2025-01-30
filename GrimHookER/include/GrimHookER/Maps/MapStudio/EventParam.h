#pragma once

#include <variant>
#include <vector>

#include "GrimHookER/Export.h"
#include "EntryParam.h"
#include "Event.h"

namespace GrimHookER::Maps::MapStudio
{
    class GRIMHOOKER_API EventParam final : public EntryParam<Event, EventVariantType>
    {
    public:
        EventParam() : EntryParam(73, "EVENT_PARAM_ST") {}

        /// @brief Create a new Event with the given name.
        template<typename T>
        [[nodiscard]] std::unique_ptr<T> Create()
        {
            static_assert(std::is_base_of_v<Event, T>, "T must derive from Event");
            return std::make_unique<T>();
        }

        /// @brief Get entries (as vector reference) of a specific Event subtype.
        template<typename T>
        [[nodiscard]] std::vector<T>& Get()
        {
            static_assert(std::is_base_of_v<Event, T>, "T must derive from Event");
            return std::get<std::vector<T>>(m_subtypeEntries[T::Type]);
        }
    };
}
