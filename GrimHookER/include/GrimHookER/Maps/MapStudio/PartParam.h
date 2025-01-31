#pragma once

#include "GrimHookER/Export.h"
#include "EntryParam.h"
#include "Enums.h"
#include "Part.h"

#define CASE_MAKE_UNIQUE(ENUM_TYPE) \
    case PartType::ENUM_TYPE: \
        newPart = std::make_unique<ENUM_TYPE##Part>(""); \
        break;


namespace GrimHookER::Maps::MapStudio
{
    class GRIMHOOKER_API PartParam final : public EntryParam<Part, PartType>
    {
    public:
        PartParam() : EntryParam(73, "PARTS_PARAM_ST") {}

        /// @brief Create a new Part with no name.
        [[nodiscard]] Part* GetNewEntry(const PartType entrySubtype) override
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

            m_entriesBySubtype.at(entrySubtype).push_back(std::move(newPart));
            return m_entriesBySubtype.at(entrySubtype).back().get();
        }
    };
}

#undef CASE_MAKE_UNIQUE