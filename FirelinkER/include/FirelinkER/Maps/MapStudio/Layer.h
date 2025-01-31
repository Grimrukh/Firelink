#pragma once

#include "Entry.h"
#include "Enums.h"
#include "MSBFormatError.h"


namespace FirelinkER::Maps::MapStudio
{

    /// @brief Empty `LayerParam` for MSB.
    class Layer final : public Entry
    {
    public:
        using EnumType = LayerType;

        static constexpr int SubtypeEnumOffset = 0;

        static const std::map<LayerType, std::string>& GetTypeNames()
        {
            static const std::map<LayerType, std::string> data = {};
            return data;
        }

        explicit Layer() : Entry("")
        {
            throw MSBFormatError("MSB `Layer` should never be instantiated.");
        }

        static LayerType GetType() { return LayerType::None; }

        void Deserialize(std::ifstream& stream) override {}
        void Serialize(std::ofstream& stream, int supertypeIndex, int subtypeIndex) const override {}
    };
}