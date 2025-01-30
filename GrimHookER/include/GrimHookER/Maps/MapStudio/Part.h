#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "GrimHook/Collections.h"
#include "GrimHookER/Export.h"
#include "Entry.h"
#include "EntryReference.h"

namespace GrimHookER::Maps::MapStudio
{
    using GrimHook::Vector3;
    using GrimHook::GroupBitSet;

    // Forward declaration of `Region` and `PatrolRouteEvent` for function arguments.
    class Model;
    class Region;
    class PatrolRouteEvent;
    // Forward declaration for use as an argument to `DeserializeStructs` and `SerializeStructs`.
    struct PartHeader;

    enum class PartType : uint32_t
    {
        MapPiece = 0,
        Character = 2,
        PlayerStart = 4,
        Collision = 5,
        DummyAsset = 9,
        DummyCharacter = 10,
        ConnectCollision = 11,
        Asset = 13,
        // No `Other` part type.
    };

    enum class CollisionHitFilter : uint8_t
    {
        Normal = 8,  // class default
        CameraOnly = 9,
        NPCOnly = 11,
        FallDeathCam = 13,
        Kill = 15,
        Unk16 = 16,
        Unk17 = 17,
        Unk19 = 19,
        Unk20 = 20,
        Unk22 = 22,
        Unk23 = 23,
        Unk24 = 24,
        Unk29 = 29,
    };

    inline std::map<PartType, std::string> partTypeNames =
    {
        {PartType::MapPiece, "MapPiece"},
        {PartType::Character, "Character"},
        {PartType::PlayerStart, "PlayerStart"},
        {PartType::Collision, "Collision"},
        {PartType::DummyAsset, "DummyAsset"},
        {PartType::DummyCharacter, "DummyCharacter"},
        {PartType::ConnectCollision, "ConnectCollision"},
        {PartType::Asset, "Asset"},
    };

    // NOTE: Parts use many different optional structs, varying across subtype, in addition to subtype-specific structs.
    // For ease of coding, we use a component approach to the structs. Only the subtype struct fields are defined as
    // direct members of each subtype.

    struct GRIMHOOKER_API DrawInfo1
    {
        GroupBitSet<256> displayGroups;
        GroupBitSet<256> drawGroups;
        GroupBitSet<1024> collisionMask;
        uint8_t condition1A;
        uint8_t condition1B;
        uint8_t unkC2;
        uint8_t unkC3;
        int16_t unkC4;  // always -1 in retail release
        int16_t unkC6;  // always 0 or 1 in retail release

        // Default constructor to properly initialize `GroupBitSet` fields with 'aggressive' draw values.
        DrawInfo1()
        {
            displayGroups = GroupBitSet<256>::AllOff();
            drawGroups = GroupBitSet<256>::AllOn();
            collisionMask = GroupBitSet<1024>::AllOff();
            condition1A = 0;
            condition1B = 0;
            unkC2 = 0;
            unkC3 = 0;
            unkC4 = -1;
            unkC6 = 0;
        }

        void Read(std::ifstream& stream);
        void Write(std::ofstream& stream) const;
    };

    struct GRIMHOOKER_API DrawInfo2
    {
        int condition2;
        GroupBitSet<256> displayGroups2;
        int16_t unk24;
        int16_t unk26;

        DrawInfo2()
        {
            condition2 = 0;
            displayGroups2 = GroupBitSet<256>::AllOff();
            unk24 = 0;
            unk26 = 0;
        }

        void Read(std::ifstream& stream);
        void Write(std::ofstream& stream) const;
    };

    // Used by all Parts except Player Starts and Connect Collisions.
    struct GRIMHOOKER_API GParam
    {
        int lightSetId = -1;
        int fogId = -1;
        int lightScatteringId = 0;
        int environmentMapId = 0;

        void Read(std::ifstream& stream);
        void Write(std::ofstream& stream) const;
    };

    // Only used by Collisions.
    struct GRIMHOOKER_API SceneGParam
    {
        float transitionTime = -1.0f;
        int8_t unk18 = -1;
        int8_t unk19 = -1;
        int8_t unk1A = -1;
        int8_t unk1B = -1;
        int8_t unk1C = -1;
        int8_t unk1D = -1;
        int8_t unk20 = -1;
        int8_t unk21 = -1;

        void Read(std::ifstream& stream);
        void Write(std::ofstream& stream) const;
    };

    // Only used by Map Pieces and Assets.
    struct GRIMHOOKER_API GrassConfig
    {
        int unk00 = 0;
        int unk04 = 0;
        int unk08 = 0;
        int unk0C = 0;
        int unk10 = 0;
        int unk14 = 0;
        int unk18 = -1;

        void Read(std::ifstream& stream);
        void Write(std::ofstream& stream) const;
    };

    struct GRIMHOOKER_API UnkPartStruct8
    {
        int unk00 = 0;  // 0 or 1

        void Read(std::ifstream& stream);
        void Write(std::ofstream& stream) const;
    };

    // Only used by Map Pieces.
    struct GRIMHOOKER_API UnkPartStruct9
    {
        int unk00 = 0;

        void Read(std::ifstream& stream);
        void Write(std::ofstream& stream) const;
    };

    struct GRIMHOOKER_API TileLoadConfig
    {
        int mapId = -1;
        int unk04 = 0;
        int unk0C = -1;
        int unk10 = 0;  // 0 or 1
        int unk14 = -1;

        void Read(std::ifstream& stream);
        void Write(std::ofstream& stream) const;
    };

    // Only used by Map Pieces, Collisions, and Connect Collisions.
    struct GRIMHOOKER_API UnkPartStruct11
    {
        int unk00 = 0;
        int unk04 = 0;

        void Read(std::ifstream& stream);
        void Write(std::ofstream& stream) const;
    };

    class GRIMHOOKER_API Part : public EntityEntry
    {
    public:
        using EnumType = PartType;

        explicit Part(const std::string& name) : EntityEntry(name) {}

        [[nodiscard]] virtual PartType GetType() const = 0;

        // Header data:
        [[nodiscard]] int GetModelInstanceId() const { return modelInstanceId; }
        void SetModelInstanceId(const int modelInstanceId) { this->modelInstanceId = modelInstanceId; }
        [[nodiscard]] Model* GetModel() const { return model.Get(); }
        void SetModel(Model* const model) { this->model.Set(model); }
        void SetModel(const std::unique_ptr<Model>& model) { this->model.Set(model); }
        [[nodiscard]] std::string GetSibPath() const { return sibPath; }
        void SetSibPath(const std::string& sibPath) { this->sibPath = sibPath; }
        [[nodiscard]] Vector3 GetTranslate() const { return translate; }
        void SetTranslate(const Vector3& translate) { this->translate = translate; }
        [[nodiscard]] Vector3 GetRotate() const { return rotate; }
        void SetRotate(const Vector3& rotate) { this->rotate = rotate; }
        [[nodiscard]] Vector3 GetScale() const { return scale; }
        void SetScale(const Vector3& scale) { this->scale = scale; }
        [[nodiscard]] int GetUnk44() const { return unk44; }
        void SetUnk44(const int unk44) { this->unk44 = unk44; }
        [[nodiscard]] int GetEventLayer() const { return eventLayer; }
        void SetEventLayer(const int eventLayer) { this->eventLayer = eventLayer; }

        // Part data:
        [[nodiscard]] uint8_t GetUnk04() const { return unk04; }
        void SetUnk04(const uint8_t unk04) { this->unk04 = unk04; }
        [[nodiscard]] int8_t GetLodId() const { return lodId; }
        void SetLodId(const int8_t lodId) { this->lodId = lodId; }
        [[nodiscard]] uint8_t GetUnk09() const { return unk09; }
        void SetUnk09(const uint8_t unk09) { this->unk09 = unk09; }
        [[nodiscard]] int8_t GetIsPointLightShadowSource() const { return isPointLightShadowSource; }

        void SetIsPointLightShadowSource(const int8_t isPointLightShadowSource)
        {
            this->isPointLightShadowSource = isPointLightShadowSource;
        }

        [[nodiscard]] uint8_t GetUnk0B() const { return unk0B; }
        void SetUnk0B(const uint8_t unk0B) { this->unk0B = unk0B; }

        [[nodiscard]] bool isIsShadowSource() const { return isShadowSource; }
        void SetIsShadowSource(const bool isShadowSource) { this->isShadowSource = isShadowSource; }

        [[nodiscard]] uint8_t GetIsStaticShadowSource() const { return isStaticShadowSource; }
        void SetIsStaticShadowSource(const uint8_t isStaticShadowSource)
        {
            this->isStaticShadowSource = isStaticShadowSource;
        }

        [[nodiscard]] uint8_t GetIsCascade3ShadowSource() const { return isCascade3ShadowSource; }
        void SetIsCascade3ShadowSource(const uint8_t isCascade3ShadowSource)
        {
            this->isCascade3ShadowSource = isCascade3ShadowSource;
        }

        [[nodiscard]] uint8_t GetUnk0F() const { return unk0F; }
        void SetUnk0F(const uint8_t unk0F) { this->unk0F = unk0F; }
        [[nodiscard]] uint8_t GetUnk10() const { return unk10; }
        void SetUnk10(const uint8_t unk10) { this->unk10 = unk10; }
        [[nodiscard]] bool isIsShadowDestination() const { return isShadowDestination; }
        void SetIsShadowDestination(const bool isShadowDestination) { this->isShadowDestination = isShadowDestination; }
        [[nodiscard]] bool isIsShadowOnly() const { return isShadowOnly; }
        void SetIsShadowOnly(const bool isShadowOnly) { this->isShadowOnly = isShadowOnly; }
        [[nodiscard]] bool isDrawByReflectCam() const { return drawByReflectCam; }
        void SetDrawByReflectCam(const bool drawByReflectCam) { this->drawByReflectCam = drawByReflectCam; }
        [[nodiscard]] bool isDrawOnlyReflectCam() const { return drawOnlyReflectCam; }
        void SetDrawOnlyReflectCam(const bool drawOnlyReflectCam) { this->drawOnlyReflectCam = drawOnlyReflectCam; }
        [[nodiscard]] uint8_t GetEnableOnAboveShadow() const { return enableOnAboveShadow; }

        void SetEnableOnAboveShadow(const uint8_t enableOnAboveShadow)
        {
            this->enableOnAboveShadow = enableOnAboveShadow;
        }

        [[nodiscard]] bool isDisablePointLightEffect() const { return disablePointLightEffect; }

        void SetDisablePointLightEffect(const bool disablePointLightEffect)
        {
            this->disablePointLightEffect = disablePointLightEffect;
        }

        [[nodiscard]] uint8_t GetUnk17() const { return unk17; }
        void SetUnk17(const uint8_t unk17) { this->unk17 = unk17; }
        [[nodiscard]] int32_t GetUnk18() const { return unk18; }
        void SetUnk18(const int32_t unk18) { this->unk18 = unk18; }
        [[nodiscard]] std::array<uint32_t, 8> GetEntityGroupIds() const { return entityGroupIds; }
        void SetEntityGroupIds(const std::array<uint32_t, 8>& entityGroupIds) { this->entityGroupIds = entityGroupIds; }
        [[nodiscard]] int16_t GetUnk3C() const { return unk3C; }
        void SetUnk3C(const int16_t unk3C) { this->unk3C = unk3C; }
        [[nodiscard]] int16_t GetUnk3E() const { return unk3E; }
        void SetUnk3E(const int16_t unk3E) { this->unk3E = unk3E; }

        void Deserialize(std::ifstream& stream) override;
        void Serialize(std::ofstream& stream, int supertypeIndex, int subtypeIndex) const override;

        virtual void DeserializeEntryReferences(const std::vector<Model*>& models, const std::vector<Part*>& parts, const std::vector<Region*>& regions);
        virtual void SerializeEntryIndices(const std::vector<Model*>& models, const std::vector<Part*>& parts, const std::vector<Region*>& regions);

    protected:
        // From header:
        int modelInstanceId = 0;  // still functions as a unique model instance, but starts at an offset like 9000
        EntryReference<Model> model;
        int modelIndex = -1;
        std::string sibPath;
        Vector3 translate{};
        Vector3 rotate{};
        Vector3 scale{1.0f, 1.0f, 1.0f};
        int unk44 = 0;
        int eventLayer = 0;

        // From Part data:
        // `entityId` defined in parent class.
        uint8_t unk04 = 0;
        int8_t lodId = 0;
        uint8_t unk09 = 0;
        int8_t isPointLightShadowSource = 0;  // always 0 or -1?
        uint8_t unk0B = 0;
        bool isShadowSource = false;
        uint8_t isStaticShadowSource = 0;    // TODO: bool?
        uint8_t isCascade3ShadowSource = 0;  // TODO: bool?
        uint8_t unk0F = 0;
        uint8_t unk10 = 0;
        bool isShadowDestination = false;
        bool isShadowOnly = false;  // always 0?
        bool drawByReflectCam = false;
        bool drawOnlyReflectCam = false;
        uint8_t enableOnAboveShadow = 0;
        bool disablePointLightEffect = false;
        uint8_t unk17 = 0;
        int32_t unk18 = 0;
        std::array<uint32_t, 8> entityGroupIds{};
        int16_t unk3C = 0;
        int16_t unk3E = 0;

        // Subtypes may define their own fields and some of the above structs.

        // Called from inside `DeserializeStructs()` override.
        void ReadSupertypeData(std::ifstream& stream);
        // Called from inside `SerializeStructs()` override.
        void WriteSupertypeData(std::ofstream& stream) const;

        // NOTE: All subclasses have subtype data, even if just padding (Map Pieces, Dummy Assets).
        virtual bool DeserializeSubtypeData(std::ifstream& stream) = 0;
        virtual bool SerializeSubtypeData(std::ofstream& stream, int supertypeIndex, int subtypeIndex) const = 0;

        // Supertype and subtype data structs are also read in here.
        virtual void DeserializeStructs(
            std::ifstream& stream,
            const PartHeader& header,
            const std::streampos& entryStart) = 0;
        virtual void SerializeStructs(
            std::ofstream& stream, PartHeader& header,
            const std::streampos& entryStart,
            const int& supertypeIndex,
            const int& subtypeIndex) const = 0;

        void OnReferencedEntryDestroy(const Entry* destroyedEntry) override;
    };

    class GRIMHOOKER_API MapPiecePart final : public Part
    {
    public:
        static constexpr auto Type = PartType::MapPiece;

        explicit MapPiecePart(const std::string& name) : Part(name) {}

        [[nodiscard]] PartType GetType() const override { return Type; }

        DrawInfo1 drawInfo1{};
        GParam gparam{};
        GrassConfig grassConfig{};
        UnkPartStruct8 unkStruct8{};
        UnkPartStruct9 unkStruct9{};
        TileLoadConfig tileLoadConfig{};
        UnkPartStruct11 unkStruct11{};

    private:
        // No real subtype data.

        void DeserializeStructs(std::ifstream& stream, const PartHeader& header, const std::streampos& entryStart) override;
        void SerializeStructs(std::ofstream& stream, PartHeader& header, const std::streampos& entryStart,
            const int& supertypeIndex, const int& subtypeIndex) const override;

        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream, int supertypeIndex, int subtypeIndex) const override;
    };

    class GRIMHOOKER_API CharacterPart : public Part
    {
    public:
        static constexpr auto Type = PartType::Character;

        explicit CharacterPart(const std::string& name) : Part(name) {}

        [[nodiscard]] PartType GetType() const override { return Type; }

        [[nodiscard]] int GetAiId() const { return aiId; }
        void SetAiId(const int aiId) { this->aiId = aiId; }
        [[nodiscard]] int GetCharacterId() const { return characterId; }
        void SetCharacterId(const int characterId) { this->characterId = characterId; }
        [[nodiscard]] int GetTalkId() const { return talkId; }
        void SetTalkId(const int talkId) { this->talkId = talkId; }
        [[nodiscard]] bool IsSUnk15() const { return sUnk15; }
        void SetSUnk15(const bool sUnk15) { this->sUnk15 = sUnk15; }
        [[nodiscard]] int16_t GetPlatoonId() const { return platoonId; }
        void SetPlatoonId(const int16_t platoonId) { this->platoonId = platoonId; }
        [[nodiscard]] int GetPlayerId() const { return playerId; }
        void SetPlayerId(const int playerId) { this->playerId = playerId; }

        [[nodiscard]] Part* GetDrawParent() const { return drawParent.Get(); }
        void SetDrawParent(Part* const drawParent) { this->drawParent.Set(drawParent); }
        void SetDrawParent(const std::unique_ptr<Part>& drawParent) { this->drawParent.Set(drawParent); }

        [[nodiscard]] PatrolRouteEvent* GetPatrolRouteEvent() const { return patrolRouteEvent.Get(); }
        void SetPatrolRouteEvent(PatrolRouteEvent* const patrolRouteEvent) { this->patrolRouteEvent.Set(patrolRouteEvent); }
        void SetPatrolRouteEvent(const std::unique_ptr<PatrolRouteEvent>& patrolRouteEvent) { this->patrolRouteEvent.Set(patrolRouteEvent); }

        [[nodiscard]] int GetSUnk24() const { return sUnk24; }
        void SetSUnk24(const int sUnk24) { this->sUnk24 = sUnk24; }

        [[nodiscard]] int GetSUnk28() const { return sUnk28; }
        void SetSUnk28(const int sUnk28) { this->sUnk28 = sUnk28; }

        [[nodiscard]] int GetActivateConditionParamId() const { return activateConditionParamId; }
        void SetActivateConditionParamId(const int activateConditionParamId) { this->activateConditionParamId = activateConditionParamId; }

        [[nodiscard]] int GetSUnk34() const { return sUnk34; }
        void SetSUnk34(const int sUnk34) { this->sUnk34 = sUnk34; }

        [[nodiscard]] int GetBackAwayEventAnimationId() const { return backAwayEventAnimationId; }
        void SetBackAwayEventAnimationId(const int backAwayEventAnimationId) { this->backAwayEventAnimationId = backAwayEventAnimationId; }

        [[nodiscard]] int GetSUnk3C() const { return sUnk3C; }
        void SetSUnk3C(const int sUnk3C) { this->sUnk3C = sUnk3C; }

        [[nodiscard]] std::array<int, 4> GetSpecialEffectSetParamIds() const { return specialEffectSetParamIds; }
        void SetSpecialEffectSetParamIds(const std::array<int, 4>& specialEffectSetParamIds) { this->specialEffectSetParamIds = specialEffectSetParamIds; }

        [[nodiscard]] float GetSUnk84() const { return sUnk84; }
        void SetSUnk84(const float sUnk84) { this->sUnk84 = sUnk84; }

        DrawInfo1 drawInfo1{};
        GParam gparam{};
        UnkPartStruct8 unkStruct8{};
        TileLoadConfig tileLoadConfig{};

        void DeserializeEntryReferences(const std::vector<Model*>& models, const std::vector<Part*>& parts, const std::vector<Region*>& regions) override;
        void SerializeEntryIndices(const std::vector<Model*>& models, const std::vector<Part*>& parts, const std::vector<Region*>& regions) override;

        // Extra functions called to refresh subtype-specific `patrolRouteEventIndex/patrolRouteEvent` fields.
        void DeserializePatrolRouteEventReference(const std::vector<PatrolRouteEvent*>& patrolRouteEvents);
        void SerializePatrolRouteEventIndex(const std::vector<PatrolRouteEvent*>& patrolRouteEvents);

    protected:
        int aiId = 0;
        int characterId = 0;
        int talkId = 0;
        bool sUnk15 = false;
        int16_t platoonId = 0;
        int playerId = 0;
        EntryReference<Part> drawParent;
        int drawParentIndex = -1;
        EntryReference<PatrolRouteEvent> patrolRouteEvent;
        int16_t patrolRouteEventIndex = -1;  // NOTE: `PatrolRouteEvent` subtype index, NOT `Event` supertype index
        int sUnk24 = -1;
        int sUnk28 = 0;
        int activateConditionParamId = 0;
        int sUnk34 = 0;
        int backAwayEventAnimationId = -1;
        int sUnk3C = -1;
        std::array<int, 4> specialEffectSetParamIds{};
        float sUnk84 = 1.0f;

        void DeserializeStructs(std::ifstream& stream, const PartHeader& header, const std::streampos& entryStart) override;
        void SerializeStructs(std::ofstream& stream, PartHeader& header, const std::streampos& entryStart,
            const int& supertypeIndex, const int& subtypeIndex) const override;

        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream, int supertypeIndex, int subtypeIndex) const override;
    };

    class GRIMHOOKER_API PlayerStartPart final : public Part
    {
    protected:
        int sUnk00 = 0;

        void DeserializeStructs(std::ifstream& stream, const PartHeader& header, const std::streampos& entryStart) override;
        void SerializeStructs(std::ofstream& stream, PartHeader& header, const std::streampos& entryStart,
            const int& supertypeIndex, const int& subtypeIndex) const override;

        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream, int supertypeIndex, int subtypeIndex) const override;

    public:
        static constexpr auto Type = PartType::PlayerStart;

        explicit PlayerStartPart(const std::string& name) : Part(name) {}

        [[nodiscard]] PartType GetType() const override { return Type; }

        [[nodiscard]] int GetSUnk00() const { return sUnk00; }
        void SetSUnk00(const int sUnk00) { this->sUnk00 = sUnk00; }

        DrawInfo1 drawInfo1{};
        UnkPartStruct8 unkStruct8{};
        TileLoadConfig tileLoadConfig{};
    };

    class GRIMHOOKER_API CollisionPart final : public Part
    {
    protected:
        // NOTE: We don't type this field as `CollisionHitFilter` in case the enum missed some valid values.
        uint8_t hitFilterId = static_cast<int8_t>(CollisionHitFilter::Normal);
        uint8_t sUnk01 = 255;
        uint8_t sUnk02 = 255;
        uint8_t sUnk03 = 0;
        float sUnk04 = 0.0f;
        float sUnk14 = -1.0f;
        int sUnk18 = -10000;
        int sUnk1C = -1;
        int playRegionId = 0;
        int16_t sUnk24 = -1;
        int16_t sUnk26 = 0;  // always 0 or 1
        int sUnk30 = 0;
        uint8_t sUnk34 = 0;
        uint8_t sUnk35 = 255;
        bool disableTorrent = false;
        int16_t sUnk3C = -1;
        int16_t sUnk3E = -1;
        float sUnk40 = 0.0f;
        uint32_t enableFastTravelFlagId = 0;
        int16_t sUnk4C = 0;  // always 0 or 1
        int16_t sUnk4E = -1;

        void DeserializeStructs(std::ifstream& stream, const PartHeader& header, const std::streampos& entryStart) override;
        void SerializeStructs(std::ofstream& stream, PartHeader& header, const std::streampos& entryStart,
            const int& supertypeIndex, const int& subtypeIndex) const override;

        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream, int supertypeIndex, int subtypeIndex) const override;

    public:
        static constexpr auto Type = PartType::Collision;

        explicit CollisionPart(const std::string& name) : Part(name) {}

        [[nodiscard]] PartType GetType() const override { return Type; }

        [[nodiscard]] uint8_t GetHitFilterId() const { return hitFilterId; }
        void SetHitFilterId(const uint8_t hitFilterId) { this->hitFilterId = hitFilterId; }
        [[nodiscard]] uint8_t GetSUnk01() const { return sUnk01; }
        void SetSUnk01(const uint8_t sUnk01) { this->sUnk01 = sUnk01; }
        [[nodiscard]] uint8_t GetSUnk02() const { return sUnk02; }
        void SetSUnk02(const uint8_t sUnk02) { this->sUnk02 = sUnk02; }
        [[nodiscard]] uint8_t GetSUnk03() const { return sUnk03; }
        void SetSUnk03(const uint8_t sUnk03) { this->sUnk03 = sUnk03; }
        [[nodiscard]] float GetSUnk04() const { return sUnk04; }
        void SetSUnk04(const float sUnk04) { this->sUnk04 = sUnk04; }
        [[nodiscard]] float GetSUnk14() const { return sUnk14; }
        void SetSUnk14(const float sUnk14) { this->sUnk14 = sUnk14; }
        [[nodiscard]] int GetSUnk18() const { return sUnk18; }
        void SetSUnk18(const int sUnk18) { this->sUnk18 = sUnk18; }
        [[nodiscard]] int GetSUnk1C() const { return sUnk1C; }
        void SetSUnk1C(const int sUnk1C) { this->sUnk1C = sUnk1C; }
        [[nodiscard]] int GetPlayRegionId() const { return playRegionId; }
        void SetPlayRegionId(const int playRegionId) { this->playRegionId = playRegionId; }
        [[nodiscard]] int16_t GetSUnk24() const { return sUnk24; }
        void SetSUnk24(const int16_t sUnk24) { this->sUnk24 = sUnk24; }
        [[nodiscard]] int16_t GetSUnk26() const { return sUnk26; }
        void SetSUnk26(const int16_t sUnk26) { this->sUnk26 = sUnk26; }
        [[nodiscard]] int GetSUnk30() const { return sUnk30; }
        void SetSUnk30(const int sUnk30) { this->sUnk30 = sUnk30; }
        [[nodiscard]] uint8_t GetSUnk34() const { return sUnk34; }
        void SetSUnk34(const uint8_t sUnk34) { this->sUnk34 = sUnk34; }
        [[nodiscard]] uint8_t GetSUnk35() const { return sUnk35; }
        void SetSUnk35(const uint8_t sUnk35) { this->sUnk35 = sUnk35; }
        [[nodiscard]] bool isDisableTorrent() const { return disableTorrent; }
        void SetDisableTorrent(const bool disableTorrent) { this->disableTorrent = disableTorrent; }
        [[nodiscard]] int16_t GetSUnk3C() const { return sUnk3C; }
        void SetSUnk3C(const int16_t sUnk3C) { this->sUnk3C = sUnk3C; }
        [[nodiscard]] int16_t GetSUnk3E() const { return sUnk3E; }
        void SetSUnk3E(const int16_t sUnk3E) { this->sUnk3E = sUnk3E; }
        [[nodiscard]] float GetSUnk40() const { return sUnk40; }
        void SetSUnk40(const float sUnk40) { this->sUnk40 = sUnk40; }
        [[nodiscard]] uint32_t GetEnableFastTravelFlagId() const { return enableFastTravelFlagId; }

        void SetEnableFastTravelFlagId(const uint32_t enableFastTravelFlagId)
        {
            this->enableFastTravelFlagId = enableFastTravelFlagId;
        }

        [[nodiscard]] int16_t GetSUnk4C() const { return sUnk4C; }
        void SetSUnk4C(const int16_t sUnk4C) { this->sUnk4C = sUnk4C; }
        [[nodiscard]] int16_t GetSUnk4E() const { return sUnk4E; }
        void SetSUnk4E(const int16_t sUnk4E) { this->sUnk4E = sUnk4E; }

        DrawInfo1 drawInfo1{};
        DrawInfo2 drawInfo2{};
        GParam gparam{};
        SceneGParam sceneGparam{};
        UnkPartStruct8 unkStruct8{};
        TileLoadConfig tileLoadConfig{};
        UnkPartStruct11 unkStruct11{};
    };

    // Unlike `DummyObject` in earlier games, almost all data has been nuked here, which prevents this from being a
    // convenient way to temporarily disable an Asset. Probably only the bare minimum for cutscenes (i.e. draw
    // settings) is left.
    class GRIMHOOKER_API DummyAssetPart final : public Part
    {
        int32_t sUnk18 = -1;

    public:
        static constexpr auto Type = PartType::DummyAsset;

        explicit DummyAssetPart(const std::string& name) : Part(name) {}

        [[nodiscard]] PartType GetType() const override { return Type; }

        [[nodiscard]] int32_t GetSUnk18() const { return sUnk18; }
        void SetSUnk18(const int32_t sUnk18) { this->sUnk18 = sUnk18; }

        DrawInfo1 drawInfo1{};
        GParam gparam{};
        UnkPartStruct8 unkStruct8{};
        TileLoadConfig tileLoadConfig{};

        void DeserializeStructs(std::ifstream& stream, const PartHeader& header, const std::streampos& entryStart) override;
        void SerializeStructs(std::ofstream& stream, PartHeader& header, const std::streampos& entryStart, const int& supertypeIndex, const int
                          & subtypeIndex) const override;

        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream, int supertypeIndex, int subtypeIndex) const override;
    };

    // May still be used in cutscenes.
    // Identical to `CharacterPart` (unlike nuked Assets) and inherits from that class for easy type conversion.
    class GRIMHOOKER_API DummyCharacterPart final : public CharacterPart
    {
    public:
        static constexpr auto Type = PartType::DummyCharacter;

        explicit DummyCharacterPart(const std::string& name) : CharacterPart(name) {}

        [[nodiscard]] PartType GetType() const override { return Type; }
    };

    class GRIMHOOKER_API ConnectCollisionPart final : public Part
    {
    protected:
        EntryReference<CollisionPart> collision;  // NOTE: `Collision` subtype
        int collisionIndex = -1;  // NOTE: `Collision` subtype index, NOT `Part` supertype index
        std::array<int8_t, 4> connectedMapId{0, 0, 0, 0};
        uint8_t sUnk08 = 0;
        bool sUnk09 = false;
        uint8_t sUnk0A = 0;
        bool sUnk0B = false;

        void DeserializeStructs(std::ifstream& stream, const PartHeader& header, const std::streampos& entryStart) override;
        void SerializeStructs(std::ofstream& stream, PartHeader& header, const std::streampos& entryStart,
            const int& supertypeIndex, const int& subtypeIndex) const override;

        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream, int supertypeIndex, int subtypeIndex) const override;

    public:
        static constexpr auto Type = PartType::ConnectCollision;

        explicit ConnectCollisionPart(const std::string& name) : Part(name) {}

        [[nodiscard]] PartType GetType() const override { return Type; }

        [[nodiscard]] CollisionPart* GetCollision() const { return collision.Get(); }
        void SetCollision(CollisionPart* const collision) { this->collision.Set(collision); }
        void SetCollision(const std::unique_ptr<CollisionPart>& collision) { this->collision.Set(collision); }

        [[nodiscard]] std::array<int8_t, 4> GetConnectedMapId() const { return connectedMapId; }
        void SetConnectedMapId(const std::array<int8_t, 4>& connectedMapId) { this->connectedMapId = connectedMapId; }

        [[nodiscard]] uint8_t GetSUnk08() const { return sUnk08; }
        void SetSUnk08(const uint8_t sUnk08) { this->sUnk08 = sUnk08; }

        [[nodiscard]] bool IsSUnk09() const { return sUnk09; }
        void SetSUnk09(const bool sUnk09) { this->sUnk09 = sUnk09; }

        [[nodiscard]] uint8_t GetSUnk0A() const { return sUnk0A; }
        void SetSUnk0A(const uint8_t sUnk0A) { this->sUnk0A = sUnk0A; }

        [[nodiscard]] bool IsSUnk0B() const { return sUnk0B; }
        void SetSUnk0B(const bool sUnk0B) { this->sUnk0B = sUnk0B; }

        DrawInfo1 drawInfo1{};
        DrawInfo2 drawInfo2{};
        UnkPartStruct8 unkStruct8{};
        TileLoadConfig tileLoadConfig{};
        UnkPartStruct11 unkStruct11{};

        // Additional methods to refresh subtype-local `collisionIndex/collision`.
        void DeserializeCollisionReference(const std::vector<CollisionPart*>& collisions);
        void SerializeCollisionIndex(const std::vector<CollisionPart*>& collisions);
    };

    // Four additional Asset sub-structs:

    struct GRIMHOOKER_API ExtraAssetData1
    {
        int16_t unk00 = 0;
        bool unk04 = false;
        // Disables ability to summon/ride Torrent, but only when asset isn't referencing a collision
        // with `disableTorrent` enabled.
        bool disableTorrentAssetOnly =false;
        int unk1C = -1;
        int16_t unk24 = -1;
        int16_t unk26 = -1;
        int unk28 = -1;
        int unk2C = -1;

        void Read(std::ifstream& stream);
        void Write(std::ofstream& stream) const;
    };

    struct GRIMHOOKER_API ExtraAssetData2
    {
        int unk00 = 0;
        int unk04 = -1;
        float unk14 = -1.0f;
        uint8_t unk1C = 255;  // suss as map ID?
        uint8_t unk1D = 255;
        uint8_t unk1E = 255;
        uint8_t unk1F = 255;

        void Read(std::ifstream& stream);
        void Write(std::ofstream& stream) const;
    };

    struct GRIMHOOKER_API ExtraAssetData3
    {
        int unk00 = 0;
        float unk04 = 0.0f;
        uint8_t unk09 = 255;
        uint8_t unk0A = 0;
        uint8_t unk0B = 255;
        int16_t unk0C = -1;
        int16_t unk0E = 0;
        float unk10 = -1.0f;
        std::array<uint8_t, 4> disableWhenMapLoadedWithId = {255, 255, 255, 255};
        int unk18 = -1;
        int unk1C = -1;
        int unk20 = -1;
        uint8_t unk24 = 255;
        bool unk25 = false;

        void Read(std::ifstream& stream);
        void Write(std::ofstream& stream) const;
    };

    struct GRIMHOOKER_API ExtraAssetData4
    {
        bool unk00 = false;
        uint8_t unk01 = 255;
        uint8_t unk02 = 255;
        bool unk03 = false;

        void Read(std::ifstream& stream);
        void Write(std::ofstream& stream) const;
    };

    class GRIMHOOKER_API AssetPart final : public Part
    {
    public:
        static constexpr auto Type = PartType::Asset;

        explicit AssetPart(const std::string& name) : Part(name)
        {
            drawParentPartsIndices.fill(-1);
        }

        [[nodiscard]] PartType GetType() const override { return Type; }

        // Uses all structs except `SceneGParam`.
        DrawInfo1 drawInfo1{};
        DrawInfo2 drawInfo2{};
        GParam gparam{};
        GrassConfig grassConfig{};
        UnkPartStruct8 unkStruct8{};
        UnkPartStruct9 unkStruct9{};
        TileLoadConfig tileLoadConfig{};
        UnkPartStruct11 unkStruct11{};

        ExtraAssetData1 extraData1{};
        ExtraAssetData2 extraData2{};
        ExtraAssetData3 extraData3{};
        ExtraAssetData4 extraData4{};

        [[nodiscard]] int16_t GetSUnk02() const { return sUnk02; }
        void SetSUnk02(const int16_t sUnk02) { this->sUnk02 = sUnk02; }

        [[nodiscard]] uint8_t GetSUnk10() const { return sUnk10; }
        void SetSUnk10(const uint8_t sUnk10) { this->sUnk10 = sUnk10; }

        [[nodiscard]] bool IsSUnk11() const { return sUnk11; }
        void SetSUnk11(const bool sUnk11) { this->sUnk11 = sUnk11; }

        [[nodiscard]] uint8_t GetSUnk12() const { return sUnk12; }
        void SetSUnk12(const uint8_t sUnk12) { this->sUnk12 = sUnk12; }

        [[nodiscard]] int16_t GetSfxParamRelativeId() const { return sfxParamRelativeId; }
        void SetSfxParamRelativeId(const int16_t sfxParamRelativeId) { this->sfxParamRelativeId = sfxParamRelativeId; }

        [[nodiscard]] int16_t GetSUnk1E() const { return sUnk1E; }
        void SetSUnk1E(const int16_t sUnk1E) { this->sUnk1E = sUnk1E; }

        [[nodiscard]] int GetSUnk24() const { return sUnk24; }
        void SetSUnk24(const int sUnk24) { this->sUnk24 = sUnk24; }

        [[nodiscard]] int GetSUnk28() const { return sUnk28; }
        void SetSUnk28(const int sUnk28) { this->sUnk28 = sUnk28; }

        [[nodiscard]] int GetSUnk30() const { return sUnk30; }
        void SetSUnk30(const int sUnk30) { this->sUnk30 = sUnk30; }

        [[nodiscard]] int GetSUnk34() const { return sUnk34; }
        void SetSUnk34(const int sUnk34) { this->sUnk34 = sUnk34; }

        [[nodiscard]] Part* GetDrawParentPartAtIndex(const int index) const { return drawParentParts[index].Get(); }
        void SetDrawParentPartAtIndex(const int index, Part* const drawParentPart) { drawParentParts[index].Set(drawParentPart); }
        void SetDrawParentPartAtIndex(const int index, const std::unique_ptr<Part>& drawParentPart) { drawParentParts[index].Set(drawParentPart); }

        [[nodiscard]] bool IsSUnk50() const { return sUnk50; }
        void SetSUnk50(const bool sUnk50) { this->sUnk50 = sUnk50; }

        [[nodiscard]] uint8_t GetSUnk51() const { return sUnk51; }
        void SetSUnk51(const uint8_t sUnk51) { this->sUnk51 = sUnk51; }

        [[nodiscard]] uint8_t GetSUnk53() const { return sUnk53; }
        void SetSUnk53(const uint8_t sUnk53) { this->sUnk53 = sUnk53; }

        [[nodiscard]] int GetSUnk54() const { return sUnk54; }
        void SetSUnk54(const int sUnk54) { this->sUnk54 = sUnk54; }

        [[nodiscard]] int GetSUnk58() const { return sUnk58; }
        void SetSUnk58(const int sUnk58) { this->sUnk58 = sUnk58; }

        [[nodiscard]] int GetSUnk5C() const { return sUnk5C; }
        void SetSUnk5C(const int sUnk5C) { this->sUnk5C = sUnk5C; }

        [[nodiscard]] int GetSUnk60() const { return sUnk60; }
        void SetSUnk60(const int sUnk60) { this->sUnk60 = sUnk60; }

        [[nodiscard]] int GetSUnk64() const { return sUnk64; }
        void SetSUnk64(const int sUnk64) { this->sUnk64 = sUnk64; }

        void DeserializeEntryReferences(const std::vector<Model*>& models, const std::vector<Part*>& parts, const std::vector<Region*>& regions) override;
        void SerializeEntryIndices(const std::vector<Model*>& models, const std::vector<Part*>& parts, const std::vector<Region*>& regions) override;

    protected:
        int16_t sUnk02 = 0;
        uint8_t sUnk10 = 0;
        bool sUnk11 = false;
        uint8_t sUnk12 = 255;
        int16_t sfxParamRelativeId = -1;
        int16_t sUnk1E = -1;
        int sUnk24 = -1;
        int sUnk28 = 0;
        int sUnk30 = -1;
        int sUnk34 = -1;
        // Definitely draw parents, but unclear how index matters. Sometimes the same Part repeats.
        std::array<EntryReference<Part>, 6> drawParentParts{};
        std::array<int, 6> drawParentPartsIndices{};
        bool sUnk50 = false;
        uint8_t sUnk51 = 0;
        uint8_t sUnk53 = 0;
        int sUnk54 = -1;
        int sUnk58 = -1;
        int sUnk5C = -1;
        int sUnk60 = -1;
        int sUnk64 = -1;

        void DeserializeStructs(std::ifstream& stream, const PartHeader& header, const std::streampos& entryStart) override;
        void SerializeStructs(std::ofstream& stream, PartHeader& header, const std::streampos& entryStart,
            const int& supertypeIndex, const int& subtypeIndex) const override;

        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream, int supertypeIndex, int subtypeIndex) const override;
    };

    using PartVariantType = std::variant<
        std::vector<std::unique_ptr<MapPiecePart>>,
        std::vector<std::unique_ptr<CharacterPart>>,
        std::vector<std::unique_ptr<PlayerStartPart>>,
        std::vector<std::unique_ptr<CollisionPart>>,
        std::vector<std::unique_ptr<DummyAssetPart>>,
        std::vector<std::unique_ptr<DummyCharacterPart>>,
        std::vector<std::unique_ptr<ConnectCollisionPart>>,
        std::vector<std::unique_ptr<AssetPart>>
    >;
}