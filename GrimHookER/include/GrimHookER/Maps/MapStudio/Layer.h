#pragma once

#include <variant>

#include "Entry.h"
#include "MSBFormatError.h"


namespace GrimHookER::Maps::MapStudio
{

    /// @brief Empty `LayerParam` for MSB.
    class Layer final : public Entry
    {
    public:
        /// @brief Placeholder enum type for `Layer`.
        enum class EnumType { None };

        explicit Layer() : Entry("")
        {
            throw MSBFormatError("MSB `Layer` should never be instantiated.");
        }

        void Deserialize(std::ifstream& stream) override {}
        void Serialize(std::ofstream& stream, int supertypeIndex, int subtypeIndex) const override {}
    };

    using LayerVariantType = std::variant<std::monostate>;
}