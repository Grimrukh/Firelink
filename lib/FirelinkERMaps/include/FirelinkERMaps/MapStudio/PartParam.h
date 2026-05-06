#pragma once

#include "EntryParam.h"
#include "Enums.h"
#include "Part.h"

#include <FirelinkERMaps/Export.h>

#define CASE_MAKE_UNIQUE(ENUM_TYPE)                    \
    case static_cast<int>(PartType::ENUM_TYPE):        \
    {                                                  \
        newPart = std::make_unique<ENUM_TYPE##Part>(); \
        break;                                         \
    }

namespace Firelink::EldenRing::Maps::MapStudio
{
    class FIRELINK_ER_MAPS_API PartParam final : public EntryParam<Part>
    {
    public:
        PartParam()
        : EntryParam(73, u"PARTS_PARAM_ST")
        {
        }

        /// @brief Create a new Part with no name.
        [[nodiscard]] Part* GetNewEntry(const int entrySubtype) override
        {
            std::unique_ptr<Part> newPart;
            switch (entrySubtype)
            {
                CASE_MAKE_UNIQUE(MapPiece)
                CASE_MAKE_UNIQUE(Character)
                CASE_MAKE_UNIQUE(PlayerStart)
                CASE_MAKE_UNIQUE(Collision)
                CASE_MAKE_UNIQUE(DummyAsset)
                CASE_MAKE_UNIQUE(DummyCharacter)
                CASE_MAKE_UNIQUE(ConnectCollision)
                CASE_MAKE_UNIQUE(Asset)
                default:
                    throw MSBFormatError(std::format("Invalid Part subtype: {}", static_cast<int>(entrySubtype)));
            }

            std::vector<std::unique_ptr<Part>>& subtypeVector = m_entriesBySubtype.at(static_cast<int>(entrySubtype));
            subtypeVector.push_back(std::move(newPart));
            return subtypeVector.back().get();
        }
    };
} // namespace Firelink::EldenRing::Maps::MapStudio

#undef CASE_MAKE_UNIQUE