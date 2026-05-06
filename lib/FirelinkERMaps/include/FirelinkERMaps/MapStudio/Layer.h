#pragma once

#include "Entry.h"
#include "Enums.h"
#include "MSBFormatError.h"

namespace Firelink::EldenRing::Maps::MapStudio
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

        explicit Layer()
        : Entry(u"")
        {
            throw MSBFormatError("MSB `Layer` should never be instantiated.");
        }

        static LayerType GetType() { return LayerType::None; }

        void Deserialize(BinaryReadWrite::BufferReader& reader) override {}
        void Serialize(BinaryReadWrite::BufferWriter& writer, int supertypeIndex, int subtypeIndex) const override {}

        explicit operator std::string() const { return "Layer[]"; }
    };
} // namespace Firelink::EldenRing::Maps::MapStudio