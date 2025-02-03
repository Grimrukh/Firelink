#pragma once

#include <array>
#include <cstdint>
#include <format>
#include <map>
#include <string>

#include "Firelink/Collections.h"
#include "FirelinkER/Export.h"
#include "Entry.h"
#include "EntryReference.h"
#include "Enums.h"
#include "Shape.h"

namespace FirelinkER::Maps::MapStudio
{
    class Part;

    using Firelink::Vector3;

    class FIRELINK_ER_API Region : public EntityEntry
    {
    public:
        using EnumType = RegionType;

        static constexpr int SubtypeEnumOffset = 8;

        [[nodiscard]] static const std::map<RegionType, std::string>& GetTypeNames()
        {
            static const std::map<RegionType, std::string> data =
            {
                {RegionType::InvasionPoint, "InvasionPoint"},
                {RegionType::EnvironmentMapPoint, "EnvironmentMapPoint"},
                {RegionType::Sound, "Sound"},
                {RegionType::VFX, "VFX"},
                {RegionType::WindVFX, "WindVFX"},
                {RegionType::SpawnPoint, "SpawnPoint"},
                {RegionType::Message, "Message"},
                {RegionType::EnvironmentMapEffectBox, "EnvironmentMapEffectBox"},
                {RegionType::WindArea, "WindArea"},
                {RegionType::Connection, "Connection"},
                {RegionType::PatrolRoute22, "PatrolRoute22"},
                {RegionType::BuddySummonPoint, "BuddySummonPoint"},
                {RegionType::MufflingBox, "MufflingBox"},
                {RegionType::MufflingPortal, "MufflingPortal"},
                {RegionType::OtherSound, "OtherSound"},
                {RegionType::MufflingPlane, "MufflingPlane"},
                {RegionType::PatrolRoute, "PatrolRoute"},
                {RegionType::MapPoint, "MapPoint"},
                {RegionType::WeatherOverride, "WeatherOverride"},
                {RegionType::AutoDrawGroupPoint, "AutoDrawGroupPoint"},
                {RegionType::GroupDefeatReward, "GroupDefeatReward"},
                {RegionType::MapPointDiscoveryOverride, "MapPointDiscoveryOverride"},
                {RegionType::MapPointParticipationOverride, "MapPointParticipationOverride"},
                {RegionType::Hitset, "Hitset"},
                {RegionType::FastTravelRestriction, "FastTravelRestriction"},
                {RegionType::WeatherCreateAssetPoint, "WeatherCreateAssetPoint"},
                {RegionType::PlayArea, "PlayArea"},
                {RegionType::EnvironmentMapOutput, "EnvironmentMapOutput"},
                {RegionType::MountJump, "MountJump"},
                {RegionType::Dummy, "Dummy"},
                {RegionType::FallPreventionRemoval, "FallPreventionRemoval"},
                {RegionType::NavmeshCutting, "NavmeshCutting"},
                {RegionType::MapNameOverride, "MapNameOverride"},
                {RegionType::MountJumpFall, "MountJumpFall"},
                {RegionType::HorseRideOverride, "HorseRideOverride"},
                {RegionType::Other, "OtherRegion"},
            };
            return data;
        }

        Region() = default;

        explicit Region(const std::u16string& name) : EntityEntry(name) {}

        /// @brief Copy constructor that clones a new unique `Shape` (if present).
        Region(const Region& other) : EntityEntry(other.m_name)
        {
            // Copy shape if present.
            m_shape = other.m_shape ? other.m_shape->Clone() : nullptr;
            m_translate = other.m_translate;
            m_rotate = other.m_rotate;
            m_hUnk40 = other.m_hUnk40;
            m_eventLayer = other.m_eventLayer;
            m_unkShortsA = std::vector(other.m_unkShortsA);
            m_unkShortsB = std::vector(other.m_unkShortsB);
            m_attachedPart = other.m_attachedPart;
            m_attachedPartIndex = other.m_attachedPartIndex;
            m_entityId = other.m_entityId;
            m_dUnk08 = other.m_dUnk08;
            m_mapId = other.m_mapId;
            m_eUnk04 = other.m_eUnk04;
            m_eUnk0C = other.m_eUnk0C;
        }

        [[nodiscard]] virtual RegionType GetType() const = 0;

        /// @brief Get type enum `ShapeType` of this Region's shape. `shape == nullptr` also means `PointShape`.
        [[nodiscard]] ShapeType GetShapeType() const { return m_shape == nullptr ? ShapeType::PointShape : m_shape->GetType(); }

        /// @brief Set this region's shape to  Creates a new instance of given shape type, which is then returned for caller modification.
        // Previous shape will be discarded (as a unique_ptr).
        std::unique_ptr<Shape>& SetShapeType(ShapeType shapeType);

        [[nodiscard]] std::string GetShapeTypeName() const { return Shape::GetTypeNames().at(GetShapeType()); }

        [[nodiscard]] std::unique_ptr<Shape>& GetShape() { return m_shape; }

        [[nodiscard]] Vector3 GetTranslate() const { return m_translate; }
        void SetTranslate(const Vector3& translate) { this->m_translate = translate; }

        [[nodiscard]] Vector3 GetRotate() const { return m_rotate; }
        void SetRotate(const Vector3& rotate) { this->m_rotate = rotate; }

        [[nodiscard]] int32_t GetHUnk40() const { return m_hUnk40; }
        void SetHUnk40(const int32_t hUnk40) { this->m_hUnk40 = hUnk40; }

        [[nodiscard]] int32_t GetEventLayer() const { return m_eventLayer; }
        void SetEventLayer(const int32_t eventLayer) { this->m_eventLayer = eventLayer; }

        [[nodiscard]] Part* GetAttachedPart() const { return m_attachedPart.Get(); }
        void SetAttachedPart(Part* const part) { this->m_attachedPart.Set(part); }
        void SetAttachedPart(const std::unique_ptr<Part>& part) { this->m_attachedPart.Set(part); }

        [[nodiscard]] uint8_t GetDUnk08() const { return m_dUnk08; }
        void SetDUnk08(const uint8_t dUnk08) { this->m_dUnk08 = dUnk08; }

        [[nodiscard]] int32_t GetMapId() const { return m_mapId; }
        void SetMapId(const int32_t mapId) { this->m_mapId = mapId; }

        [[nodiscard]] int32_t GetEUnk04() const { return m_eUnk04; }
        void SetEUnk04(const int32_t eUnk04) { this->m_eUnk04 = eUnk04; }

        [[nodiscard]] int32_t GetEUnk0C() const { return m_eUnk0C; }
        void SetEUnk0C(const int32_t eUnk0C) { this->m_eUnk0C = eUnk0C; }

        void Deserialize(std::ifstream& stream) override;
        void Serialize(std::ofstream& stream, int supertypeIndex, int subtypeIndex) const override;

        virtual void DeserializeEntryReferences(const std::vector<Region*>& regions, const std::vector<Part*>& parts);
        virtual void SerializeEntryIndices(const std::vector<Region*>& regions, const std::vector<Part*>& parts);

        explicit operator std::string() const
        {
            return std::format(
                "{}[\"{}\", <{}>]", GetTypeNames().at(GetType()), GetNameUTF8(), GetShapeTypeName());
        }

    protected:

        /// @brief Stores child `Region` references for `Composite` shape (and each of their `unk04` values).
        struct CompositeShapeReferences
        {
            std::array<EntryReference<Region>, 8> regionReferences{};  // null references
            std::array<int, 8> regionIndices{};  // filled with -1 in constructor
            std::array<int, 8> unk04s{};  // default 0 is good

            /// @brief Default region indices are -1 (indicates unused).
            explicit CompositeShapeReferences()
            {
                regionIndices.fill(-1);
            }

            void SetReferences(const std::vector<Region*>& regions)
            {
                for (int i = 0; i < 8; ++i)
                    regionReferences[i].SetFromIndex(regions, regionIndices[i]);
            }

            void SetIndices(const Entry* sourceEntry, const std::vector<Region*>& regions)
            {
                for (int i = 0; i < 8; ++i)
                    regionIndices[i] = regionReferences[i].ToIndex(sourceEntry, regions);
            }
        };

        /// @brief `Shape` of this region. Must be a pointer for polymorphism. `nullptr` means a `Point` shape.
        std::unique_ptr<Shape> m_shape = nullptr;

        /// @brief Optional `CompositeShapeReferences` to store `Composite` type shape children and their `unk04` value.
        /// Stored here, rather than in `Composite`, to make use of safe entry referencing system.
        std::unique_ptr<CompositeShapeReferences> m_compositeChildren = nullptr;

        Vector3 m_translate = Vector3{};
        Vector3 m_rotate = Vector3{};  // Euler angles in radians
        int32_t m_hUnk40 = 0;
        int32_t m_eventLayer = -1;
        std::vector<int16_t> m_unkShortsA{};
        std::vector<int16_t> m_unkShortsB{};

        EntryReference<Part> m_attachedPart;
        int32_t m_attachedPartIndex = -1;
        // `entityId` defined in parent class.
        uint8_t m_dUnk08 = 0;

        int32_t m_mapId = 0;
        int32_t m_eUnk04 = 0;
        int32_t m_eUnk0C = 0;

        virtual bool DeserializeSubtypeData(std::ifstream& stream) { return false; }
        virtual bool SerializeSubtypeData(std::ofstream& stream) const { return false; }
    };

    class FIRELINK_ER_API InvasionPointRegion final : public Region
    {
    public:
        static constexpr auto Type = RegionType::InvasionPoint;

        InvasionPointRegion() = default;

        explicit InvasionPointRegion(const std::u16string& name) : Region(name) {}

        [[nodiscard]] RegionType GetType() const override { return Type; }

        [[nodiscard]] int GetPriority() const { return m_priority; }
        void SetPriority(const int priority) { m_priority = priority; }

    private:
        int m_priority = 0;

        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream) const override;
    };

    class FIRELINK_ER_API EnvironmentMapPointRegion final : public Region
    {
    public:
        static constexpr auto Type = RegionType::EnvironmentMapPoint;

        EnvironmentMapPointRegion() = default;

        explicit EnvironmentMapPointRegion(const std::u16string& name) : Region(name) {}

        [[nodiscard]] RegionType GetType() const override { return Type; }

        [[nodiscard]] float GetSUnk00() const { return sUnk00; }
        void SetSUnk00(const float unk00) { this->sUnk00 = unk00; }
        [[nodiscard]] int GetSUnk04() const { return sUnk04; }
        void SetSUnk04(const int unk04) { this->sUnk04 = unk04; }
        [[nodiscard]] bool IsSUnk0D() const { return sUnk0D; }
        void SetSUnk0D(const bool unk0D) { this->sUnk0D = unk0D; }
        [[nodiscard]] bool IsSUnk0E() const { return sUnk0E; }
        void SetSUnk0E(const bool unk0E) { this->sUnk0E = unk0E; }
        [[nodiscard]] bool IsSUnk0F() const { return sUnk0F; }
        void SetSUnk0F(const bool unk0F) { this->sUnk0F = unk0F; }
        [[nodiscard]] float GetSUnk10() const { return sUnk10; }
        void SetSUnk10(const float unk10) { this->sUnk10 = unk10; }
        [[nodiscard]] float GetSUnk14() const { return sUnk14; }
        void SetSUnk14(const float unk14) { this->sUnk14 = unk14; }
        [[nodiscard]] int GetSMapId() const { return sMapId; }
        void SetSMapId(const int mapId) { this->sMapId = mapId; }
        [[nodiscard]] int GetSUnk20() const { return sUnk20; }
        void SetSUnk20(const int unk20) { this->sUnk20 = unk20; }
        [[nodiscard]] int GetSUnk24() const { return sUnk24; }
        void SetSUnk24(const int unk24) { this->sUnk24 = unk24; }
        [[nodiscard]] int GetSUnk28() const { return sUnk28; }
        void SetSUnk28(const int unk28) { this->sUnk28 = unk28; }
        [[nodiscard]] uint8_t GetSUnk2C() const { return sUnk2C; }
        void SetSUnk2C(const uint8_t unk2C) { this->sUnk2C = unk2C; }
        [[nodiscard]] uint8_t GetSUnk2D() const { return sUnk2D; }
        void SetSUnk2D(const uint8_t unk2D) { this->sUnk2D = unk2D; }

    private:
        float sUnk00 = 0.0f;
        int sUnk04 = 0;
        bool sUnk0D = false;
        bool sUnk0E = false;
        bool sUnk0F = false;
        float sUnk10 = 0.0f;
        float sUnk14 = 0.0f;
        int sMapId = 0;
        int sUnk20 = 0;
        int sUnk24 = 0;
        int sUnk28 = 0;
        uint8_t sUnk2C = 0;
        uint8_t sUnk2D = 0;

        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream) const override;
    };

    class FIRELINK_ER_API SoundRegion final : public Region
    {
    public:
        static constexpr auto Type = RegionType::Sound;

        SoundRegion() = default;

        explicit SoundRegion(const std::u16string& name) : Region(name)
        {
            childRegionsIndices.fill(-1);
        }

        [[nodiscard]] RegionType GetType() const override { return Type; }

        [[nodiscard]] int GetSoundType() const { return soundType; }
        void SetSoundType(const int soundType) { this->soundType = soundType; }

        [[nodiscard]] int GetSoundId() const { return soundId; }
        void SetSoundId(const int soundId) { this->soundId = soundId; }

        [[nodiscard]] Region* GetChildRegionAtIndex(const int index) const { return childRegions[index].Get(); }
        void SetChildRegionAtIndex(const int index, Region* const region) { childRegions[index].Set(region); }
        void SetChildRegionAtIndex(const int index, const std::unique_ptr<Region>& region) { childRegions[index].Set(region); }

        [[nodiscard]] bool IsSUnk49() const { return sUnk49; }
        void SetSUnk49(const bool sUnk49) { this->sUnk49 = sUnk49; }

        void DeserializeEntryReferences(const std::vector<Region*>& regions, const std::vector<Part*>& parts) override;
        void SerializeEntryIndices(const std::vector<Region*>& regions, const std::vector<Part*>& parts) override;

    private:
        int soundType = 0;
        int soundId = 0;
        std::array<EntryReference<Region>, 16> childRegions{};
        std::array<int, 16> childRegionsIndices{};
        bool sUnk49 = false;

        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream) const override;
    };

    class FIRELINK_ER_API VFXRegion final : public Region
    {
    public:
        static constexpr auto Type = RegionType::VFX;

        VFXRegion() = default;

        explicit VFXRegion(const std::u16string& name) : Region(name) {}

        [[nodiscard]] RegionType GetType() const override { return Type; }

        [[nodiscard]] int GetEffectId() const { return effectId; }
        void SetEffectId(const int effectId) { this->effectId = effectId; }

        [[nodiscard]] int GetSUnk04() const { return sUnk04; }
        void SetSUnk04(const int unk04) { this->sUnk04 = unk04; }

    private:
        int effectId = 0;
        int sUnk04 = 0;

        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream) const override;};

    class FIRELINK_ER_API WindVFXRegion final : public Region
    {
    public:
        static constexpr auto Type = RegionType::WindVFX;

        WindVFXRegion() = default;

        explicit WindVFXRegion(const std::u16string& name) : Region(name) {}

        [[nodiscard]] RegionType GetType() const override { return Type; }

        [[nodiscard]] int GetEffectId() const { return effectId; }
        void SetEffectId(const int effectId) { this->effectId = effectId; }

        [[nodiscard]] Region* GetWindRegion() const { return windRegion.Get(); }
        void SetWindRegion(Region* const windRegion) { this->windRegion.Set(windRegion); }
        void SetWindRegion(const std::unique_ptr<Region>& windRegion) { this->windRegion.Set(windRegion); }

        [[nodiscard]] float GetSUnk08() const { return sUnk08; }
        void SetSUnk08(const float unk08) { this->sUnk08 = unk08; }

        void DeserializeEntryReferences(const std::vector<Region*>& regions, const std::vector<Part*>& parts) override;
        void SerializeEntryIndices(const std::vector<Region*>& regions, const std::vector<Part*>& parts) override;

    private:
        int effectId = 0;
        EntryReference<Region> windRegion;
        int windRegionIndex = -1;
        float sUnk08 = 0.0f;

        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream) const override;
    };


    class FIRELINK_ER_API SpawnPointRegion final : public Region
    {
    public:
        static constexpr auto Type = RegionType::SpawnPoint;

        SpawnPointRegion() = default;

        explicit SpawnPointRegion(const std::u16string& name) : Region(name) {}

        [[nodiscard]] RegionType GetType() const override { return Type; }

    private:
        // No actual data (just pads).

        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream) const override;
    };

    class FIRELINK_ER_API MessageRegion final : public Region
    {
    public:
        static constexpr auto Type = RegionType::Message;

        MessageRegion() = default;

        explicit MessageRegion(const std::u16string& name) : Region(name) {}

        [[nodiscard]] RegionType GetType() const override { return Type; }

        [[nodiscard]] int16_t GetMessageId() const { return messageId; }
        void SetMessageId(const int16_t messageId) { this->messageId = messageId; }
        [[nodiscard]] int16_t GetSUnk02() const { return sUnk02; }
        void SetSUnk02(const int16_t sUnk02) { this->sUnk02 = sUnk02; }
        [[nodiscard]] bool IsHidden() const { return hidden; }
        void SetHidden(const bool hidden) { this->hidden = hidden; }
        [[nodiscard]] int GetSUnk08() const { return sUnk08; }
        void SetSUnk08(const int sUnk08) { this->sUnk08 = sUnk08; }
        [[nodiscard]] int GetSUnk0C() const { return sUnk0C; }
        void SetSUnk0C(const int sUnk0C) { this->sUnk0C = sUnk0C; }
        [[nodiscard]] uint32_t GetEnableEventFlagId() const { return enableEventFlagId; }
        void SetEnableEventFlagId(const uint32_t enableEventFlagId) { this->enableEventFlagId = enableEventFlagId; }
        [[nodiscard]] int GetCharacterModelName() const { return characterModelName; }
        void SetCharacterModelName(const int characterModelName) { this->characterModelName = characterModelName; }
        [[nodiscard]] int GetCharacterId() const { return characterId; }
        void SetCharacterId(const int characterId) { this->characterId = characterId; }
        [[nodiscard]] int GetAnimationId() const { return animationId; }
        void SetAnimationId(const int animationId) { this->animationId = animationId; }
        [[nodiscard]] int GetPlayerId() const { return playerId; }
        void SetPlayerId(const int playerId) { this->playerId = playerId; }
    private:
        int16_t messageId = 0;
        int16_t sUnk02 = 0;
        bool hidden = false;  // stored as 32-bit int
        int sUnk08 = 0;
        int sUnk0C = 0;
        uint32_t enableEventFlagId = 0;
        int characterModelName = 0;  // TODO: Is this really a 'name ID' or just the ID? e.g. 1234 -> 'c1234'
        int characterId = 0; // NpcParam
        int animationId = 0;
        int playerId = 0; // CharaInitParam

        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream) const override;  };

    class FIRELINK_ER_API EnvironmentMapEffectBoxRegion final : public Region
    {
    public:
        static constexpr auto Type = RegionType::EnvironmentMapEffectBox;

        EnvironmentMapEffectBoxRegion() = default;

        explicit EnvironmentMapEffectBoxRegion(const std::u16string& name) : Region(name) {}

        [[nodiscard]] RegionType GetType() const override { return Type; }

        [[nodiscard]] float GetEnableDist() const { return enableDist; }
        void SetEnableDist(const float enableDist) { this->enableDist = enableDist; }
        [[nodiscard]] float GetTransitionDist() const { return transitionDist; }
        void SetTransitionDist(const float transitionDist) { this->transitionDist = transitionDist; }
        [[nodiscard]] int GetSUnk08() const { return sUnk08; }
        void SetSUnk08(const int sUnk08) { this->sUnk08 = sUnk08; }
        [[nodiscard]] int GetSUnk09() const { return sUnk09; }
        void SetSUnk09(const int sUnk09) { this->sUnk09 = sUnk09; }
        [[nodiscard]] int16_t GetSUnk0A() const { return sUnk0A; }
        void SetSUnk0A(const int16_t sUnk0A) { this->sUnk0A = sUnk0A; }
        [[nodiscard]] float GetSUnk24() const { return sUnk24; }
        void SetSUnk24(const float sUnk24) { this->sUnk24 = sUnk24; }
        [[nodiscard]] float GetSUnk28() const { return sUnk28; }
        void SetSUnk28(const float sUnk28) { this->sUnk28 = sUnk28; }
        [[nodiscard]] int16_t GetSUnk2C() const { return sUnk2C; }
        void SetSUnk2C(const int16_t sUnk2C) { this->sUnk2C = sUnk2C; }
        [[nodiscard]] bool IsSUnk2E() const { return sUnk2E; }
        void SetSUnk2E(const bool sUnk2E) { this->sUnk2E = sUnk2E; }
        [[nodiscard]] bool IsSUnk2F() const { return sUnk2f; }
        void SetSUnk2F(const bool sUnk2F) { sUnk2f = sUnk2F; }
        [[nodiscard]] int16_t GetSUnk30() const { return sUnk30; }
        void SetSUnk30(const int16_t sUnk30) { this->sUnk30 = sUnk30; }
        [[nodiscard]] bool IsSUnk33() const { return sUnk33; }
        void SetSUnk33(const bool sUnk33) { this->sUnk33 = sUnk33; }
        [[nodiscard]] int16_t GetSUnk34() const { return sUnk34; }
        void SetSUnk34(const int16_t sUnk34) { this->sUnk34 = sUnk34; }
        [[nodiscard]] int16_t GetSUnk36() const { return sUnk36; }
        void SetSUnk36(const int16_t sUnk36) { this->sUnk36 = sUnk36; }

    private:
        float enableDist = 0.0f;
        float transitionDist = 0.0f;
        uint8_t sUnk08 = 0;
        uint8_t sUnk09 = 0;
        int16_t sUnk0A = 0;
        float sUnk24 = 0.0f;
        float sUnk28 = 0.0f;
        int16_t sUnk2C = 0;
        bool sUnk2E = false;
        bool sUnk2f = false;
        int16_t sUnk30 = 0;
        bool sUnk33 = false;
        int16_t sUnk34 = 0;
        int16_t sUnk36 = 0;

        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream) const override;
    };

    class FIRELINK_ER_API WindAreaRegion final : public Region
    {
    public:
        static constexpr auto Type = RegionType::WindArea;

        WindAreaRegion() = default;

        explicit WindAreaRegion(const std::u16string& name) : Region(name) {}

        [[nodiscard]] RegionType GetType() const override { return Type; }

        // No subtype data read/write, not even pads.
    };

    class FIRELINK_ER_API ConnectionRegion final : public Region
    {
    public:
        static constexpr auto Type = RegionType::Connection;

        ConnectionRegion() = default;

        explicit ConnectionRegion(const std::u16string& name) : Region(name) {}

        [[nodiscard]] RegionType GetType() const override { return Type; }

        [[nodiscard]] std::array<int8_t, 4> GetTargetMapId() const { return targetMapId; }
        void SetTargetMapId(const std::array<int8_t, 4>& targetMapId) { this->targetMapId = targetMapId; }

    private:
        std::array<int8_t, 4> targetMapId = {0, 0, 0, 0};  // map reference, e.g. {10, 0, 0, 0} for 'm10_00_00_00'

        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream) const override;
    };

    class FIRELINK_ER_API PatrolRoute22Region final : public Region
    {
    public:
        static constexpr auto Type = RegionType::PatrolRoute22;

        PatrolRoute22Region() = default;

        explicit PatrolRoute22Region(const std::u16string& name) : Region(name) {}

        [[nodiscard]] RegionType GetType() const override { return Type; }
    private:
        // No subtype data (just pads).
        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream) const override;

    };

    class FIRELINK_ER_API BuddySummonPointRegion final : public Region
    {
    public:
        static constexpr auto Type = RegionType::BuddySummonPoint;

        BuddySummonPointRegion() = default;

        explicit BuddySummonPointRegion(const std::u16string& name) : Region(name) {}

        [[nodiscard]] RegionType GetType() const override { return Type; }
    private:
        // No subtype data (just pads).
        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream) const override;
    };

    class FIRELINK_ER_API MufflingBoxRegion final : public Region
    {
    public:
        static constexpr auto Type = RegionType::MufflingBox;

        MufflingBoxRegion() = default;

        explicit MufflingBoxRegion(const std::u16string& name) : Region(name) {}

        [[nodiscard]] RegionType GetType() const override { return Type; }

        [[nodiscard]] int GetSUnk00() const { return sUnk00; }
        void SetSUnk00(const int sUnk00) { this->sUnk00 = sUnk00; }
        [[nodiscard]] float GetSUnk24() const { return sUnk24; }
        void SetSUnk24(const float sUnk24) { this->sUnk24 = sUnk24; }
        [[nodiscard]] float GetSUnk34() const { return sUnk34; }
        void SetSUnk34(const float sUnk34) { this->sUnk34 = sUnk34; }
        [[nodiscard]] float GetSUnk3C() const { return sUnk3c; }
        void SetSUnk3C(const float sUnk3C) { sUnk3c = sUnk3C; }
        [[nodiscard]] float GetSUnk40() const { return sUnk40; }
        void SetSUnk40(const float sUnk40) { this->sUnk40 = sUnk40; }
        [[nodiscard]] float GetSUnk44() const { return sUnk44; }
        void SetSUnk44(const float sUnk44) { this->sUnk44 = sUnk44; }

    private:
        int sUnk00 = 0;
        float sUnk24 = 0.0f;
        float sUnk34 = 0.0f;
        float sUnk3c = 0.0f;
        float sUnk40 = 0.0f;
        float sUnk44 = 0.0f;

        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream) const override;
    };

    class FIRELINK_ER_API MufflingPortalRegion final : public Region
    {
    public:
        static constexpr auto Type = RegionType::MufflingPortal;

        MufflingPortalRegion() = default;

        explicit MufflingPortalRegion(const std::u16string& name) : Region(name) {}

        [[nodiscard]] RegionType GetType() const override { return Type; }

        [[nodiscard]] int GetSUnk00() const { return sUnk00; }
        void SetSUnk00(const int sUnk00) { this->sUnk00 = sUnk00; }

    private:
        int sUnk00 = 0;

        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream) const override;
    };

    class FIRELINK_ER_API OtherSoundRegion final : public Region
    {
    public:
        static constexpr auto Type = RegionType::OtherSound;

        OtherSoundRegion() = default;

        explicit OtherSoundRegion(const std::u16string& name) : Region(name) {}

        [[nodiscard]] RegionType GetType() const override { return Type; }

        [[nodiscard]] uint8_t GetSUnk00() const { return sUnk00; }
        void SetSUnk00(const uint8_t sUnk00) { this->sUnk00 = sUnk00; }
        [[nodiscard]] uint8_t GetSUnk01() const { return sUnk01; }
        void SetSUnk01(const uint8_t sUnk01) { this->sUnk01 = sUnk01; }
        [[nodiscard]] uint8_t GetSUnk02() const { return sUnk02; }
        void SetSUnk02(const uint8_t sUnk02) { this->sUnk02 = sUnk02; }
        [[nodiscard]] uint8_t GetSUnk03() const { return sUnk03; }
        void SetSUnk03(const uint8_t sUnk03) { this->sUnk03 = sUnk03; }
        [[nodiscard]] int GetSUnk04() const { return sUnk04; }
        void SetSUnk04(const int sUnk04) { this->sUnk04 = sUnk04; }
        [[nodiscard]] short GetSUnk08() const { return sUnk08; }
        void SetSUnk08(const short sUnk08) { this->sUnk08 = sUnk08; }
        [[nodiscard]] short GetSUnk0A() const { return sUnk0a; }
        void SetSUnk0A(const short sUnk0A) { sUnk0a = sUnk0A; }
        [[nodiscard]] uint8_t GetSUnk0C() const { return sUnk0c; }
        void SetSUnk0C(const uint8_t sUnk0C) { sUnk0c = sUnk0C; }

    private:
        uint8_t sUnk00 = 0;
        uint8_t sUnk01 = 0;
        uint8_t sUnk02 = 0;
        uint8_t sUnk03 = 0;
        int sUnk04 = 0;
        short sUnk08 = 0;
        short sUnk0a = 0;
        uint8_t sUnk0c = 0;

        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream) const override;
    };

    class FIRELINK_ER_API MufflingPlaneRegion final : public Region
    {
        // No subtype data at all.
    public:
        static constexpr auto Type = RegionType::MufflingPlane;

        MufflingPlaneRegion() = default;

        explicit MufflingPlaneRegion(const std::u16string& name) : Region(name) {}

        [[nodiscard]] RegionType GetType() const override { return Type; }
    };

    class FIRELINK_ER_API PatrolRouteRegion final : public Region
    {
    public:
        static constexpr auto Type = RegionType::PatrolRoute;

        PatrolRouteRegion() = default;

        explicit PatrolRouteRegion(const std::u16string& name) : Region(name) {}

        [[nodiscard]] RegionType GetType() const override { return Type; }

        [[nodiscard]] int GetSUnk00() const { return sUnk00; }
        void SetSUnk00(const int sUnk00) { this->sUnk00 = sUnk00; }

    private:
        int sUnk00 = 0;

        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream) const override;
    };

    class FIRELINK_ER_API MapPointRegion final : public Region
    {
    public:
        static constexpr auto Type = RegionType::MapPoint;

        MapPointRegion() = default;

        explicit MapPointRegion(const std::u16string& name) : Region(name) {}

        [[nodiscard]] RegionType GetType() const override { return Type; }

        [[nodiscard]] int GetSUnk00() const { return sUnk00; }
        void SetSUnk00(const int sUnk00) { this->sUnk00 = sUnk00; }
        [[nodiscard]] int GetSUnk04() const { return sUnk04; }
        void SetSUnk04(const int sUnk04) { this->sUnk04 = sUnk04; }
        [[nodiscard]] float GetSUnk08() const { return sUnk08; }
        void SetSUnk08(const float sUnk08) { this->sUnk08 = sUnk08; }
        [[nodiscard]] float GetSUnk0C() const { return sUnk0C; }
        void SetSUnk0C(const float sUnk0C) { this->sUnk0C = sUnk0C; }
        [[nodiscard]] float GetSUnk14() const { return sUnk14; }
        void SetSUnk14(const float sUnk14) { this->sUnk14 = sUnk14; }
        [[nodiscard]] float GetSUnk18() const { return sUnk18; }
        void SetSUnk18(const float sUnk18) { this->sUnk18 = sUnk18; }

    private:
        int sUnk00 = 0;
        int sUnk04 = 0;
        float sUnk08 = 0.0f;
        float sUnk0C = 0.0f;
        float sUnk14 = 0.0f;
        float sUnk18 = 0.0f;

        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream) const override;    };

    class FIRELINK_ER_API WeatherOverrideRegion final : public Region
    {
    public:
        static constexpr auto Type = RegionType::WeatherOverride;

        WeatherOverrideRegion() = default;

        explicit WeatherOverrideRegion(const std::u16string& name) : Region(name) {}

        [[nodiscard]] RegionType GetType() const override { return Type; }

        [[nodiscard]] int GetWeatherLotId() const { return weatherLotId; }
        void SetWeatherLotId(const int weatherLotId) { this->weatherLotId = weatherLotId; }

    private:
        int weatherLotId = 0;

        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream) const override;
    };

    class FIRELINK_ER_API AutoDrawGroupPointRegion final : public Region
    {
    public:
        static constexpr auto Type = RegionType::AutoDrawGroupPoint;

        AutoDrawGroupPointRegion() = default;

        explicit AutoDrawGroupPointRegion(const std::u16string& name) : Region(name) {}

        [[nodiscard]] RegionType GetType() const override { return Type; }

        [[nodiscard]] int GetSUnk00() const { return sUnk00; }
        void SetSUnk00(const int sUnk00) { this->sUnk00 = sUnk00; }

    private:
        int sUnk00 = 0;

        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream) const override;
    };

    class FIRELINK_ER_API GroupDefeatRewardRegion final : public Region
    {
    public:
        static constexpr auto Type = RegionType::GroupDefeatReward;

        GroupDefeatRewardRegion() = default;

        explicit GroupDefeatRewardRegion(const std::u16string& name) : Region(name)
        {
            groupPartsIndices.fill(-1);
        }

        [[nodiscard]] RegionType GetType() const override { return Type; }

        [[nodiscard]] int GetSUnk00() const { return sUnk00; }
        void SetSUnk00(const int sUnk00) { this->sUnk00 = sUnk00; }
        [[nodiscard]] int GetSUnk04() const { return sUnk04; }
        void SetSUnk04(const int sUnk04) { this->sUnk04 = sUnk04; }
        [[nodiscard]] int GetSUnk08() const { return sUnk08; }
        void SetSUnk08(const int sUnk08) { this->sUnk08 = sUnk08; }

        [[nodiscard]] Part* GetGroupPartAtIndex(const int index) const { return groupParts[index].Get(); }
        void SetGroupPartAtIndex(const int index, Part* const part) { groupParts[index].Set(part); }
        void SetGroupPartAtIndex(const int index, const std::unique_ptr<Part>& part) { groupParts[index].Set(part); }

        [[nodiscard]] int GetSUnk34() const { return sUnk34; }
        void SetSUnk34(const int sUnk34) { this->sUnk34 = sUnk34; }
        [[nodiscard]] int GetSUnk38() const { return sUnk38; }
        void SetSUnk38(const int sUnk38) { this->sUnk38 = sUnk38; }
        [[nodiscard]] int GetSUnk54() const { return sUnk54; }
        void SetSUnk54(const int sUnk54) { this->sUnk54 = sUnk54; }

        void DeserializeEntryReferences(const std::vector<Region*>& regions, const std::vector<Part*>& parts) override;
        void SerializeEntryIndices(const std::vector<Region*>& regions, const std::vector<Part*>& parts) override;

    private:
        int sUnk00 = 0;
        int sUnk04 = 0;
        int sUnk08 = 0;
        std::array<EntryReference<Part>, 8> groupParts{};
        std::array<int32_t, 8> groupPartsIndices{};  // TODO: always Characters (still by Part supertype index)?
        int sUnk34 = 0;
        int sUnk38 = 0;
        int sUnk54 = 0;

        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream) const override;
    };

    class FIRELINK_ER_API MapPointDiscoveryOverrideRegion final : public Region
    {
        // No data at all.
    public:
        static constexpr auto Type = RegionType::MapPointDiscoveryOverride;

        MapPointDiscoveryOverrideRegion() = default;

        explicit MapPointDiscoveryOverrideRegion(const std::u16string& name) : Region(name) {}

        [[nodiscard]] RegionType GetType() const override { return Type; }
    };

    class FIRELINK_ER_API MapPointParticipationOverrideRegion final : public Region
    {
        // No data at all.
    public:
        static constexpr auto Type = RegionType::MapPointParticipationOverride;

        MapPointParticipationOverrideRegion() = default;

        explicit MapPointParticipationOverrideRegion(const std::u16string& name) : Region(name) {}
        
        [[nodiscard]] RegionType GetType() const override { return Type; }
    };
    
    class FIRELINK_ER_API HitsetRegion final : public Region
    {
    public:
        static constexpr auto Type = RegionType::Hitset;

        HitsetRegion() = default;

        explicit HitsetRegion(const std::u16string& name) : Region(name) {}
        
        [[nodiscard]] RegionType GetType() const override { return Type; }

        [[nodiscard]] int GetSUnk00() const { return sUnk00; }
        void SetSUnk00(const int sUnk00) { this->sUnk00 = sUnk00; }

    private:
        int sUnk00 = 0;

        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream) const override;};
    
    class FIRELINK_ER_API FastTravelRestrictionRegion final : public Region
    {
    public:
        static constexpr auto Type = RegionType::FastTravelRestriction;

        FastTravelRestrictionRegion() = default;

        explicit FastTravelRestrictionRegion(const std::u16string& name) : Region(name) {}
        
        [[nodiscard]] RegionType GetType() const override { return Type; }

        [[nodiscard]] uint32_t GetEventFlagId() const { return eventFlagId; }
        void SetEventFlagId(const uint32_t eventFlagId) { this->eventFlagId = eventFlagId; }

    private:
        uint32_t eventFlagId = 0;

        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream) const override;
    };
    
    class FIRELINK_ER_API WeatherCreateAssetPointRegion final : public Region
    {
    public:
        static constexpr auto Type = RegionType::WeatherCreateAssetPoint;

        WeatherCreateAssetPointRegion() = default;

        explicit WeatherCreateAssetPointRegion(const std::u16string& name) : Region(name) {}
        
        [[nodiscard]] RegionType GetType() const override { return Type; }
    private:
        // No real data.
        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream) const override;
    };
    
    class FIRELINK_ER_API PlayAreaRegion final : public Region
    {
    public:
        static constexpr auto Type = RegionType::PlayArea;

        PlayAreaRegion() = default;

        explicit PlayAreaRegion(const std::u16string& name) : Region(name) {}
        
        [[nodiscard]] RegionType GetType() const override { return Type; }

        [[nodiscard]] int GetSUnk00() const { return sUnk00; }
        void SetSUnk00(const int sUnk00) { this->sUnk00 = sUnk00; }
        [[nodiscard]] int GetSUnk04() const { return sUnk04; }
        void SetSUnk04(const int sUnk04) { this->sUnk04 = sUnk04; }

    private:
        int sUnk00 = 0;
        int sUnk04 = 0;

        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream) const override;
    };
    
    class FIRELINK_ER_API EnvironmentMapOutputRegion final : public Region
    {
    public:
        static constexpr auto Type = RegionType::EnvironmentMapOutput;

        EnvironmentMapOutputRegion() = default;

        explicit EnvironmentMapOutputRegion(const std::u16string& name) : Region(name) {}
        
        [[nodiscard]] RegionType GetType() const override { return Type; }

        // No subtype data at all (not even pads).
    };
    
    class FIRELINK_ER_API MountJumpRegion final : public Region
    {
    public:
        static constexpr auto Type = RegionType::MountJump;

        MountJumpRegion() = default;

        explicit MountJumpRegion(const std::u16string& name) : Region(name) {}
        
        [[nodiscard]] RegionType GetType() const override { return Type; }

        [[nodiscard]] float GetJumpHeight() const { return jumpHeight; }
        void SetJumpHeight(const float jumpHeight) { this->jumpHeight = jumpHeight; }
        [[nodiscard]] int GetSUnk04() const { return sUnk04; }
        void SetSUnk04(const int sUnk04) { this->sUnk04 = sUnk04; }

    private:
        float jumpHeight = 0.0f;
        int sUnk04 = 0;

        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream) const override;
    };
    
    class FIRELINK_ER_API DummyRegion final : public Region
    {
    public:
        static constexpr auto Type = RegionType::Dummy;

        DummyRegion() = default;

        explicit DummyRegion(const std::u16string& name) : Region(name) {}
        
        [[nodiscard]] RegionType GetType() const override { return Type; }

        [[nodiscard]] int GetSUnk00() const { return sUnk00; }
        void SetSUnk00(const int sUnk00) { this->sUnk00 = sUnk00; }

    private:
        int sUnk00 = 0;

        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream) const override;
    };
    
    class FIRELINK_ER_API FallPreventionRemovalRegion final : public Region
    {
    public:
        static constexpr auto Type = RegionType::FallPreventionRemoval;

        FallPreventionRemovalRegion() = default;

        explicit FallPreventionRemovalRegion(const std::u16string& name) : Region(name) {}
        
        [[nodiscard]] RegionType GetType() const override { return Type; }
    private:
        // No real data, just pads.
        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream) const override;
    };
    
    class FIRELINK_ER_API NavmeshCuttingRegion final : public Region
    {
    public:
        static constexpr auto Type = RegionType::NavmeshCutting;

        NavmeshCuttingRegion() = default;

        explicit NavmeshCuttingRegion(const std::u16string& name) : Region(name) {}
        
        [[nodiscard]] RegionType GetType() const override { return Type; }
    private:
        // No real data, just pads.
        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream) const override;
    };
    
    class FIRELINK_ER_API MapNameOverrideRegion final : public Region
    {
    public:
        static constexpr auto Type = RegionType::MapNameOverride;

        MapNameOverrideRegion() = default;

        explicit MapNameOverrideRegion(const std::u16string& name) : Region(name) {}
        
        [[nodiscard]] RegionType GetType() const override { return Type; }

        [[nodiscard]] int GetMapNameId() const { return mapNameId; }
        void SetMapNameId(const int mapNameId) { this->mapNameId = mapNameId; }

    private:
        int mapNameId = 0;

        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream) const override;
    };
    
    class FIRELINK_ER_API MountJumpFallRegion final : public Region
    {
    public:
        static constexpr auto Type = RegionType::MountJumpFall;

        MountJumpFallRegion() = default;

        explicit MountJumpFallRegion(const std::u16string& name) : Region(name) {}
        
        [[nodiscard]] RegionType GetType() const override { return Type; }

    private:
        // No real data, just pads.
        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream) const override;
    };
    
    class FIRELINK_ER_API HorseRideOverrideRegion final : public Region
    {
    public:

        /// @brief Exhaustive values for `HorseRideOverrideRegion.overrideType`.
        enum class HorseRideOverrideType : int
        {
            Default = 0,
            Prevent = 1,
            Allow = 2
        };

        static constexpr auto Type = RegionType::HorseRideOverride;

        HorseRideOverrideRegion() = default;

        explicit HorseRideOverrideRegion(const std::u16string& name) : Region(name) {}
        
        [[nodiscard]] RegionType GetType() const override { return Type; }

        [[nodiscard]] HorseRideOverrideType GetOverrideType() const { return overrideType; }
        void SetOverrideType(const HorseRideOverrideType overrideType) { this->overrideType = overrideType; }

    private:
        HorseRideOverrideType overrideType = HorseRideOverrideType::Default;

        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream) const override;
    };
    
    /// @brief `OtherRegion`s are used frequently as generic EMEVD triggers, etc.
    class FIRELINK_ER_API OtherRegion final : public Region
    {
    public:
        static constexpr auto Type = RegionType::Other;

        OtherRegion() = default;

        explicit OtherRegion(const std::u16string& name) : Region(name) {}
        
        [[nodiscard]] RegionType GetType() const override { return Type; }

        // No subtype data at all (not even pads).
    };
}
