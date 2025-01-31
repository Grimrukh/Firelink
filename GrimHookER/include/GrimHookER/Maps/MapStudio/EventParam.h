#pragma once

#include <format>

#include "GrimHookER/Export.h"
#include "EntryParam.h"
#include "Enums.h"
#include "Event.h"
#include "MSBFormatError.h"

#define CASE_MAKE_UNIQUE(ENUM_TYPE) \
    case EventType::ENUM_TYPE: \
        newEvent = std::make_unique<ENUM_TYPE##Event>(""); \
        break;

namespace GrimHookER::Maps::MapStudio
{
    class GRIMHOOKER_API EventParam final : public EntryParam<Event, EventType>
    {
    public:
        EventParam() : EntryParam(73, "EVENT_PARAM_ST") {}

        /// @brief Create a new Event with no name.
        [[nodiscard]] Event* GetNewEntry(const EventType entrySubtype) override
        {
            std::unique_ptr<Event> newEvent;
            switch (entrySubtype)
            {
                CASE_MAKE_UNIQUE(Treasure)
                CASE_MAKE_UNIQUE(Spawner)
                CASE_MAKE_UNIQUE(ObjAct)
                CASE_MAKE_UNIQUE(Navigation)
                CASE_MAKE_UNIQUE(NPCInvasion)
                CASE_MAKE_UNIQUE(Platoon)
                CASE_MAKE_UNIQUE(PatrolRoute)
                CASE_MAKE_UNIQUE(Mount)
                CASE_MAKE_UNIQUE(SignPool)
                CASE_MAKE_UNIQUE(RetryPoint)
                CASE_MAKE_UNIQUE(AreaTeam)
                CASE_MAKE_UNIQUE(Other)
                default:
                    throw MSBFormatError(std::format(
                        "Invalid Event subtype: {}", static_cast<int>(entrySubtype)));
            }

            m_entriesBySubtype.at(entrySubtype).push_back(std::move(newEvent));
            return m_entriesBySubtype.at(entrySubtype).back().get();
        }
    };
}

#undef CASE_MAKE_UNIQUE