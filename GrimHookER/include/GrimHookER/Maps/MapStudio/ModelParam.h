#pragma once

#include <variant>
#include <vector>

#include "GrimHookER/Export.h"
#include "EntryParam.h"
#include "Model.h"

namespace GrimHookER::Maps::MapStudio
{
    class GRIMHOOKER_API ModelParam final : public EntryParam<Model, ModelVariantType>
    {
    public:
        // These vectors are the owners of the unique Entry pointers.
        ModelParam() : EntryParam(73, "MODEL_PARAM_ST") {}

        /// @brief Create a new Model with the given name.
        template<typename T>
        [[nodiscard]] std::unique_ptr<T> Create()
        {
            static_assert(std::is_base_of_v<Model, T>, "T must derive from Model");
            return std::make_unique<T>();
        }

        /// @brief Get entries (as vector reference) of a specific Model subtype.
        template<typename T>
        [[nodiscard]] std::vector<T>& Get()
        {
            static_assert(std::is_base_of_v<Model, T>, "T must derive from Model");
            return std::get<std::vector<T>>(m_subtypeEntries[T::Type]);
        }
    };
}
