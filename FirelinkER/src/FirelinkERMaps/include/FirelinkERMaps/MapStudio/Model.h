#pragma once

#include "Entry.h"
#include "Enums.h"

#include <FirelinkERMaps/Export.h>

#include <format>
#include <map>
#include <string>

namespace Firelink::EldenRing::Maps::MapStudio
{
    // MSB Model supertype
    class FIRELINK_ER_MAPS_API Model : public Entry
    {
    public:
        using EnumType = ModelType;

        static constexpr int SubtypeEnumOffset = 8;

        [[nodiscard]] static const std::map<ModelType, std::string>& GetTypeNames()
        {
            static const std::map<ModelType, std::string> data = {
                {ModelType::MapPiece, "MapPiece"},
                {ModelType::Character, "Character"},
                {ModelType::Player, "Player"},
                {ModelType::Collision, "Collision"},
                {ModelType::Asset, "Asset"},
            };
            return data;
        }

        Model() = default;

        explicit Model(const std::u16string& name)
        : Entry(name)
        {
        }

        /// @brief Returns `static constexpr ModelType` defined in each subclass.
        [[nodiscard]] virtual ModelType GetType() const = 0;

        void SetInstanceCount(const int count) { m_instanceCount = count; }

        void Deserialize(Firelink::BinaryReadWrite::BufferReader& reader) override;
        void Serialize(Firelink::BinaryReadWrite::BufferWriter& writer, int supertypeIndex, int subtypeIndex) const override;

        explicit operator std::string() const
        {
            return std::format("{}[\"{}\"]", GetTypeNames().at(GetType()), GetNameUTF8());
        }

    protected:
        std::u16string m_sibPath;
        int m_instanceCount = 0;
        int hUnk1C = 0;
    };

    class FIRELINK_ER_MAPS_API MapPieceModel final : public Model
    {
    public:
        static constexpr auto Type = ModelType::MapPiece;

        MapPieceModel()
        : Model(u"m999999")
        {
        }

        explicit MapPieceModel(const std::u16string& name)
        : Model(name)
        {
        }

        [[nodiscard]] ModelType GetType() const override { return Type; }
    };

    // Asset model name is split into three-digit prefix and three-digit suffix. The prefix corresponds to a 'category'
    // and has a subfolder in the game data.
    class FIRELINK_ER_MAPS_API AssetModel final : public Model
    {
    public:
        static constexpr auto Type = ModelType::Asset;

        AssetModel()
        : Model(u"AEG999_999")
        {
        }

        explicit AssetModel(const std::u16string& name)
        : Model(name)
        {
        }

        [[nodiscard]] ModelType GetType() const override { return Type; }
    };

    class FIRELINK_ER_MAPS_API CharacterModel final : public Model
    {
    public:
        static constexpr auto Type = ModelType::Character;

        CharacterModel()
        : Model(u"c9999")
        {
        }

        explicit CharacterModel(const std::u16string& name)
        : Model(name)
        {
        }

        [[nodiscard]] ModelType GetType() const override { return Type; }
    };

    // Used for 'c0000' (equipment-supporting 'Human' model) only.
    class FIRELINK_ER_MAPS_API PlayerModel final : public Model
    {
    public:
        static constexpr auto Type = ModelType::Player;

        PlayerModel()
        : Model(u"c0000")
        {
        }

        explicit PlayerModel(const std::u16string& name)
        : Model(name)
        {
        }

        [[nodiscard]] ModelType GetType() const override { return Type; }
    };

    // Used for both Collision and Connect Collision parts (and the latter typically reference the matching former).
    class FIRELINK_ER_MAPS_API CollisionModel final : public Model
    {
    public:
        static constexpr auto Type = ModelType::Collision;

        CollisionModel()
        : Model(u"h999999")
        {
        }

        explicit CollisionModel(const std::u16string& name)
        : Model(name)
        {
        }

        [[nodiscard]] ModelType GetType() const override { return Type; }
    };
} // namespace Firelink::EldenRing::Maps::MapStudio
