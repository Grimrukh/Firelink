#pragma once

#include <cstdint>


/// @brief Type enumeration for MSB Models. Not all numeric values are used.
enum class ModelType : uint32_t
{
    MapPiece = 0,
    Character = 2,
    Player = 4,
    Collision = 5,
    Asset = 10
};


/// @brief Type enumeration for MSB Events. Not all numeric values are used.
enum class EventType : uint32_t
{
    Treasure = 4,
    Spawner = 5,
    ObjAct = 7,
    Navigation = 10,
    NPCInvasion = 12,
    Platoon = 15,
    PatrolRoute = 20,  // NOTE: be wary of Region subtype with same name
    Mount = 21,
    SignPool = 23,
    RetryPoint = 24,
    AreaTeam = 25,
    Other = 0xFFFFFFFF,
};


/// @brief Type enumeration for MSB Regions. Not all numeric values are used.
enum class RegionType : uint32_t
{
    InvasionPoint = 1,
    EnvironmentMapPoint = 2,
    Sound = 4,
    VFX = 5,
    WindVFX = 6,
    SpawnPoint = 8,
    Message = 9,
    EnvironmentMapEffectBox = 17,
    WindArea = 18,
    Connection = 21,
    PatrolRoute22 = 22,
    BuddySummonPoint = 26,
    MufflingBox = 28,
    MufflingPortal = 29,
    OtherSound = 30,
    MufflingPlane = 31,
    PatrolRoute = 32,  // NOTE: be wary of Event subtype with same name
    MapPoint = 33,
    WeatherOverride = 35,
    AutoDrawGroupPoint = 36,
    GroupDefeatReward = 37,
    MapPointDiscoveryOverride = 38,
    MapPointParticipationOverride = 39,
    Hitset = 40,
    FastTravelRestriction = 41,
    WeatherCreateAssetPoint = 42,
    PlayArea = 43,
    EnvironmentMapOutput = 44,
    MountJump = 46,
    Dummy = 48,
    FallPreventionRemoval = 49,
    NavmeshCutting = 50,
    MapNameOverride = 51,
    MountJumpFall = 52,
    HorseRideOverride = 53,
    Other = 0xFFFFFFFF,
};


/// @brief Type enumeration for MSB Parts. Not all numeric values are used.
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


/// @brief Dummy enum for unused `LayerParam` entries.
enum class LayerType : uint32_t
{
    None = 0
};


/// @brief Type enumeration for MSB Routes.
enum class RouteType : uint32_t
{
    MufflingPortalLink = 3,
    MufflingBoxLink = 4,
    Other = 0xFFFFFFFF,
};
