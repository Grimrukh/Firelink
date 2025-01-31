#pragma once

#include "GrimHookER/Export.h"
#include "EntryParam.h"
#include "Enums.h"
#include "Model.h"

#define CASE_MAKE_UNIQUE(ENUM_TYPE) \
    case ModelType::ENUM_TYPE: \
        newModel = std::make_unique<ENUM_TYPE##Model>(""); \
        break;

namespace GrimHookER::Maps::MapStudio
{
    class GRIMHOOKER_API ModelParam final : public EntryParam<Model, ModelType>
    {
    public:
        ModelParam() : EntryParam(73, "MODEL_PARAM_ST") {}

        /// @brief Create a new Model with no name.
        [[nodiscard]] Model* GetNewEntry(const ModelType entrySubtype) override
        {
            std::unique_ptr<Model> newModel;
            switch (entrySubtype)
            {
                CASE_MAKE_UNIQUE(MapPiece)
                CASE_MAKE_UNIQUE(Character)
                CASE_MAKE_UNIQUE(Player)
                CASE_MAKE_UNIQUE(Collision)
                CASE_MAKE_UNIQUE(Asset)
                default:
                    throw MSBFormatError(std::format(
                        "Invalid Model subtype: {}", static_cast<int>(entrySubtype)));
            }

            m_entriesBySubtype.at(entrySubtype).push_back(std::move(newModel));
            return m_entriesBySubtype.at(entrySubtype).back().get();
        }
    };
}

#undef CASE_MAKE_UNIQUE