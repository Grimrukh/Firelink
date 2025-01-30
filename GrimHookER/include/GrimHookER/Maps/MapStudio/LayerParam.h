#pragma once

#include "EntryParam.h"
#include "Layer.h"

namespace GrimHookER::Maps::MapStudio
{
    /// @brief Never properly instantiated; class is only used to read/write the constant header.
    class LayerParam final : public EntryParam<Layer, LayerVariantType>
    {
    public:
        LayerParam() : EntryParam(73, "LAYER_PARAM_ST") {}
    };

}