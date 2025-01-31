#pragma once

#include <format>
#include <map>
#include <string>

#include "FirelinkER/Export.h"
#include "Entry.h"
#include "Enums.h"


namespace FirelinkER::Maps::MapStudio
{

    // MSB Model supertype
    class FIRELINKER_API Model : public Entry
    {
    public:
        using EnumType = ModelType;

        static constexpr int SubtypeEnumOffset = 8;

        [[nodiscard]] static const std::map<ModelType, std::string>& GetTypeNames()
        {
            static const std::map<ModelType, std::string> data =
            {
                {ModelType::MapPiece, "MapPiece"},
                {ModelType::Character, "Character"},
                {ModelType::Player, "Player"},
                {ModelType::Collision, "Collision"},
                {ModelType::Asset, "Asset"},
            };
            return data;
        }

        Model() = default;

        explicit Model(const std::string& name) : Entry(name) {}

        /// @brief Returns `static constexpr ModelType` defined in each subclass.
        [[nodiscard]] virtual ModelType GetType() const = 0;

        void SetInstanceCount(const int count) { instanceCount = count; }

        void Deserialize(std::ifstream& stream) override;
        void Serialize(std::ofstream& stream, int supertypeIndex, int subtypeIndex) const override;

        explicit operator std::string() const
        {
            return std::format("{}[\"{}\"]", GetTypeNames().at(GetType()), m_name);
        }

    protected:
        std::string sibPath;
        int instanceCount = 0;
        int hUnk1C = 0;
    };

    class FIRELINKER_API MapPieceModel final : public Model
    {
    public:
        static constexpr auto Type = ModelType::MapPiece;

        MapPieceModel() : Model("m999999") {}

        explicit MapPieceModel(const std::string& name) : Model(name) {}

        [[nodiscard]] ModelType GetType() const override { return Type; }
    };

    // Asset model name is split into three-digit prefix and three-digit suffix. The prefix corresponds to a 'category'
    // and has a subfolder in the game data.
    class FIRELINKER_API AssetModel final : public Model
    {
    public:
        static constexpr auto Type = ModelType::Asset;

        AssetModel() : Model("AEG999_999") {}

        explicit AssetModel(const std::string& name) : Model(name) {}

        [[nodiscard]] ModelType GetType() const override { return Type; }
    };

    class FIRELINKER_API CharacterModel final : public Model
    {
    public:
        static constexpr auto Type = ModelType::Character;

        CharacterModel() : Model("c9999") {}

        explicit CharacterModel(const std::string& name) : Model(name) {}

        [[nodiscard]] ModelType GetType() const override { return Type; }
    };

    // Used for 'c0000' (equipment-supporting 'Human' model) only.
    class FIRELINKER_API PlayerModel final : public Model
    {
    public:
        static constexpr auto Type = ModelType::Player;

        PlayerModel() : Model("c0000") {}

        explicit PlayerModel(const std::string& name) : Model(name) {}

        [[nodiscard]] ModelType GetType() const override { return Type; }
    };

    // Used for both Collision and Connect Collision parts (and the latter typically reference the matching former).
    class FIRELINKER_API CollisionModel final : public Model
    {
    public:
        static constexpr auto Type = ModelType::Collision;

        CollisionModel() : Model("h999999") {}

        explicit CollisionModel(const std::string& name) : Model(name) {}

        [[nodiscard]] ModelType GetType() const override { return Type; }
    };
}
