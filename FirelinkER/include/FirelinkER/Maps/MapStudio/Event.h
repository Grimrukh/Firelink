#pragma once

#include <array>
#include <cstdint>
#include <format>
#include <map>
#include <string>

#include "FirelinkER/Export.h"
#include "Entry.h"
#include "EntryReference.h"
#include "Enums.h"


namespace FirelinkER::Maps::MapStudio
{
    // Forward declarations to other `Entry` subtypes.
    class Part;
    class Region;

    // MSB Event supertype. Does not inherit from `EntityEntry` because despite all having an `entityId` field, not all
    // events actually use it (i.e. not all are valid EMEVD variables).
    class FIRELINK_ER_API Event : public Entry
    {
    public:
        using EnumType = EventType;

        static constexpr int SubtypeEnumOffset = 12;

        [[nodiscard]] static const std::map<EventType, std::string>& GetTypeNames()
        {
            static const std::map<EventType, std::string> data =
            {
                {EventType::Treasure, "Treasure"},
                {EventType::Spawner, "Spawner"},
                {EventType::ObjAct, "ObjAct"},
                {EventType::Navigation, "Navigation"},
                {EventType::NPCInvasion, "NPCInvasion"},
                {EventType::Platoon, "Platoon"},
                {EventType::PatrolRoute, "PatrolRoute"},
                {EventType::Mount, "Mount"},
                {EventType::SignPool, "SignPool"},
                {EventType::RetryPoint, "RetryPoint"},
                {EventType::AreaTeam, "AreaTeam"},
                {EventType::Other, "Other"},
            };
            return data;
        }

        Event() = default;

        explicit Event(const std::u16string& name) : Entry(name) {}

        /// @brief Returns `static constexpr EventType` defined in each subclass.
        [[nodiscard]] virtual EventType GetType() const = 0;

        [[nodiscard]] int GetSubtypeIndexOverride() const { return subtypeIndexOverride; }

        void SetSubtypeIndexOverride(const int subtypeIndexOverride)
        {
            this->subtypeIndexOverride = subtypeIndexOverride;
        }

        [[nodiscard]] int32_t GetHUnk14() const { return hUnk14; }
        void SetHUnk14(const int32_t hUnk14) { this->hUnk14 = hUnk14; }

        [[nodiscard]] Part* GetAttachedPart() const { return attachedPart.Get(); }
        void SetAttachedPart(Part* attachedPart) { this->attachedPart.Set(attachedPart); }
        void SetAttachedPart(const std::unique_ptr<Part>& attachedPart) { this->attachedPart.Set(attachedPart); }

        [[nodiscard]] Region* GetAttachedRegion() const { return attachedRegion.Get(); }
        void SetAttachedRegion(Region* attachedRegion) { this->attachedRegion = attachedRegion; }
        void SetAttachedRegion(const std::unique_ptr<Region>& attachedRegion) { this->attachedRegion.Set(attachedRegion); }

        [[nodiscard]] int32_t GetEntityId() const { return entityId; }
        void SetEntityId(const int32_t entityId) { this->entityId = entityId; }

        [[nodiscard]] uint8_t GetDUnk0C() const { return dUnk0C; }
        void SetDUnk0C(const uint8_t dUnk0C) { this->dUnk0C = dUnk0C; }

        [[nodiscard]] int32_t GetMapId() const { return mapId; }
        void SetMapId(const int32_t mapId) { this->mapId = mapId; }

        [[nodiscard]] int32_t GetEUnk04() const { return eUnk04; }
        void SetEUnk04(const int32_t eUnk04) { this->eUnk04 = eUnk04; }

        [[nodiscard]] int32_t GetEUnk08() const { return eUnk08; }
        void SetEUnk08(const int32_t eUnk08) { this->eUnk08 = eUnk08; }

        [[nodiscard]] int32_t GetEUnk0C() const { return eUnk0C; }
        void SetEUnk0C(const int32_t eUnk0C) { this->eUnk0C = eUnk0C; }

        void Deserialize(std::ifstream& stream) override;
        void Serialize(std::ofstream& stream, int supertypeIndex, int subtypeIndex) const override;

        virtual void DeserializeEntryReferences(const std::vector<Part*>& parts, const std::vector<Region*>& regions);
        virtual void SerializeEntryIndices(const std::vector<Part*>& parts, const std::vector<Region*>& regions);

        explicit operator std::string() const
        {
            return std::format("{}[\"{}\"]", GetTypeNames().at(GetType()), GetNameUTF8());
        }

    protected:
        int subtypeIndexOverride = -1; // used for `Other` event type (unknown purpose)

        int32_t hUnk14 = 0;
        EntryReference<Part> attachedPart;
        int32_t attachedPartIndex = -1;
        EntryReference<Region> attachedRegion;
        int32_t attachedRegionIndex = -1;
        int32_t entityId = 0;
        uint8_t dUnk0C = 0;
        int32_t mapId = -1;
        int32_t eUnk04 = 0;
        int32_t eUnk08 = 0;
        int32_t eUnk0C = 0;

        virtual bool DeserializeSubtypeData(std::ifstream& stream) { return false; }
        virtual bool SerializeSubtypeData(std::ofstream& stream) const { return false; }

    };

    class FIRELINK_ER_API TreasureEvent final : public Event
    {
    public:
        static constexpr auto Type = EventType::Treasure;

        TreasureEvent() = default;

        explicit TreasureEvent(const std::u16string& name) : Event(name) {}

        [[nodiscard]] EventType GetType() const override { return Type; }

        [[nodiscard]] Part* GetTreasurePart() const { return treasurePart.Get(); }
        void SetTreasurePart(Part* treasurePart) { this->treasurePart.Set(treasurePart); }
        void SetTreasurePart(const std::unique_ptr<Part>& treasurePart) { this->treasurePart.Set(treasurePart); }

        [[nodiscard]] int32_t GetItemLot() const { return itemLot; }
        void SetItemLot(const int32_t itemLot) { this->itemLot = itemLot; }

        [[nodiscard]] int32_t GetActionButtonId() const { return actionButtonId; }
        void SetActionButtonId(const int32_t actionButtonId) { this->actionButtonId = actionButtonId; }

        [[nodiscard]] int32_t GetPickupAnimationId() const { return pickupAnimationId; }
        void SetPickupAnimationId(const int32_t pickupAnimationId) { this->pickupAnimationId = pickupAnimationId; }

        [[nodiscard]] bool isInChest() const { return inChest; }
        void SetInChest(const bool inChest) { this->inChest = inChest; }

        [[nodiscard]] bool isStartsDisabled() const { return startsDisabled; }
        void SetStartsDisabled(const bool startsDisabled) { this->startsDisabled = startsDisabled; }

        void DeserializeEntryReferences(const std::vector<Part*>& parts, const std::vector<Region*>& regions) override;
        void SerializeEntryIndices(const std::vector<Part*>& parts, const std::vector<Region*>& regions) override;

    private:
        EntryReference<Part> treasurePart;
        int32_t treasurePartIndex = -1;
        int32_t itemLot = -1;
        int32_t actionButtonId = 0;
        int32_t pickupAnimationId = 0;
        bool inChest = false;
        bool startsDisabled = false;

        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream) const override;
    };

    class FIRELINK_ER_API SpawnerEvent final : public Event
    {
    public:
        static constexpr auto Type = EventType::Spawner;

        SpawnerEvent() = default;

        explicit SpawnerEvent(const std::u16string& name) : Event(name)
        {
            spawnPartsIndices.fill(-1);
            spawnRegionsIndices.fill(-1);
        }

        [[nodiscard]] EventType GetType() const override { return Type; }

        [[nodiscard]] int32_t GetMaxCount() const { return maxCount; }
        void SetMaxCount(const int32_t maxCount) { this->maxCount = maxCount; }

        [[nodiscard]] int8_t GetSpawnerType() const { return spawnerType; }
        void SetSpawnerType(const int8_t spawnerType) { this->spawnerType = spawnerType; }

        [[nodiscard]] int16_t GetLimitCount() const { return limitCount; }
        void SetLimitCount(const int16_t limitCount) { this->limitCount = limitCount; }

        [[nodiscard]] int16_t GetMinSpawnerCount() const { return minSpawnerCount; }
        void SetMinSpawnerCount(const int16_t minSpawnerCount) { this->minSpawnerCount = minSpawnerCount; }

        [[nodiscard]] int16_t GetMaxSpawnerCount() const { return maxSpawnerCount; }
        void SetMaxSpawnerCount(const int16_t maxSpawnerCount) { this->maxSpawnerCount = maxSpawnerCount; }

        [[nodiscard]] float GetMinInterval() const { return minInterval; }
        void SetMinInterval(const float minInterval) { this->minInterval = minInterval; }

        [[nodiscard]] float GetMaxInterval() const { return maxInterval; }
        void SetMaxInterval(const float maxInterval) { this->maxInterval = maxInterval; }

        [[nodiscard]] uint8_t GetInitialSpawnCount() const { return initialSpawnCount; }
        void SetInitialSpawnCount(const uint8_t initialSpawnCount) { this->initialSpawnCount = initialSpawnCount; }

        [[nodiscard]] float GetSpawnerUnk14() const { return spawnerUnk14; }
        void SetSpawnerUnk14(const float spawnerUnk14) { this->spawnerUnk14 = spawnerUnk14; }

        [[nodiscard]] float GetSpawnerUnk18() const { return spawnerUnk18; }
        void SetSpawnerUnk18(const float spawnerUnk18) { this->spawnerUnk18 = spawnerUnk18; }

        [[nodiscard]] Part* GetSpawnPartAtIndex(const int index) const { return spawnParts[index].Get(); }
        void SetSpawnPartAtIndex(const int index, Part* spawnPart) { spawnParts[index].Set(spawnPart); }
        void SetSpawnPartAtIndex(const int index, const std::unique_ptr<Part>& spawnPart) { spawnParts[index].Set(spawnPart); }

        [[nodiscard]] Region* GetSpawnRegionAtIndex(const int index) const { return spawnRegions[index].Get(); }
        void SetSpawnRegionAtIndex(const int index, Region* spawnRegion) { spawnRegions[index].Set(spawnRegion); }
        void SetSpawnRegionAtIndex(const int index, const std::unique_ptr<Region>& spawnRegion) { spawnRegions[index].Set(spawnRegion); }

        void DeserializeEntryReferences(const std::vector<Part*>& parts, const std::vector<Region*>& regions) override;
        void SerializeEntryIndices(const std::vector<Part*>& parts, const std::vector<Region*>& regions) override;

    private:
        uint8_t maxCount = 255;
        int8_t spawnerType = 0;
        int16_t limitCount = -1;
        int16_t minSpawnerCount = 1;
        int16_t maxSpawnerCount = 1;
        float minInterval = 1.0f;
        float maxInterval = 1.0f;
        uint8_t initialSpawnCount = 1;
        float spawnerUnk14 = 0.0f;
        float spawnerUnk18 = 0.0f;

        std::array<int32_t, 32> spawnPartsIndices{};
        std::array<int32_t, 8> spawnRegionsIndices{};
        std::array<EntryReference<Part>, 32> spawnParts{};
        std::array<EntryReference<Region>, 8> spawnRegions{};

        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream) const override;
    };


    class FIRELINK_ER_API NavigationEvent final : public Event
    {
    public:
        static constexpr auto Type = EventType::Navigation;


NavigationEvent() = default;

        explicit NavigationEvent(const std::u16string& name) : Event(name) {}

        [[nodiscard]] EventType GetType() const override { return Type; }

        [[nodiscard]] Region* GetNavigationRegion() const { return navigationRegion.Get(); }
        void SetNavigationRegion(Region* navigationRegion) { this->navigationRegion = navigationRegion; }

        void DeserializeEntryReferences(const std::vector<Part*>& parts, const std::vector<Region*>& regions) override;
        void SerializeEntryIndices(const std::vector<Part*>& parts, const std::vector<Region*>& regions) override;

    private:
        EntryReference<Region> navigationRegion;
        int32_t navigationRegionIndex = -1;

        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream) const override;
    };

    class FIRELINK_ER_API ObjActEvent final : public Event
    {
    public:
        static constexpr auto Type = EventType::ObjAct;

        ObjActEvent() = default;

        explicit ObjActEvent(const std::u16string& name) : Event(name) {}

        [[nodiscard]] EventType GetType() const override { return Type; }

        [[nodiscard]] int32_t GetObjActEntityId() const { return objActEntityId; }
        void SetObjActEntityId(const int32_t objActEntityId) { this->objActEntityId = objActEntityId; }

        [[nodiscard]] Part* GetObjActPart() const { return objActPart.Get(); }
        void SetObjActPart(Part* objActPart) { this->objActPart.Set(objActPart); }
        void SetObjActPart(const std::unique_ptr<Part>& objActPart) { this->objActPart.Set(objActPart); }

        [[nodiscard]] int32_t GetObjActParamId() const { return objActParamId; }
        void SetObjActParamId(const int32_t objActParamId) { this->objActParamId = objActParamId; }

        [[nodiscard]] uint8_t GetObjActState() const { return objActState; }
        void SetObjActState(const uint8_t objActState) { this->objActState = objActState; }

        [[nodiscard]] int32_t GetObjActFlag() const { return objActFlag; }
        void SetObjActFlag(const int32_t objActFlag) { this->objActFlag = objActFlag; }

        void DeserializeEntryReferences(const std::vector<Part*>& parts, const std::vector<Region*>& regions) override;
        void SerializeEntryIndices(const std::vector<Part*>& parts, const std::vector<Region*>& regions) override;

    private:
        int32_t objActEntityId = -1;
        EntryReference<Part> objActPart;
        int32_t objActPartIndex = -1;
        int32_t objActParamId = -1;
        uint8_t objActState = 0;
        int32_t objActFlag = 0;

        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream) const override;
    };

    class FIRELINK_ER_API NPCInvasionEvent final : public Event
    {
        int32_t hostEntityId = -1;
        int32_t invasionFlagId = -1;
        int32_t activateGoodId = -1;
        int32_t unk0c = 0;
        int32_t unk10 = 0;
        int32_t unk14 = 0;
        int32_t unk18 = 0;
        int32_t unk1c = 0;

        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream) const override;

    public:
        static constexpr auto Type = EventType::NPCInvasion;

        NPCInvasionEvent() = default;

        explicit NPCInvasionEvent(const std::u16string& name) : Event(name) {}

        [[nodiscard]] EventType GetType() const override { return Type; }

        [[nodiscard]] int32_t GetHostEntityId() const { return hostEntityId; }
        void SetHostEntityId(const int32_t hostEntityId) { this->hostEntityId = hostEntityId; }

        [[nodiscard]] int32_t GetInvasionFlagId() const { return invasionFlagId; }
        void SetInvasionFlagId(const int32_t invasionFlagId) { this->invasionFlagId = invasionFlagId; }

        [[nodiscard]] int32_t GetActivateGoodId() const { return activateGoodId; }
        void SetActivateGoodId(const int32_t activateGoodId) { this->activateGoodId = activateGoodId; }

        [[nodiscard]] int32_t GetUnk0c() const { return unk0c; }
        void SetUnk0c(const int32_t unk0c) { this->unk0c = unk0c; }

        [[nodiscard]] int32_t GetUnk10() const { return unk10; }
        void SetUnk10(const int32_t unk10) { this->unk10 = unk10; }

        [[nodiscard]] int32_t GetUnk14() const { return unk14; }
        void SetUnk14(const int32_t unk14) { this->unk14 = unk14; }

        [[nodiscard]] int32_t GetUnk18() const { return unk18; }
        void SetUnk18(const int32_t unk18) { this->unk18 = unk18; }

        [[nodiscard]] int32_t GetUnk1c() const { return unk1c; }
        void SetUnk1c(const int32_t unk1c) { this->unk1c = unk1c; }
    };

    class FIRELINK_ER_API PlatoonEvent final : public Event
    {
    public:
        static constexpr auto Type = EventType::Platoon;

        PlatoonEvent() = default;

        explicit PlatoonEvent(const std::u16string& name) : Event(name)
        {
            platoonPartsIndices.fill(-1);
        }

        [[nodiscard]] EventType GetType() const override { return Type; }

        [[nodiscard]] int32_t GetPlatoonIdScriptActivate() const { return platoonIdScriptActivate; }
        void SetPlatoonIdScriptActivate(const int32_t platoonIdScriptActivate)
        {
            this->platoonIdScriptActivate = platoonIdScriptActivate;
        }

        [[nodiscard]] int32_t GetState() const { return state; }
        void SetState(const int32_t state) { this->state = state; }

        [[nodiscard]] Part* GetPlatoonPartAtIndex(const int index) const { return platoonParts[index].Get(); }
        void SetPlatoonPartAtIndex(const int index, Part* platoonPart) { platoonParts[index].Set(platoonPart); }
        void SetPlatoonPartAtIndex(const int index, const std::unique_ptr<Part>& platoonPart) { platoonParts[index].Set(platoonPart); }

        void DeserializeEntryReferences(const std::vector<Part*>& parts, const std::vector<Region*>& regions) override;
        void SerializeEntryIndices(const std::vector<Part*>& parts, const std::vector<Region*>& regions) override;

    private:
        int32_t platoonIdScriptActivate = -1;
        int32_t state = -1;
        std::array<EntryReference<Part>, 32> platoonParts{};
        std::array<int32_t, 32> platoonPartsIndices{};

        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream) const override;
    };

    class FIRELINK_ER_API PatrolRouteEvent final : public Event
    {
    public:
        static constexpr auto Type = EventType::PatrolRoute;

        PatrolRouteEvent() = default;

        explicit PatrolRouteEvent(const std::u16string& name) : Event(name)
        {
            patrolRegionsIndices.fill(-1);
        }

        [[nodiscard]] EventType GetType() const override { return Type; }

        [[nodiscard]] uint8_t GetPatrolType() const { return patrolType; }
        void SetPatrolType(const uint8_t patrolType) { this->patrolType = patrolType; }

        [[nodiscard]] Region* GetPatrolRegionAtIndex(const int index) const { return patrolRegions[index].Get(); }
        void SetPatrolRegionAtIndex(const int index, Region* region) { this->patrolRegions[index].Set(region); }
        void SetPatrolRegionAtIndex(const int index, const std::unique_ptr<Region>& region) { this->patrolRegions[index].Set(region); }

        void DeserializeEntryReferences(const std::vector<Part*>& parts, const std::vector<Region*>& regions) override;
        void SerializeEntryIndices(const std::vector<Part*>& parts, const std::vector<Region*>& regions) override;

    private:
        uint8_t patrolType = 0;
        std::array<EntryReference<Region>, 64> patrolRegions{};
        std::array<int16_t, 64> patrolRegionsIndices{};

        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream) const override;
    };

    class FIRELINK_ER_API MountEvent final : public Event
    {
    public:
        static constexpr auto Type = EventType::Mount;

        MountEvent() = default;

        explicit MountEvent(const std::u16string& name) : Event(name) {}

        [[nodiscard]] EventType GetType() const override { return Type; }

        [[nodiscard]] Part* GetRiderPart() const { return riderPart.Get(); }
        void SetRiderPart(Part* riderPart) { this->riderPart.Set(riderPart); }
        void SetRiderPart(const std::unique_ptr<Part>& riderPart) { this->riderPart.Set(riderPart); }

        [[nodiscard]] Part* GetMountPart() const { return mountPart.Get(); }
        void SetMountPart(Part* mountPart) { this->mountPart.Set(mountPart); }
        void SetMountPart(const std::unique_ptr<Part>& mountPart) { this->mountPart.Set(mountPart); }

        void DeserializeEntryReferences(const std::vector<Part*>& parts, const std::vector<Region*>& regions) override;
        void SerializeEntryIndices(const std::vector<Part*>& parts, const std::vector<Region*>& regions) override;

    private:
        EntryReference<Part> riderPart;
        int32_t riderPartIndex = -1;
        EntryReference<Part> mountPart;
        int32_t mountPartIndex = -1;

        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream) const override;
    };

    class FIRELINK_ER_API SignPoolEvent final : public Event
    {
    public:
        static constexpr auto Type = EventType::SignPool;

        SignPoolEvent() = default;

        explicit SignPoolEvent(const std::u16string& name) : Event(name) {}

        [[nodiscard]] EventType GetType() const override { return Type; }

        [[nodiscard]] Part* GetSignPart() const { return signPart.Get(); }
        void SetSignPart(Part* signPart) { this->signPart.Set(signPart); }
        void SetSignPart(const std::unique_ptr<Part>& signPart) { this->signPart.Set(signPart); }

        [[nodiscard]] int32_t GetSignPoolUnk04() const { return signPoolUnk04; }
        void SetSignPoolUnk04(const int32_t signPoolUnk04) { this->signPoolUnk04 = signPoolUnk04; }

        void DeserializeEntryReferences(const std::vector<Part*>& parts, const std::vector<Region*>& regions) override;
        void SerializeEntryIndices(const std::vector<Part*>& parts, const std::vector<Region*>& regions) override;

    private:
        EntryReference<Part> signPart;
        int32_t signPartIndex = -1;
        int32_t signPoolUnk04 = 0;

        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream) const override;
    };

    class FIRELINK_ER_API RetryPointEvent final : public Event
    {
    public:
        static constexpr auto Type = EventType::RetryPoint;

        RetryPointEvent() = default;

        explicit RetryPointEvent(const std::u16string& name) : Event(name) {}

        [[nodiscard]] EventType GetType() const override { return Type; }

        [[nodiscard]] Part* GetRetryPart() const { return retryPart.Get(); }
        void SetRetryPart(Part* retryPart) { this->retryPart.Set(retryPart); }
        void SetRetryPart(const std::unique_ptr<Part>& retryPart) { this->retryPart.Set(retryPart); }

        [[nodiscard]] int32_t GetEventFlagId() const { return eventFlagId; }
        void SetEventFlagId(const int32_t eventFlagId) { this->eventFlagId = eventFlagId; }

        [[nodiscard]] float GetRetryPointUnk08() const { return retryPointUnk08; }
        void SetRetryPointUnk08(const float retryPointUnk08) { this->retryPointUnk08 = retryPointUnk08; }

        [[nodiscard]] Region* GetRetryRegion() const { return retryRegion.Get(); }
        void SetRetryRegion(Region* const region) { this->retryRegion.Set(region); }
        void SetRetryRegion(const std::unique_ptr<Region>& region) { this->retryRegion.Set(region); }

        void DeserializeEntryReferences(const std::vector<Part*>& parts, const std::vector<Region*>& regions) override;
        void SerializeEntryIndices(const std::vector<Part*>& parts, const std::vector<Region*>& regions) override;

    private:
        EntryReference<Part> retryPart;
        int32_t retryPartIndex = -1;
        int32_t eventFlagId = 0;
        float retryPointUnk08 = 0.0f;
        EntryReference<Region> retryRegion;
        int16_t retryRegionIndex = -1;

        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream) const override;
    };

    class FIRELINK_ER_API AreaTeamEvent final : public Event
    {
    public:
        static constexpr auto Type = EventType::AreaTeam;

        AreaTeamEvent() = default;

        explicit AreaTeamEvent(const std::u16string& name) : Event(name) {}

        [[nodiscard]] EventType GetType() const override { return Type; }

        [[nodiscard]] int32_t GetLeaderEntityId() const { return leaderEntityId; }
        void SetLeaderEntityId(const int32_t leaderEntityId) { this->leaderEntityId = leaderEntityId; }

        [[nodiscard]] int32_t GetSUnk04() const { return sUnk04; }
        void SetSUnk04(const int32_t sUnk04) { this->sUnk04 = sUnk04; }

        [[nodiscard]] int32_t GetSUnk08() const { return sUnk08; }
        void SetSUnk08(const int32_t sUnk08) { this->sUnk08 = sUnk08; }

        [[nodiscard]] int32_t GetSUnk0C() const { return sUnk0C; }
        void SetSUnk0C(const int32_t sUnk0C) { this->sUnk0C = sUnk0C; }

        [[nodiscard]] int32_t GetLeaderRegionEntityId() const { return leaderRegionEntityId; }
        void SetLeaderRegionEntityId(const int32_t leaderRegionEntityId)
        {
            this->leaderRegionEntityId = leaderRegionEntityId;
        }

        [[nodiscard]] int32_t GetGuest1RegionEntityId() const { return guest1RegionEntityId; }
        void SetGuest1RegionEntityId(const int32_t guest1RegionEntityId)
        {
            this->guest1RegionEntityId = guest1RegionEntityId;
        }

        [[nodiscard]] int32_t GetGuest2RegionEntityId() const { return guest2RegionEntityId; }
        void SetGuest2RegionEntityId(const int32_t guest2RegionEntityId)
        {
            this->guest2RegionEntityId = guest2RegionEntityId;
        }

        [[nodiscard]] int32_t GetSUnk1C() const { return sUnk1C; }
        void SetSUnk1C(const int32_t sUnk1C) { this->sUnk1C = sUnk1C; }

        [[nodiscard]] int32_t GetSUnk20() const { return sUnk20; }
        void SetSUnk20(const int32_t sUnk20) { this->sUnk20 = sUnk20; }

        [[nodiscard]] int32_t GetSUnk24() const { return sUnk24; }
        void SetSUnk24(const int32_t sUnk24) { this->sUnk24 = sUnk24; }

        [[nodiscard]] int32_t GetSUnk28() const { return sUnk28; }
        void SetSUnk28(const int32_t sUnk28) { this->sUnk28 = sUnk28; }

    private:
        int32_t leaderEntityId = 0;
        int32_t sUnk04 = 0;
        int32_t sUnk08 = 0;
        int32_t sUnk0C = 0;
        int32_t leaderRegionEntityId = 0;
        int32_t guest1RegionEntityId = 0;
        int32_t guest2RegionEntityId = 0;
        int32_t sUnk1C = 0;
        int32_t sUnk20 = 0;
        int32_t sUnk24 = 0;
        int32_t sUnk28 = 0;

        bool DeserializeSubtypeData(std::ifstream& stream) override;
        bool SerializeSubtypeData(std::ofstream& stream) const override;
    };

    class FIRELINK_ER_API OtherEvent final : public Event
    {
    public:
        static constexpr auto Type = EventType::Other;

        OtherEvent() = default;

        explicit OtherEvent(const std::u16string& name) : Event(name) {}

        [[nodiscard]] EventType GetType() const override { return Type; }

        // No data.
        // Will probably use `subtypeIndexOverride`.
    };
}
