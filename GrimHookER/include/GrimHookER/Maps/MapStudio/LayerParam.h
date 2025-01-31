#pragma once

#include "EntryParam.h"
#include "Enums.h"
#include "Layer.h"

namespace GrimHookER::Maps::MapStudio
{
    /// @brief Never properly instantiated; class is only used to read/write the constant header.
    class LayerParam final : public EntryParam<Layer, LayerType>
    {
    public:
        LayerParam() : EntryParam(73, "LAYER_PARAM_ST") {}

        /// @brief Implemented for completion; cannot be called.
        [[nodiscard]] Layer* GetNewEntry(const LayerType entrySubtype) override
        {
            throw MSBFormatError("Cannot create `Layer` instances. LayerParam must be empty.");
        }
    };

}