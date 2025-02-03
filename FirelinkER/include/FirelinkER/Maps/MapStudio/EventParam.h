#pragma once

#include <format>

#include "FirelinkER/Export.h"
#include "EntryParam.h"
#include "Enums.h"
#include "Event.h"
#include "MSBFormatError.h"

#define CASE_MAKE_UNIQUE(ENUM_TYPE) \
    case static_cast<int>(EventType::ENUM_TYPE): \
    { \
        newEvent = std::make_unique<ENUM_TYPE##Event>(); \
        break; \
    }

namespace FirelinkER::Maps::MapStudio
{
    class FIRELINK_ER_API EventParam final : public EntryParam<Event>
    {
    public:
        EventParam() : EntryParam(73, u"EVENT_PARAM_ST") {}

        /// @brief Create a new Event with no name.
        [[nodiscard]] Event* GetNewEntry(const int entrySubtype) override
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

            std::vector<std::unique_ptr<Event>>& subtypeVector = m_entriesBySubtype.at(static_cast<int>(entrySubtype));
            subtypeVector.push_back(std::move(newEvent));
            return subtypeVector.back().get();
        }
    };
}

#undef CASE_MAKE_UNIQUE