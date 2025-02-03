#pragma once

#include "EntryParam.h"
#include "Layer.h"

namespace FirelinkER::Maps::MapStudio
{
    /// @brief Never properly instantiated; class is only used to read/write the constant header.
    class LayerParam final : public EntryParam<Layer>
    {
    public:
        LayerParam() : EntryParam(73, u"LAYER_PARAM_ST") {}

        /// @brief Implemented for completion; cannot be called.
        [[nodiscard]] Layer* GetNewEntry(const int entrySubtype) override
        {
            throw MSBFormatError("Cannot create `Layer` instances. LayerParam must be empty.");
        }
    };

}