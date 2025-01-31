#pragma once

#include "GrimHookER/Export.h"
#include "EntryParam.h"
#include "Enums.h"
#include "Model.h"

#define CASE_MAKE_UNIQUE(ENUM_TYPE) \
    case static_cast<int>(ModelType::ENUM_TYPE): \
    { \
        newModel = std::make_unique<ENUM_TYPE##Model>(); \
        break; \
    }

namespace GrimHookER::Maps::MapStudio
{
    class GRIMHOOKER_API ModelParam final : public EntryParam<Model>
    {
    public:
        ModelParam() : EntryParam(73, "MODEL_PARAM_ST") {}

        /// @brief Create a new Model with no name.
        [[nodiscard]] Model* GetNewEntry(const int entrySubtype) override
        {
            std::unique_ptr<Model> newModel;
            switch (entrySubtype)
            {
                case static_cast<int>(ModelType::MapPiece): { newModel = std::make_unique<MapPieceModel>(); break; }
                case static_cast<int>(ModelType::Character): { newModel = std::make_unique<CharacterModel>(); break; }
                case static_cast<int>(ModelType::Player): { newModel = std::make_unique<PlayerModel>(); break; }
                case static_cast<int>(ModelType::Collision): { newModel = std::make_unique<CollisionModel>(); break; }
                case static_cast<int>(ModelType::Asset): { newModel = std::make_unique<AssetModel>(); break; }
                default:
                    throw MSBFormatError(std::format(
                        "Invalid Model subtype: {}", static_cast<int>(entrySubtype)));
            }

            std::vector<std::unique_ptr<Model>>& subtypeVector = m_entriesBySubtype.at(static_cast<int>(entrySubtype));
            subtypeVector.push_back(std::move(newModel));
            return subtypeVector.back().get();
        }
    };
}

#undef CASE_MAKE_UNIQUE