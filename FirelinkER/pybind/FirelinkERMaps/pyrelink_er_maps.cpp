// pyrelink_er_maps.cpp — pybind11 bindings for FirelinkERMaps.
//
// Exposes the C++ Elden Ring MSB to Python for manipulation.

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>

#include <FirelinkCore/BinaryReadWrite.h>
#include <FirelinkCore/Collections.h>
#include <FirelinkCore/DCX.h>
#include <FirelinkCore/MemoryUtils.h>
#include <FirelinkERMaps/MapStudio/MSB.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace py = pybind11;
using namespace Firelink;
using namespace Firelink::EldenRing::Maps::MapStudio;


// ============================================================================
// Helpers
// ============================================================================

/// Return a Python tuple from a Vector3.
static py::tuple vec3_to_tuple(const Vector3& v)
{
    return py::make_tuple(v.x, v.y, v.z);
}

/// Build a Vector3 from a Python sequence.
static Vector3 tuple_to_vec3(const py::object& obj)
{
    auto seq = obj.cast<py::sequence>();
    return Vector3{seq[0].cast<float>(), seq[1].cast<float>(), seq[2].cast<float>()};
}

// ============================================================================
// GroupBitSet binding helper
// ============================================================================

template <std::size_t N>
static void bind_group_bitset(py::module_& m, const char* name)
{
    using GBS = GroupBitSet<N>;
    py::class_<GBS>(m, name)
        .def(py::init<>())
        .def(py::init<const std::set<int>&>(), py::arg("bits"))
        .def_static("all_on", &GBS::AllOn)
        .def_static("all_off", &GBS::AllOff)
        .def("enable", &GBS::Enable, py::arg("index"))
        .def("disable", &GBS::Disable, py::arg("index"))
        .def("__getitem__", [](const GBS& self, std::size_t i) { return self[i]; })
        .def("to_list", &GBS::ToSortedBitList)
        .def("__repr__", [](const GBS& self) { return static_cast<std::string>(self); })
        .def("__eq__", &GBS::operator==)
        .def("__ne__", &GBS::operator!=);
}

// ============================================================================
// Shape binding helpers
// ============================================================================

static void bind_shapes(py::module_& m)
{
    py::class_<Shape>(m, "Shape")
        .def_property_readonly("shape_type_name", &Shape::GetShapeTypeName)
        .def_property_readonly("has_shape_data", &Shape::HasShapeData);

    py::class_<Point, Shape>(m, "PointShape")
        .def(py::init<>());

    py::class_<Circle, Shape>(m, "CircleShape")
        .def(py::init<>())
        .def(py::init<float>(), py::arg("radius"))
        .def_property("radius", &Circle::GetRadius, &Circle::SetRadius);

    py::class_<Sphere, Shape>(m, "SphereShape")
        .def(py::init<>())
        .def(py::init<float>(), py::arg("radius"))
        .def_property("radius", &Sphere::GetRadius, &Sphere::SetRadius);

    py::class_<Cylinder, Shape>(m, "CylinderShape")
        .def(py::init<>())
        .def(py::init<float, float>(), py::arg("radius"), py::arg("height"))
        .def_property("radius", &Cylinder::GetRadius, &Cylinder::SetRadius)
        .def_property("height", &Cylinder::GetHeight, &Cylinder::SetHeight);

    py::class_<Firelink::EldenRing::Maps::MapStudio::Rectangle, Shape>(m, "RectangleShape")
        .def(py::init<>())
        .def(py::init<float, float>(), py::arg("width"), py::arg("depth"))
        .def_property("width", &Rectangle::GetWidth, &Rectangle::SetWidth)
        .def_property("depth", &Rectangle::GetDepth, &Rectangle::SetDepth);

    py::class_<Box, Shape>(m, "BoxShape")
        .def(py::init<>())
        .def(py::init<float, float, float>(), py::arg("width"), py::arg("depth"), py::arg("height"))
        .def_property("width", &Box::GetWidth, &Box::SetWidth)
        .def_property("depth", &Box::GetDepth, &Box::SetDepth)
        .def_property("height", &Box::GetHeight, &Box::SetHeight);

    py::class_<Composite, Shape>(m, "CompositeShape")
        .def(py::init<>());

    py::enum_<ShapeType>(m, "ShapeType")
        .value("Point", ShapeType::PointShape)
        .value("Circle", ShapeType::CircleShape)
        .value("Sphere", ShapeType::SphereShape)
        .value("Cylinder", ShapeType::CylinderShape)
        .value("Rectangle", ShapeType::RectangleShape)
        .value("Box", ShapeType::BoxShape)
        .value("Composite", ShapeType::CompositeShape);
}

// ============================================================================
// Part struct component bindings
// ============================================================================

static void bind_part_structs(py::module_& m)
{
    py::class_<DrawInfo1>(m, "DrawInfo1")
        .def(py::init<>())
        .def_readwrite("display_groups", &DrawInfo1::displayGroups)
        .def_readwrite("draw_groups", &DrawInfo1::drawGroups)
        .def_readwrite("collision_mask", &DrawInfo1::collisionMask)
        .def_readwrite("condition_1a", &DrawInfo1::condition1A)
        .def_readwrite("condition_1b", &DrawInfo1::condition1B)
        .def_readwrite("unk_c2", &DrawInfo1::unkC2)
        .def_readwrite("unk_c3", &DrawInfo1::unkC3)
        .def_readwrite("unk_c4", &DrawInfo1::unkC4)
        .def_readwrite("unk_c6", &DrawInfo1::unkC6);

    py::class_<DrawInfo2>(m, "DrawInfo2")
        .def(py::init<>())
        .def_readwrite("condition_2", &DrawInfo2::condition2)
        .def_readwrite("display_groups_2", &DrawInfo2::displayGroups2)
        .def_readwrite("unk_24", &DrawInfo2::unk24)
        .def_readwrite("unk_26", &DrawInfo2::unk26);

    py::class_<GParam>(m, "GParam")
        .def(py::init<>())
        .def_readwrite("light_set_id", &GParam::lightSetId)
        .def_readwrite("fog_id", &GParam::fogId)
        .def_readwrite("light_scattering_id", &GParam::lightScatteringId)
        .def_readwrite("environment_map_id", &GParam::environmentMapId);

    py::class_<SceneGParam>(m, "SceneGParam")
        .def(py::init<>())
        .def_readwrite("transition_time", &SceneGParam::transitionTime)
        .def_readwrite("unk_18", &SceneGParam::unk18)
        .def_readwrite("unk_19", &SceneGParam::unk19)
        .def_readwrite("unk_1a", &SceneGParam::unk1A)
        .def_readwrite("unk_1b", &SceneGParam::unk1B)
        .def_readwrite("unk_1c", &SceneGParam::unk1C)
        .def_readwrite("unk_1d", &SceneGParam::unk1D)
        .def_readwrite("unk_20", &SceneGParam::unk20)
        .def_readwrite("unk_21", &SceneGParam::unk21);

    py::class_<GrassConfig>(m, "GrassConfig")
        .def(py::init<>())
        .def_readwrite("unk_00", &GrassConfig::unk00)
        .def_readwrite("unk_04", &GrassConfig::unk04)
        .def_readwrite("unk_08", &GrassConfig::unk08)
        .def_readwrite("unk_0c", &GrassConfig::unk0C)
        .def_readwrite("unk_10", &GrassConfig::unk10)
        .def_readwrite("unk_14", &GrassConfig::unk14)
        .def_readwrite("unk_18", &GrassConfig::unk18);

    py::class_<UnkPartStruct8>(m, "UnkPartStruct8")
        .def(py::init<>())
        .def_readwrite("unk_00", &UnkPartStruct8::unk00);

    py::class_<UnkPartStruct9>(m, "UnkPartStruct9")
        .def(py::init<>())
        .def_readwrite("unk_00", &UnkPartStruct9::unk00);

    py::class_<TileLoadConfig>(m, "TileLoadConfig")
        .def(py::init<>())
        .def_readwrite("map_id", &TileLoadConfig::mapId)
        .def_readwrite("unk_04", &TileLoadConfig::unk04)
        .def_readwrite("unk_0c", &TileLoadConfig::unk0C)
        .def_readwrite("unk_10", &TileLoadConfig::unk10)
        .def_readwrite("unk_14", &TileLoadConfig::unk14);

    py::class_<UnkPartStruct11>(m, "UnkPartStruct11")
        .def(py::init<>())
        .def_readwrite("unk_00", &UnkPartStruct11::unk00)
        .def_readwrite("unk_04", &UnkPartStruct11::unk04);

    py::class_<ExtraAssetData1>(m, "ExtraAssetData1")
        .def(py::init<>())
        .def_readwrite("unk_00", &ExtraAssetData1::unk00)
        .def_readwrite("unk_04", &ExtraAssetData1::unk04)
        .def_readwrite("disable_torrent_asset_only", &ExtraAssetData1::disableTorrentAssetOnly)
        .def_readwrite("unk_1c", &ExtraAssetData1::unk1C)
        .def_readwrite("unk_24", &ExtraAssetData1::unk24)
        .def_readwrite("unk_26", &ExtraAssetData1::unk26)
        .def_readwrite("unk_28", &ExtraAssetData1::unk28)
        .def_readwrite("unk_2c", &ExtraAssetData1::unk2C);

    py::class_<ExtraAssetData2>(m, "ExtraAssetData2")
        .def(py::init<>())
        .def_readwrite("unk_00", &ExtraAssetData2::unk00)
        .def_readwrite("unk_04", &ExtraAssetData2::unk04)
        .def_readwrite("unk_14", &ExtraAssetData2::unk14)
        .def_readwrite("unk_1c", &ExtraAssetData2::unk1C)
        .def_readwrite("unk_1d", &ExtraAssetData2::unk1D)
        .def_readwrite("unk_1e", &ExtraAssetData2::unk1E)
        .def_readwrite("unk_1f", &ExtraAssetData2::unk1F);

    py::class_<ExtraAssetData3>(m, "ExtraAssetData3")
        .def(py::init<>())
        .def_readwrite("unk_00", &ExtraAssetData3::unk00)
        .def_readwrite("unk_04", &ExtraAssetData3::unk04)
        .def_readwrite("unk_09", &ExtraAssetData3::unk09)
        .def_readwrite("unk_0a", &ExtraAssetData3::unk0A)
        .def_readwrite("unk_0b", &ExtraAssetData3::unk0B)
        .def_readwrite("unk_0c", &ExtraAssetData3::unk0C)
        .def_readwrite("unk_0e", &ExtraAssetData3::unk0E)
        .def_readwrite("unk_10", &ExtraAssetData3::unk10)
        .def_readwrite("unk_18", &ExtraAssetData3::unk18)
        .def_readwrite("unk_1c", &ExtraAssetData3::unk1C)
        .def_readwrite("unk_20", &ExtraAssetData3::unk20)
        .def_readwrite("unk_24", &ExtraAssetData3::unk24)
        .def_readwrite("unk_25", &ExtraAssetData3::unk25);

    py::class_<ExtraAssetData4>(m, "ExtraAssetData4")
        .def(py::init<>())
        .def_readwrite("unk_00", &ExtraAssetData4::unk00)
        .def_readwrite("unk_01", &ExtraAssetData4::unk01)
        .def_readwrite("unk_02", &ExtraAssetData4::unk02)
        .def_readwrite("unk_03", &ExtraAssetData4::unk03);
}

// ============================================================================
// Macro: define properties common to all Entry subtypes
// ============================================================================

#define DEF_ENTRY_COMMON(PyClass)                                                                     \
    .def_property("name",                                                                             \
        [](const PyClass& self) { return self.GetNameUTF8(); },                                          \
        [](PyClass& self, const std::string& name) { self.SetName(UTF8ToUTF16(name)); })                 \
    .def("__repr__", [](const PyClass& self) { return static_cast<std::string>(self); })

// ============================================================================
// Module definition
// ============================================================================

PYBIND11_MODULE(_pyrelink_er_maps, m)
{
    m.doc() = "C++ Elden Ring MSB (MapStudio) bindings";

    // ---- Utility types ------------------------------------------------
    bind_group_bitset<128>(m, "GroupBitSet128");
    bind_group_bitset<256>(m, "GroupBitSet256");
    bind_group_bitset<1024>(m, "GroupBitSet1024");

    bind_shapes(m);
    bind_part_structs(m);

    // ---- Enums --------------------------------------------------------
    py::enum_<ModelType>(m, "ModelType")
        .value("MapPiece", ModelType::MapPiece)
        .value("Character", ModelType::Character)
        .value("Player", ModelType::Player)
        .value("Collision", ModelType::Collision)
        .value("Asset", ModelType::Asset);

    py::enum_<EventType>(m, "EventType")
        .value("Treasure", EventType::Treasure)
        .value("Spawner", EventType::Spawner)
        .value("ObjAct", EventType::ObjAct)
        .value("Navigation", EventType::Navigation)
        .value("NPCInvasion", EventType::NPCInvasion)
        .value("Platoon", EventType::Platoon)
        .value("PatrolRoute", EventType::PatrolRoute)
        .value("Mount", EventType::Mount)
        .value("SignPool", EventType::SignPool)
        .value("RetryPoint", EventType::RetryPoint)
        .value("AreaTeam", EventType::AreaTeam)
        .value("Other", EventType::Other);

    py::enum_<RegionType>(m, "RegionType")
        .value("InvasionPoint", RegionType::InvasionPoint)
        .value("EnvironmentMapPoint", RegionType::EnvironmentMapPoint)
        .value("Sound", RegionType::Sound)
        .value("VFX", RegionType::VFX)
        .value("WindVFX", RegionType::WindVFX)
        .value("SpawnPoint", RegionType::SpawnPoint)
        .value("Message", RegionType::Message)
        .value("EnvironmentMapEffectBox", RegionType::EnvironmentMapEffectBox)
        .value("WindArea", RegionType::WindArea)
        .value("Connection", RegionType::Connection)
        .value("PatrolRoute22", RegionType::PatrolRoute22)
        .value("BuddySummonPoint", RegionType::BuddySummonPoint)
        .value("MufflingBox", RegionType::MufflingBox)
        .value("MufflingPortal", RegionType::MufflingPortal)
        .value("OtherSound", RegionType::OtherSound)
        .value("MufflingPlane", RegionType::MufflingPlane)
        .value("PatrolRoute", RegionType::PatrolRoute)
        .value("MapPoint", RegionType::MapPoint)
        .value("WeatherOverride", RegionType::WeatherOverride)
        .value("AutoDrawGroupPoint", RegionType::AutoDrawGroupPoint)
        .value("GroupDefeatReward", RegionType::GroupDefeatReward)
        .value("MapPointDiscoveryOverride", RegionType::MapPointDiscoveryOverride)
        .value("MapPointParticipationOverride", RegionType::MapPointParticipationOverride)
        .value("Hitset", RegionType::Hitset)
        .value("FastTravelRestriction", RegionType::FastTravelRestriction)
        .value("WeatherCreateAssetPoint", RegionType::WeatherCreateAssetPoint)
        .value("PlayArea", RegionType::PlayArea)
        .value("EnvironmentMapOutput", RegionType::EnvironmentMapOutput)
        .value("MountJump", RegionType::MountJump)
        .value("Dummy", RegionType::Dummy)
        .value("FallPreventionRemoval", RegionType::FallPreventionRemoval)
        .value("NavmeshCutting", RegionType::NavmeshCutting)
        .value("MapNameOverride", RegionType::MapNameOverride)
        .value("MountJumpFall", RegionType::MountJumpFall)
        .value("HorseRideOverride", RegionType::HorseRideOverride)
        .value("Other", RegionType::Other);

    py::enum_<PartType>(m, "PartType")
        .value("MapPiece", PartType::MapPiece)
        .value("Character", PartType::Character)
        .value("PlayerStart", PartType::PlayerStart)
        .value("Collision", PartType::Collision)
        .value("DummyAsset", PartType::DummyAsset)
        .value("DummyCharacter", PartType::DummyCharacter)
        .value("ConnectCollision", PartType::ConnectCollision)
        .value("Asset", PartType::Asset);

    py::enum_<RouteType>(m, "RouteType")
        .value("MufflingPortalLink", RouteType::MufflingPortalLink)
        .value("MufflingBoxLink", RouteType::MufflingBoxLink)
        .value("Other", RouteType::Other);

    py::enum_<CollisionHitFilter>(m, "CollisionHitFilter")
        .value("Normal", CollisionHitFilter::Normal)
        .value("CameraOnly", CollisionHitFilter::CameraOnly)
        .value("NPCOnly", CollisionHitFilter::NPCOnly)
        .value("FallDeathCam", CollisionHitFilter::FallDeathCam)
        .value("Kill", CollisionHitFilter::Kill)
        .value("Unk16", CollisionHitFilter::Unk16)
        .value("Unk17", CollisionHitFilter::Unk17)
        .value("Unk19", CollisionHitFilter::Unk19)
        .value("Unk20", CollisionHitFilter::Unk20)
        .value("Unk22", CollisionHitFilter::Unk22)
        .value("Unk23", CollisionHitFilter::Unk23)
        .value("Unk24", CollisionHitFilter::Unk24)
        .value("Unk29", CollisionHitFilter::Unk29);

    py::enum_<HorseRideOverrideRegion::HorseRideOverrideType>(m, "HorseRideOverrideType")
        .value("Default", HorseRideOverrideRegion::HorseRideOverrideType::Default)
        .value("Prevent", HorseRideOverrideRegion::HorseRideOverrideType::Prevent)
        .value("Allow", HorseRideOverrideRegion::HorseRideOverrideType::Allow);

    py::enum_<Firelink::DCXType>(m, "DCXType")
        .value("Unknown", Firelink::DCXType::Unknown)
        .value("Null", Firelink::DCXType::Null)
        .value("Zlib", Firelink::DCXType::Zlib)
        .value("DCP_EDGE", Firelink::DCXType::DCP_EDGE)
        .value("DCP_DFLT", Firelink::DCXType::DCP_DFLT)
        .value("DCX_EDGE", Firelink::DCXType::DCX_EDGE)
        .value("DCX_DFLT_10000_24_9", Firelink::DCXType::DCX_DFLT_10000_24_9)
        .value("DCX_DFLT_10000_44_9", Firelink::DCXType::DCX_DFLT_10000_44_9)
        .value("DCX_DFLT_11000_44_8", Firelink::DCXType::DCX_DFLT_11000_44_8)
        .value("DCX_DFLT_11000_44_9", Firelink::DCXType::DCX_DFLT_11000_44_9)
        .value("DCX_DFLT_11000_44_9_15", Firelink::DCXType::DCX_DFLT_11000_44_9_15)
        .value("DCX_KRAK", Firelink::DCXType::DCX_KRAK)
        .value("DCX_ZSTD", Firelink::DCXType::DCX_ZSTD);

    // ====================================================================
    // MODELS
    // ====================================================================

    auto pyModel = py::class_<Model>(m, "Model")
        DEF_ENTRY_COMMON(Model)
        .def_property_readonly("model_type", &Model::GetType);

    py::class_<MapPieceModel, Model>(m, "MapPieceModel")
        .def(py::init<>())
        .def(py::init([](const std::string& name) { return std::make_unique<MapPieceModel>(UTF8ToUTF16(name)); }),
            py::arg("name"));

    py::class_<AssetModel, Model>(m, "AssetModel")
        .def(py::init<>())
        .def(py::init([](const std::string& name) { return std::make_unique<AssetModel>(UTF8ToUTF16(name)); }),
            py::arg("name"));

    py::class_<CharacterModel, Model>(m, "CharacterModel")
        .def(py::init<>())
        .def(py::init([](const std::string& name) { return std::make_unique<CharacterModel>(UTF8ToUTF16(name)); }),
            py::arg("name"));

    py::class_<PlayerModel, Model>(m, "PlayerModel")
        .def(py::init<>())
        .def(py::init([](const std::string& name) { return std::make_unique<PlayerModel>(UTF8ToUTF16(name)); }),
            py::arg("name"));

    py::class_<CollisionModel, Model>(m, "CollisionModel")
        .def(py::init<>())
        .def(py::init([](const std::string& name) { return std::make_unique<CollisionModel>(UTF8ToUTF16(name)); }),
            py::arg("name"));

    // ====================================================================
    // EVENTS
    // ====================================================================

    auto pyEvent = py::class_<Event>(m, "Event")
        DEF_ENTRY_COMMON(Event)
        .def_property_readonly("event_type", &Event::GetType)
        .def_property("entity_id", &Event::GetEntityId, &Event::SetEntityId)
        .def_property("attached_part",
            [](const Event& self) -> Part* { return self.GetAttachedPart(); },
            [](Event& self, Part* p) { self.SetAttachedPart(p); },
            py::return_value_policy::reference)
        .def_property("attached_region",
            [](const Event& self) -> Region* { return self.GetAttachedRegion(); },
            [](Event& self, Region* r) { self.SetAttachedRegion(r); },
            py::return_value_policy::reference)
        .def_property("h_unk_14", &Event::GetHUnk14, &Event::SetHUnk14)
        .def_property("d_unk_0c", &Event::GetDUnk0C, &Event::SetDUnk0C)
        .def_property("map_id", &Event::GetMapId, &Event::SetMapId)
        .def_property("e_unk_04", &Event::GetEUnk04, &Event::SetEUnk04)
        .def_property("e_unk_08", &Event::GetEUnk08, &Event::SetEUnk08)
        .def_property("e_unk_0c", &Event::GetEUnk0C, &Event::SetEUnk0C);

    py::class_<TreasureEvent, Event>(m, "TreasureEvent")
        .def(py::init<>())
        .def(py::init([](const std::string& n) { return std::make_unique<TreasureEvent>(UTF8ToUTF16(n)); }),
            py::arg("name"))
        .def_property("treasure_part",
            [](const TreasureEvent& self) -> Part* { return self.GetTreasurePart(); },
            [](TreasureEvent& self, Part* p) { self.SetTreasurePart(p); },
            py::return_value_policy::reference)
        .def_property("item_lot", &TreasureEvent::GetItemLot, &TreasureEvent::SetItemLot)
        .def_property("action_button_id", &TreasureEvent::GetActionButtonId, &TreasureEvent::SetActionButtonId)
        .def_property("pickup_animation_id", &TreasureEvent::GetPickupAnimationId, &TreasureEvent::SetPickupAnimationId)
        .def_property("in_chest", &TreasureEvent::isInChest, &TreasureEvent::SetInChest)
        .def_property("starts_disabled", &TreasureEvent::isStartsDisabled, &TreasureEvent::SetStartsDisabled);

    py::class_<SpawnerEvent, Event>(m, "SpawnerEvent")
        .def(py::init<>())
        .def(py::init([](const std::string& n) { return std::make_unique<SpawnerEvent>(UTF8ToUTF16(n)); }),
            py::arg("name"))
        .def_property("max_count", &SpawnerEvent::GetMaxCount, &SpawnerEvent::SetMaxCount)
        .def_property("spawner_type", &SpawnerEvent::GetSpawnerType, &SpawnerEvent::SetSpawnerType)
        .def_property("limit_count", &SpawnerEvent::GetLimitCount, &SpawnerEvent::SetLimitCount)
        .def_property("min_spawner_count", &SpawnerEvent::GetMinSpawnerCount, &SpawnerEvent::SetMinSpawnerCount)
        .def_property("max_spawner_count", &SpawnerEvent::GetMaxSpawnerCount, &SpawnerEvent::SetMaxSpawnerCount)
        .def_property("min_interval", &SpawnerEvent::GetMinInterval, &SpawnerEvent::SetMinInterval)
        .def_property("max_interval", &SpawnerEvent::GetMaxInterval, &SpawnerEvent::SetMaxInterval)
        .def_property("initial_spawn_count", &SpawnerEvent::GetInitialSpawnCount, &SpawnerEvent::SetInitialSpawnCount)
        .def_property("spawner_unk_14", &SpawnerEvent::GetSpawnerUnk14, &SpawnerEvent::SetSpawnerUnk14)
        .def_property("spawner_unk_18", &SpawnerEvent::GetSpawnerUnk18, &SpawnerEvent::SetSpawnerUnk18)
        .def("get_spawn_part", &SpawnerEvent::GetSpawnPartAtIndex, py::arg("index"),
            py::return_value_policy::reference)
        .def("set_spawn_part", static_cast<void(SpawnerEvent::*)(int, Part*)>(&SpawnerEvent::SetSpawnPartAtIndex),
            py::arg("index"), py::arg("part"))
        .def("get_spawn_region", &SpawnerEvent::GetSpawnRegionAtIndex, py::arg("index"),
            py::return_value_policy::reference)
        .def("set_spawn_region", static_cast<void(SpawnerEvent::*)(int, Region*)>(&SpawnerEvent::SetSpawnRegionAtIndex),
            py::arg("index"), py::arg("region"));

    py::class_<NavigationEvent, Event>(m, "NavigationEvent")
        .def(py::init<>())
        .def(py::init([](const std::string& n) { return std::make_unique<NavigationEvent>(UTF8ToUTF16(n)); }),
            py::arg("name"))
        .def_property("navigation_region",
            [](const NavigationEvent& self) -> Region* { return self.GetNavigationRegion(); },
            [](NavigationEvent& self, Region* r) { self.SetNavigationRegion(r); },
            py::return_value_policy::reference);

    py::class_<ObjActEvent, Event>(m, "ObjActEvent")
        .def(py::init<>())
        .def(py::init([](const std::string& n) { return std::make_unique<ObjActEvent>(UTF8ToUTF16(n)); }),
            py::arg("name"))
        .def_property("obj_act_entity_id", &ObjActEvent::GetObjActEntityId, &ObjActEvent::SetObjActEntityId)
        .def_property("obj_act_part",
            [](const ObjActEvent& self) -> Part* { return self.GetObjActPart(); },
            [](ObjActEvent& self, Part* p) { self.SetObjActPart(p); },
            py::return_value_policy::reference)
        .def_property("obj_act_param_id", &ObjActEvent::GetObjActParamId, &ObjActEvent::SetObjActParamId)
        .def_property("obj_act_state", &ObjActEvent::GetObjActState, &ObjActEvent::SetObjActState)
        .def_property("obj_act_flag", &ObjActEvent::GetObjActFlag, &ObjActEvent::SetObjActFlag);

    py::class_<NPCInvasionEvent, Event>(m, "NPCInvasionEvent")
        .def(py::init<>())
        .def(py::init([](const std::string& n) { return std::make_unique<NPCInvasionEvent>(UTF8ToUTF16(n)); }),
            py::arg("name"))
        .def_property("host_entity_id", &NPCInvasionEvent::GetHostEntityId, &NPCInvasionEvent::SetHostEntityId)
        .def_property("invasion_flag_id", &NPCInvasionEvent::GetInvasionFlagId, &NPCInvasionEvent::SetInvasionFlagId)
        .def_property("activate_good_id", &NPCInvasionEvent::GetActivateGoodId, &NPCInvasionEvent::SetActivateGoodId)
        .def_property("unk_0c", &NPCInvasionEvent::GetUnk0c, &NPCInvasionEvent::SetUnk0c)
        .def_property("unk_10", &NPCInvasionEvent::GetUnk10, &NPCInvasionEvent::SetUnk10)
        .def_property("unk_14", &NPCInvasionEvent::GetUnk14, &NPCInvasionEvent::SetUnk14)
        .def_property("unk_18", &NPCInvasionEvent::GetUnk18, &NPCInvasionEvent::SetUnk18)
        .def_property("unk_1c", &NPCInvasionEvent::GetUnk1c, &NPCInvasionEvent::SetUnk1c);

    py::class_<PlatoonEvent, Event>(m, "PlatoonEvent")
        .def(py::init<>())
        .def(py::init([](const std::string& n) { return std::make_unique<PlatoonEvent>(UTF8ToUTF16(n)); }),
            py::arg("name"))
        .def_property("platoon_id_script_activate", &PlatoonEvent::GetPlatoonIdScriptActivate,
            &PlatoonEvent::SetPlatoonIdScriptActivate)
        .def_property("state", &PlatoonEvent::GetState, &PlatoonEvent::SetState)
        .def("get_platoon_part", &PlatoonEvent::GetPlatoonPartAtIndex, py::arg("index"),
            py::return_value_policy::reference)
        .def("set_platoon_part",
            static_cast<void(PlatoonEvent::*)(int, Part*)>(&PlatoonEvent::SetPlatoonPartAtIndex),
            py::arg("index"), py::arg("part"));

    py::class_<PatrolRouteEvent, Event>(m, "PatrolRouteEvent")
        .def(py::init<>())
        .def(py::init([](const std::string& n) { return std::make_unique<PatrolRouteEvent>(UTF8ToUTF16(n)); }),
            py::arg("name"))
        .def_property("patrol_type", &PatrolRouteEvent::GetPatrolType, &PatrolRouteEvent::SetPatrolType)
        .def("get_patrol_region", &PatrolRouteEvent::GetPatrolRegionAtIndex, py::arg("index"),
            py::return_value_policy::reference)
        .def("set_patrol_region",
            static_cast<void(PatrolRouteEvent::*)(int, Region*)>(&PatrolRouteEvent::SetPatrolRegionAtIndex),
            py::arg("index"), py::arg("region"));

    py::class_<MountEvent, Event>(m, "MountEvent")
        .def(py::init<>())
        .def(py::init([](const std::string& n) { return std::make_unique<MountEvent>(UTF8ToUTF16(n)); }),
            py::arg("name"))
        .def_property("rider_part",
            [](const MountEvent& self) -> Part* { return self.GetRiderPart(); },
            [](MountEvent& self, Part* p) { self.SetRiderPart(p); },
            py::return_value_policy::reference)
        .def_property("mount_part",
            [](const MountEvent& self) -> Part* { return self.GetMountPart(); },
            [](MountEvent& self, Part* p) { self.SetMountPart(p); },
            py::return_value_policy::reference);

    py::class_<SignPoolEvent, Event>(m, "SignPoolEvent")
        .def(py::init<>())
        .def(py::init([](const std::string& n) { return std::make_unique<SignPoolEvent>(UTF8ToUTF16(n)); }),
            py::arg("name"))
        .def_property("sign_part",
            [](const SignPoolEvent& self) -> Part* { return self.GetSignPart(); },
            [](SignPoolEvent& self, Part* p) { self.SetSignPart(p); },
            py::return_value_policy::reference)
        .def_property("sign_pool_unk_04", &SignPoolEvent::GetSignPoolUnk04, &SignPoolEvent::SetSignPoolUnk04);

    py::class_<RetryPointEvent, Event>(m, "RetryPointEvent")
        .def(py::init<>())
        .def(py::init([](const std::string& n) { return std::make_unique<RetryPointEvent>(UTF8ToUTF16(n)); }),
            py::arg("name"))
        .def_property("retry_part",
            [](const RetryPointEvent& self) -> Part* { return self.GetRetryPart(); },
            [](RetryPointEvent& self, Part* p) { self.SetRetryPart(p); },
            py::return_value_policy::reference)
        .def_property("event_flag_id", &RetryPointEvent::GetEventFlagId, &RetryPointEvent::SetEventFlagId)
        .def_property("retry_point_unk_08", &RetryPointEvent::GetRetryPointUnk08,
            &RetryPointEvent::SetRetryPointUnk08)
        .def_property("retry_region",
            [](const RetryPointEvent& self) -> Region* { return self.GetRetryRegion(); },
            [](RetryPointEvent& self, Region* r) { self.SetRetryRegion(r); },
            py::return_value_policy::reference);

    py::class_<AreaTeamEvent, Event>(m, "AreaTeamEvent")
        .def(py::init<>())
        .def(py::init([](const std::string& n) { return std::make_unique<AreaTeamEvent>(UTF8ToUTF16(n)); }),
            py::arg("name"))
        .def_property("leader_entity_id", &AreaTeamEvent::GetLeaderEntityId, &AreaTeamEvent::SetLeaderEntityId)
        .def_property("s_unk_04", &AreaTeamEvent::GetSUnk04, &AreaTeamEvent::SetSUnk04)
        .def_property("s_unk_08", &AreaTeamEvent::GetSUnk08, &AreaTeamEvent::SetSUnk08)
        .def_property("s_unk_0c", &AreaTeamEvent::GetSUnk0C, &AreaTeamEvent::SetSUnk0C)
        .def_property("leader_region_entity_id", &AreaTeamEvent::GetLeaderRegionEntityId,
            &AreaTeamEvent::SetLeaderRegionEntityId)
        .def_property("guest_1_region_entity_id", &AreaTeamEvent::GetGuest1RegionEntityId,
            &AreaTeamEvent::SetGuest1RegionEntityId)
        .def_property("guest_2_region_entity_id", &AreaTeamEvent::GetGuest2RegionEntityId,
            &AreaTeamEvent::SetGuest2RegionEntityId)
        .def_property("s_unk_1c", &AreaTeamEvent::GetSUnk1C, &AreaTeamEvent::SetSUnk1C)
        .def_property("s_unk_20", &AreaTeamEvent::GetSUnk20, &AreaTeamEvent::SetSUnk20)
        .def_property("s_unk_24", &AreaTeamEvent::GetSUnk24, &AreaTeamEvent::SetSUnk24)
        .def_property("s_unk_28", &AreaTeamEvent::GetSUnk28, &AreaTeamEvent::SetSUnk28);

    py::class_<OtherEvent, Event>(m, "OtherEvent")
        .def(py::init<>())
        .def(py::init([](const std::string& n) { return std::make_unique<OtherEvent>(UTF8ToUTF16(n)); }),
            py::arg("name"));

    // ====================================================================
    // REGIONS
    // ====================================================================

    auto pyRegion = py::class_<Region>(m, "Region")
        DEF_ENTRY_COMMON(Region)
        .def_property_readonly("region_type", &Region::GetType)
        .def_property("entity_id",
            [](const Region& self) { return self.GetEntityId(); },
            [](Region& self, uint32_t id) { self.SetEntityId(id); })
        .def_property("translate",
            [](const Region& self) { return vec3_to_tuple(self.GetTranslate()); },
            [](Region& self, const py::object& v) { self.SetTranslate(tuple_to_vec3(v)); })
        .def_property("rotate",
            [](const Region& self) { return vec3_to_tuple(self.GetRotate()); },
            [](Region& self, const py::object& v) { self.SetRotate(tuple_to_vec3(v)); })
        .def_property("h_unk_40", &Region::GetHUnk40, &Region::SetHUnk40)
        .def_property("event_layer", &Region::GetEventLayer, &Region::SetEventLayer)
        .def_property("attached_part",
            [](const Region& self) -> Part* { return self.GetAttachedPart(); },
            [](Region& self, Part* p) { self.SetAttachedPart(p); },
            py::return_value_policy::reference)
        .def_property("d_unk_08", &Region::GetDUnk08, &Region::SetDUnk08)
        .def_property("map_id", &Region::GetMapId, &Region::SetMapId)
        .def_property("e_unk_04", &Region::GetEUnk04, &Region::SetEUnk04)
        .def_property("e_unk_0c", &Region::GetEUnk0C, &Region::SetEUnk0C)
        .def_property_readonly("shape_type", &Region::GetShapeType)
        .def_property_readonly("shape_type_name", &Region::GetShapeTypeName)
        .def("set_shape_type", &Region::SetShapeType, py::arg("shape_type"),
            py::return_value_policy::reference_internal,
            "Set the shape type for this region, creating a new shape instance.")
        .def_property_readonly("shape",
            [](Region& self) -> Shape* { return self.GetShape().get(); },
            py::return_value_policy::reference_internal);

    #define DEF_REGION_SUBTYPE_EMPTY(CppClass, PyName)        \
        py::class_<CppClass, Region>(m, PyName)               \
            .def(py::init<>())                                \
            .def(py::init([](const std::string& n) {          \
                return std::make_unique<CppClass>(UTF8ToUTF16(n)); \
            }), py::arg("name"))

    py::class_<InvasionPointRegion, Region>(m, "InvasionPointRegion")
        .def(py::init<>())
        .def(py::init([](const std::string& n) {
            return std::make_unique<InvasionPointRegion>(UTF8ToUTF16(n)); }), py::arg("name"))
        .def_property("priority", &InvasionPointRegion::GetPriority, &InvasionPointRegion::SetPriority);

    py::class_<EnvironmentMapPointRegion, Region>(m, "EnvironmentMapPointRegion")
        .def(py::init<>())
        .def(py::init([](const std::string& n) {
            return std::make_unique<EnvironmentMapPointRegion>(UTF8ToUTF16(n)); }), py::arg("name"))
        .def_property("s_unk_00", &EnvironmentMapPointRegion::GetSUnk00, &EnvironmentMapPointRegion::SetSUnk00)
        .def_property("s_unk_04", &EnvironmentMapPointRegion::GetSUnk04, &EnvironmentMapPointRegion::SetSUnk04)
        .def_property("s_unk_0d", &EnvironmentMapPointRegion::IsSUnk0D, &EnvironmentMapPointRegion::SetSUnk0D)
        .def_property("s_unk_0e", &EnvironmentMapPointRegion::IsSUnk0E, &EnvironmentMapPointRegion::SetSUnk0E)
        .def_property("s_unk_0f", &EnvironmentMapPointRegion::IsSUnk0F, &EnvironmentMapPointRegion::SetSUnk0F)
        .def_property("s_unk_10", &EnvironmentMapPointRegion::GetSUnk10, &EnvironmentMapPointRegion::SetSUnk10)
        .def_property("s_unk_14", &EnvironmentMapPointRegion::GetSUnk14, &EnvironmentMapPointRegion::SetSUnk14)
        .def_property("s_map_id", &EnvironmentMapPointRegion::GetSMapId, &EnvironmentMapPointRegion::SetSMapId)
        .def_property("s_unk_20", &EnvironmentMapPointRegion::GetSUnk20, &EnvironmentMapPointRegion::SetSUnk20)
        .def_property("s_unk_24", &EnvironmentMapPointRegion::GetSUnk24, &EnvironmentMapPointRegion::SetSUnk24)
        .def_property("s_unk_28", &EnvironmentMapPointRegion::GetSUnk28, &EnvironmentMapPointRegion::SetSUnk28)
        .def_property("s_unk_2c", &EnvironmentMapPointRegion::GetSUnk2C, &EnvironmentMapPointRegion::SetSUnk2C)
        .def_property("s_unk_2d", &EnvironmentMapPointRegion::GetSUnk2D, &EnvironmentMapPointRegion::SetSUnk2D);

    py::class_<SoundRegion, Region>(m, "SoundRegion")
        .def(py::init<>())
        .def(py::init([](const std::string& n) {
            return std::make_unique<SoundRegion>(UTF8ToUTF16(n)); }), py::arg("name"))
        .def_property("sound_type", &SoundRegion::GetSoundType, &SoundRegion::SetSoundType)
        .def_property("sound_id", &SoundRegion::GetSoundId, &SoundRegion::SetSoundId)
        .def_property("s_unk_49", &SoundRegion::IsSUnk49, &SoundRegion::SetSUnk49)
        .def("get_child_region", &SoundRegion::GetChildRegionAtIndex, py::arg("index"),
            py::return_value_policy::reference)
        .def("set_child_region",
            static_cast<void(SoundRegion::*)(int, Region*)>(&SoundRegion::SetChildRegionAtIndex),
            py::arg("index"), py::arg("region"));

    py::class_<VFXRegion, Region>(m, "VFXRegion")
        .def(py::init<>())
        .def(py::init([](const std::string& n) {
            return std::make_unique<VFXRegion>(UTF8ToUTF16(n)); }), py::arg("name"))
        .def_property("effect_id", &VFXRegion::GetEffectId, &VFXRegion::SetEffectId)
        .def_property("s_unk_04", &VFXRegion::GetSUnk04, &VFXRegion::SetSUnk04);

    py::class_<WindVFXRegion, Region>(m, "WindVFXRegion")
        .def(py::init<>())
        .def(py::init([](const std::string& n) {
            return std::make_unique<WindVFXRegion>(UTF8ToUTF16(n)); }), py::arg("name"))
        .def_property("effect_id", &WindVFXRegion::GetEffectId, &WindVFXRegion::SetEffectId)
        .def_property("wind_region",
            [](const WindVFXRegion& self) -> Region* { return self.GetWindRegion(); },
            [](WindVFXRegion& self, Region* r) { self.SetWindRegion(r); },
            py::return_value_policy::reference)
        .def_property("s_unk_08", &WindVFXRegion::GetSUnk08, &WindVFXRegion::SetSUnk08);

    DEF_REGION_SUBTYPE_EMPTY(SpawnPointRegion, "SpawnPointRegion");

    py::class_<MessageRegion, Region>(m, "MessageRegion")
        .def(py::init<>())
        .def(py::init([](const std::string& n) {
            return std::make_unique<MessageRegion>(UTF8ToUTF16(n)); }), py::arg("name"))
        .def_property("message_id", &MessageRegion::GetMessageId, &MessageRegion::SetMessageId)
        .def_property("s_unk_02", &MessageRegion::GetSUnk02, &MessageRegion::SetSUnk02)
        .def_property("hidden", &MessageRegion::IsHidden, &MessageRegion::SetHidden)
        .def_property("s_unk_08", &MessageRegion::GetSUnk08, &MessageRegion::SetSUnk08)
        .def_property("s_unk_0c", &MessageRegion::GetSUnk0C, &MessageRegion::SetSUnk0C)
        .def_property("enable_event_flag_id", &MessageRegion::GetEnableEventFlagId,
            &MessageRegion::SetEnableEventFlagId)
        .def_property("character_model_name", &MessageRegion::GetCharacterModelName,
            &MessageRegion::SetCharacterModelName)
        .def_property("character_id", &MessageRegion::GetCharacterId, &MessageRegion::SetCharacterId)
        .def_property("animation_id", &MessageRegion::GetAnimationId, &MessageRegion::SetAnimationId)
        .def_property("player_id", &MessageRegion::GetPlayerId, &MessageRegion::SetPlayerId);

    py::class_<EnvironmentMapEffectBoxRegion, Region>(m, "EnvironmentMapEffectBoxRegion")
        .def(py::init<>())
        .def(py::init([](const std::string& n) {
            return std::make_unique<EnvironmentMapEffectBoxRegion>(UTF8ToUTF16(n)); }), py::arg("name"))
        .def_property("enable_dist", &EnvironmentMapEffectBoxRegion::GetEnableDist,
            &EnvironmentMapEffectBoxRegion::SetEnableDist)
        .def_property("transition_dist", &EnvironmentMapEffectBoxRegion::GetTransitionDist,
            &EnvironmentMapEffectBoxRegion::SetTransitionDist)
        .def_property("s_unk_08", &EnvironmentMapEffectBoxRegion::GetSUnk08, &EnvironmentMapEffectBoxRegion::SetSUnk08)
        .def_property("s_unk_09", &EnvironmentMapEffectBoxRegion::GetSUnk09, &EnvironmentMapEffectBoxRegion::SetSUnk09)
        .def_property("s_unk_0a", &EnvironmentMapEffectBoxRegion::GetSUnk0A, &EnvironmentMapEffectBoxRegion::SetSUnk0A)
        .def_property("s_unk_24", &EnvironmentMapEffectBoxRegion::GetSUnk24, &EnvironmentMapEffectBoxRegion::SetSUnk24)
        .def_property("s_unk_28", &EnvironmentMapEffectBoxRegion::GetSUnk28, &EnvironmentMapEffectBoxRegion::SetSUnk28)
        .def_property("s_unk_2c", &EnvironmentMapEffectBoxRegion::GetSUnk2C, &EnvironmentMapEffectBoxRegion::SetSUnk2C)
        .def_property("s_unk_2e", &EnvironmentMapEffectBoxRegion::IsSUnk2E, &EnvironmentMapEffectBoxRegion::SetSUnk2E)
        .def_property("s_unk_2f", &EnvironmentMapEffectBoxRegion::IsSUnk2F, &EnvironmentMapEffectBoxRegion::SetSUnk2F)
        .def_property("s_unk_30", &EnvironmentMapEffectBoxRegion::GetSUnk30, &EnvironmentMapEffectBoxRegion::SetSUnk30)
        .def_property("s_unk_33", &EnvironmentMapEffectBoxRegion::IsSUnk33, &EnvironmentMapEffectBoxRegion::SetSUnk33)
        .def_property("s_unk_34", &EnvironmentMapEffectBoxRegion::GetSUnk34, &EnvironmentMapEffectBoxRegion::SetSUnk34)
        .def_property("s_unk_36", &EnvironmentMapEffectBoxRegion::GetSUnk36, &EnvironmentMapEffectBoxRegion::SetSUnk36);

    DEF_REGION_SUBTYPE_EMPTY(WindAreaRegion, "WindAreaRegion");

    py::class_<ConnectionRegion, Region>(m, "ConnectionRegion")
        .def(py::init<>())
        .def(py::init([](const std::string& n) {
            return std::make_unique<ConnectionRegion>(UTF8ToUTF16(n)); }), py::arg("name"))
        .def_property("target_map_id",
            [](const ConnectionRegion& self) {
                auto id = self.GetTargetMapId();
                return py::make_tuple(id[0], id[1], id[2], id[3]);
            },
            [](ConnectionRegion& self, const py::tuple& t) {
                self.SetTargetMapId({t[0].cast<int8_t>(), t[1].cast<int8_t>(),
                                     t[2].cast<int8_t>(), t[3].cast<int8_t>()});
            });

    DEF_REGION_SUBTYPE_EMPTY(PatrolRoute22Region, "PatrolRoute22Region");
    DEF_REGION_SUBTYPE_EMPTY(BuddySummonPointRegion, "BuddySummonPointRegion");

    py::class_<MufflingBoxRegion, Region>(m, "MufflingBoxRegion")
        .def(py::init<>())
        .def(py::init([](const std::string& n) {
            return std::make_unique<MufflingBoxRegion>(UTF8ToUTF16(n)); }), py::arg("name"))
        .def_property("s_unk_00", &MufflingBoxRegion::GetSUnk00, &MufflingBoxRegion::SetSUnk00)
        .def_property("s_unk_24", &MufflingBoxRegion::GetSUnk24, &MufflingBoxRegion::SetSUnk24)
        .def_property("s_unk_34", &MufflingBoxRegion::GetSUnk34, &MufflingBoxRegion::SetSUnk34)
        .def_property("s_unk_3c", &MufflingBoxRegion::GetSUnk3C, &MufflingBoxRegion::SetSUnk3C)
        .def_property("s_unk_40", &MufflingBoxRegion::GetSUnk40, &MufflingBoxRegion::SetSUnk40)
        .def_property("s_unk_44", &MufflingBoxRegion::GetSUnk44, &MufflingBoxRegion::SetSUnk44);

    py::class_<MufflingPortalRegion, Region>(m, "MufflingPortalRegion")
        .def(py::init<>())
        .def(py::init([](const std::string& n) {
            return std::make_unique<MufflingPortalRegion>(UTF8ToUTF16(n)); }), py::arg("name"))
        .def_property("s_unk_00", &MufflingPortalRegion::GetSUnk00, &MufflingPortalRegion::SetSUnk00);

    py::class_<OtherSoundRegion, Region>(m, "OtherSoundRegion")
        .def(py::init<>())
        .def(py::init([](const std::string& n) {
            return std::make_unique<OtherSoundRegion>(UTF8ToUTF16(n)); }), py::arg("name"))
        .def_property("s_unk_00", &OtherSoundRegion::GetSUnk00, &OtherSoundRegion::SetSUnk00)
        .def_property("s_unk_01", &OtherSoundRegion::GetSUnk01, &OtherSoundRegion::SetSUnk01)
        .def_property("s_unk_02", &OtherSoundRegion::GetSUnk02, &OtherSoundRegion::SetSUnk02)
        .def_property("s_unk_03", &OtherSoundRegion::GetSUnk03, &OtherSoundRegion::SetSUnk03)
        .def_property("s_unk_04", &OtherSoundRegion::GetSUnk04, &OtherSoundRegion::SetSUnk04)
        .def_property("s_unk_08", &OtherSoundRegion::GetSUnk08, &OtherSoundRegion::SetSUnk08)
        .def_property("s_unk_0a", &OtherSoundRegion::GetSUnk0A, &OtherSoundRegion::SetSUnk0A)
        .def_property("s_unk_0c", &OtherSoundRegion::GetSUnk0C, &OtherSoundRegion::SetSUnk0C);

    DEF_REGION_SUBTYPE_EMPTY(MufflingPlaneRegion, "MufflingPlaneRegion");

    py::class_<PatrolRouteRegion, Region>(m, "PatrolRouteRegion")
        .def(py::init<>())
        .def(py::init([](const std::string& n) {
            return std::make_unique<PatrolRouteRegion>(UTF8ToUTF16(n)); }), py::arg("name"))
        .def_property("s_unk_00", &PatrolRouteRegion::GetSUnk00, &PatrolRouteRegion::SetSUnk00);

    py::class_<MapPointRegion, Region>(m, "MapPointRegion")
        .def(py::init<>())
        .def(py::init([](const std::string& n) {
            return std::make_unique<MapPointRegion>(UTF8ToUTF16(n)); }), py::arg("name"))
        .def_property("s_unk_00", &MapPointRegion::GetSUnk00, &MapPointRegion::SetSUnk00)
        .def_property("s_unk_04", &MapPointRegion::GetSUnk04, &MapPointRegion::SetSUnk04)
        .def_property("s_unk_08", &MapPointRegion::GetSUnk08, &MapPointRegion::SetSUnk08)
        .def_property("s_unk_0c", &MapPointRegion::GetSUnk0C, &MapPointRegion::SetSUnk0C)
        .def_property("s_unk_14", &MapPointRegion::GetSUnk14, &MapPointRegion::SetSUnk14)
        .def_property("s_unk_18", &MapPointRegion::GetSUnk18, &MapPointRegion::SetSUnk18);

    py::class_<WeatherOverrideRegion, Region>(m, "WeatherOverrideRegion")
        .def(py::init<>())
        .def(py::init([](const std::string& n) {
            return std::make_unique<WeatherOverrideRegion>(UTF8ToUTF16(n)); }), py::arg("name"))
        .def_property("weather_lot_id", &WeatherOverrideRegion::GetWeatherLotId,
            &WeatherOverrideRegion::SetWeatherLotId);

    py::class_<AutoDrawGroupPointRegion, Region>(m, "AutoDrawGroupPointRegion")
        .def(py::init<>())
        .def(py::init([](const std::string& n) {
            return std::make_unique<AutoDrawGroupPointRegion>(UTF8ToUTF16(n)); }), py::arg("name"))
        .def_property("s_unk_00", &AutoDrawGroupPointRegion::GetSUnk00, &AutoDrawGroupPointRegion::SetSUnk00);

    py::class_<GroupDefeatRewardRegion, Region>(m, "GroupDefeatRewardRegion")
        .def(py::init<>())
        .def(py::init([](const std::string& n) {
            return std::make_unique<GroupDefeatRewardRegion>(UTF8ToUTF16(n)); }), py::arg("name"))
        .def_property("s_unk_00", &GroupDefeatRewardRegion::GetSUnk00, &GroupDefeatRewardRegion::SetSUnk00)
        .def_property("s_unk_04", &GroupDefeatRewardRegion::GetSUnk04, &GroupDefeatRewardRegion::SetSUnk04)
        .def_property("s_unk_08", &GroupDefeatRewardRegion::GetSUnk08, &GroupDefeatRewardRegion::SetSUnk08)
        .def("get_group_part", &GroupDefeatRewardRegion::GetGroupPartAtIndex, py::arg("index"),
            py::return_value_policy::reference)
        .def("set_group_part",
            static_cast<void(GroupDefeatRewardRegion::*)(int, Part*)>(&GroupDefeatRewardRegion::SetGroupPartAtIndex),
            py::arg("index"), py::arg("part"))
        .def_property("s_unk_34", &GroupDefeatRewardRegion::GetSUnk34, &GroupDefeatRewardRegion::SetSUnk34)
        .def_property("s_unk_38", &GroupDefeatRewardRegion::GetSUnk38, &GroupDefeatRewardRegion::SetSUnk38)
        .def_property("s_unk_54", &GroupDefeatRewardRegion::GetSUnk54, &GroupDefeatRewardRegion::SetSUnk54);

    DEF_REGION_SUBTYPE_EMPTY(MapPointDiscoveryOverrideRegion, "MapPointDiscoveryOverrideRegion");
    DEF_REGION_SUBTYPE_EMPTY(MapPointParticipationOverrideRegion, "MapPointParticipationOverrideRegion");

    py::class_<HitsetRegion, Region>(m, "HitsetRegion")
        .def(py::init<>())
        .def(py::init([](const std::string& n) {
            return std::make_unique<HitsetRegion>(UTF8ToUTF16(n)); }), py::arg("name"))
        .def_property("s_unk_00", &HitsetRegion::GetSUnk00, &HitsetRegion::SetSUnk00);

    py::class_<FastTravelRestrictionRegion, Region>(m, "FastTravelRestrictionRegion")
        .def(py::init<>())
        .def(py::init([](const std::string& n) {
            return std::make_unique<FastTravelRestrictionRegion>(UTF8ToUTF16(n)); }), py::arg("name"))
        .def_property("event_flag_id", &FastTravelRestrictionRegion::GetEventFlagId,
            &FastTravelRestrictionRegion::SetEventFlagId);

    DEF_REGION_SUBTYPE_EMPTY(WeatherCreateAssetPointRegion, "WeatherCreateAssetPointRegion");

    py::class_<PlayAreaRegion, Region>(m, "PlayAreaRegion")
        .def(py::init<>())
        .def(py::init([](const std::string& n) {
            return std::make_unique<PlayAreaRegion>(UTF8ToUTF16(n)); }), py::arg("name"))
        .def_property("s_unk_00", &PlayAreaRegion::GetSUnk00, &PlayAreaRegion::SetSUnk00)
        .def_property("s_unk_04", &PlayAreaRegion::GetSUnk04, &PlayAreaRegion::SetSUnk04);

    DEF_REGION_SUBTYPE_EMPTY(EnvironmentMapOutputRegion, "EnvironmentMapOutputRegion");

    py::class_<MountJumpRegion, Region>(m, "MountJumpRegion")
        .def(py::init<>())
        .def(py::init([](const std::string& n) {
            return std::make_unique<MountJumpRegion>(UTF8ToUTF16(n)); }), py::arg("name"))
        .def_property("jump_height", &MountJumpRegion::GetJumpHeight, &MountJumpRegion::SetJumpHeight)
        .def_property("s_unk_04", &MountJumpRegion::GetSUnk04, &MountJumpRegion::SetSUnk04);

    py::class_<DummyRegion, Region>(m, "DummyRegion")
        .def(py::init<>())
        .def(py::init([](const std::string& n) {
            return std::make_unique<DummyRegion>(UTF8ToUTF16(n)); }), py::arg("name"))
        .def_property("s_unk_00", &DummyRegion::GetSUnk00, &DummyRegion::SetSUnk00);

    DEF_REGION_SUBTYPE_EMPTY(FallPreventionRemovalRegion, "FallPreventionRemovalRegion");
    DEF_REGION_SUBTYPE_EMPTY(NavmeshCuttingRegion, "NavmeshCuttingRegion");

    py::class_<MapNameOverrideRegion, Region>(m, "MapNameOverrideRegion")
        .def(py::init<>())
        .def(py::init([](const std::string& n) {
            return std::make_unique<MapNameOverrideRegion>(UTF8ToUTF16(n)); }), py::arg("name"))
        .def_property("map_name_id", &MapNameOverrideRegion::GetMapNameId, &MapNameOverrideRegion::SetMapNameId);

    DEF_REGION_SUBTYPE_EMPTY(MountJumpFallRegion, "MountJumpFallRegion");

    py::class_<HorseRideOverrideRegion, Region>(m, "HorseRideOverrideRegion")
        .def(py::init<>())
        .def(py::init([](const std::string& n) {
            return std::make_unique<HorseRideOverrideRegion>(UTF8ToUTF16(n)); }), py::arg("name"))
        .def_property("override_type", &HorseRideOverrideRegion::GetOverrideType,
            &HorseRideOverrideRegion::SetOverrideType);

    DEF_REGION_SUBTYPE_EMPTY(OtherRegion, "OtherRegion");

    #undef DEF_REGION_SUBTYPE_EMPTY

    // ====================================================================
    // ROUTES
    // ====================================================================

    auto pyRoute = py::class_<Route>(m, "Route")
        DEF_ENTRY_COMMON(Route)
        .def_property_readonly("route_type", &Route::GetType)
        .def_property("h_unk_08", &Route::GetHUnk08, &Route::SetHUnk08)
        .def_property("h_unk_0c", &Route::GetHUnk0C, &Route::SetHUnk0C);

    py::class_<MufflingPortalLinkRoute, Route>(m, "MufflingPortalLinkRoute")
        .def(py::init<>())
        .def(py::init([](const std::string& n) {
            return std::make_unique<MufflingPortalLinkRoute>(UTF8ToUTF16(n)); }), py::arg("name"));

    py::class_<MufflingBoxLinkRoute, Route>(m, "MufflingBoxLinkRoute")
        .def(py::init<>())
        .def(py::init([](const std::string& n) {
            return std::make_unique<MufflingBoxLinkRoute>(UTF8ToUTF16(n)); }), py::arg("name"));

    py::class_<OtherRoute, Route>(m, "OtherRoute")
        .def(py::init<>())
        .def(py::init([](const std::string& n) {
            return std::make_unique<OtherRoute>(UTF8ToUTF16(n)); }), py::arg("name"));

    // ====================================================================
    // PARTS
    // ====================================================================

    auto pyPart = py::class_<Part>(m, "Part")
        DEF_ENTRY_COMMON(Part)
        .def_property_readonly("part_type", &Part::GetType)
        .def_property("entity_id",
            [](const Part& self) { return self.GetEntityId(); },
            [](Part& self, uint32_t id) { self.SetEntityId(id); })
        .def_property("model",
            [](const Part& self) -> Model* { return self.GetModel(); },
            [](Part& self, Model* m) { self.SetModel(m); },
            py::return_value_policy::reference)
        .def_property("model_instance_id", &Part::GetModelInstanceId, &Part::SetModelInstanceId)
        .def_property("sib_path",
            [](const Part& self) { return self.GetSibPathUTF8(); },
            [](Part& self, const std::string& p) { self.SetSibPath(UTF8ToUTF16(p)); })
        .def_property("translate",
            [](const Part& self) { return vec3_to_tuple(self.GetTranslate()); },
            [](Part& self, const py::object& v) { self.SetTranslate(tuple_to_vec3(v)); })
        .def_property("rotate",
            [](const Part& self) { return vec3_to_tuple(self.GetRotate()); },
            [](Part& self, const py::object& v) { self.SetRotate(tuple_to_vec3(v)); })
        .def_property("scale",
            [](const Part& self) { return vec3_to_tuple(self.GetScale()); },
            [](Part& self, const py::object& v) { self.SetScale(tuple_to_vec3(v)); })
        .def_property("unk_44", &Part::GetUnk44, &Part::SetUnk44)
        .def_property("event_layer", &Part::GetEventLayer, &Part::SetEventLayer)
        .def_property("unk_04", &Part::GetUnk04, &Part::SetUnk04)
        .def_property("lod_id", &Part::GetLodId, &Part::SetLodId)
        .def_property("unk_09", &Part::GetUnk09, &Part::SetUnk09)
        .def_property("is_point_light_shadow_source", &Part::GetIsPointLightShadowSource,
            &Part::SetIsPointLightShadowSource)
        .def_property("unk_0b", &Part::GetUnk0B, &Part::SetUnk0B)
        .def_property("is_shadow_source", &Part::isIsShadowSource, &Part::SetIsShadowSource)
        .def_property("is_static_shadow_source", &Part::GetIsStaticShadowSource, &Part::SetIsStaticShadowSource)
        .def_property("is_cascade_3_shadow_source", &Part::GetIsCascade3ShadowSource,
            &Part::SetIsCascade3ShadowSource)
        .def_property("unk_0f", &Part::GetUnk0F, &Part::SetUnk0F)
        .def_property("unk_10", &Part::GetUnk10, &Part::SetUnk10)
        .def_property("is_shadow_destination", &Part::isIsShadowDestination, &Part::SetIsShadowDestination)
        .def_property("is_shadow_only", &Part::isIsShadowOnly, &Part::SetIsShadowOnly)
        .def_property("draw_by_reflect_cam", &Part::isDrawByReflectCam, &Part::SetDrawByReflectCam)
        .def_property("draw_only_reflect_cam", &Part::isDrawOnlyReflectCam, &Part::SetDrawOnlyReflectCam)
        .def_property("enable_on_above_shadow", &Part::GetEnableOnAboveShadow, &Part::SetEnableOnAboveShadow)
        .def_property("disable_point_light_effect", &Part::isDisablePointLightEffect,
            &Part::SetDisablePointLightEffect)
        .def_property("unk_17", &Part::GetUnk17, &Part::SetUnk17)
        .def_property("unk_18", &Part::GetUnk18, &Part::SetUnk18)
        .def_property("entity_group_ids",
            [](const Part& self) {
                auto ids = self.GetEntityGroupIds();
                py::list result;
                for (auto id : ids) result.append(id);
                return result;
            },
            [](Part& self, const py::list& ids) {
                std::array<uint32_t, 8> arr{};
                for (size_t i = 0; i < 8 && i < py::len(ids); ++i)
                    arr[i] = ids[i].cast<uint32_t>();
                self.SetEntityGroupIds(arr);
            })
        .def_property("unk_3c", &Part::GetUnk3C, &Part::SetUnk3C)
        .def_property("unk_3e", &Part::GetUnk3E, &Part::SetUnk3E);

    py::class_<MapPiecePart, Part>(m, "MapPiecePart")
        .def(py::init<>())
        .def(py::init([](const std::string& n) {
            return std::make_unique<MapPiecePart>(UTF8ToUTF16(n)); }), py::arg("name"))
        .def_readwrite("draw_info_1", &MapPiecePart::drawInfo1)
        .def_readwrite("gparam", &MapPiecePart::gparam)
        .def_readwrite("grass_config", &MapPiecePart::grassConfig)
        .def_readwrite("unk_struct_8", &MapPiecePart::unkStruct8)
        .def_readwrite("unk_struct_9", &MapPiecePart::unkStruct9)
        .def_readwrite("tile_load_config", &MapPiecePart::tileLoadConfig)
        .def_readwrite("unk_struct_11", &MapPiecePart::unkStruct11);

    auto pyCharacterPart = py::class_<CharacterPart, Part>(m, "CharacterPart")
        .def(py::init<>())
        .def(py::init([](const std::string& n) {
            return std::make_unique<CharacterPart>(UTF8ToUTF16(n)); }), py::arg("name"))
        .def_property("ai_id", &CharacterPart::GetAiId, &CharacterPart::SetAiId)
        .def_property("character_id", &CharacterPart::GetCharacterId, &CharacterPart::SetCharacterId)
        .def_property("talk_id", &CharacterPart::GetTalkId, &CharacterPart::SetTalkId)
        .def_property("s_unk_15", &CharacterPart::IsSUnk15, &CharacterPart::SetSUnk15)
        .def_property("platoon_id", &CharacterPart::GetPlatoonId, &CharacterPart::SetPlatoonId)
        .def_property("player_id", &CharacterPart::GetPlayerId, &CharacterPart::SetPlayerId)
        .def_property("draw_parent",
            [](const CharacterPart& self) -> Part* { return self.GetDrawParent(); },
            [](CharacterPart& self, Part* p) { self.SetDrawParent(p); },
            py::return_value_policy::reference)
        .def_property("patrol_route_event",
            [](const CharacterPart& self) -> PatrolRouteEvent* { return self.GetPatrolRouteEvent(); },
            [](CharacterPart& self, PatrolRouteEvent* e) { self.SetPatrolRouteEvent(e); },
            py::return_value_policy::reference)
        .def_property("s_unk_24", &CharacterPart::GetSUnk24, &CharacterPart::SetSUnk24)
        .def_property("s_unk_28", &CharacterPart::GetSUnk28, &CharacterPart::SetSUnk28)
        .def_property("activate_condition_param_id", &CharacterPart::GetActivateConditionParamId,
            &CharacterPart::SetActivateConditionParamId)
        .def_property("s_unk_34", &CharacterPart::GetSUnk34, &CharacterPart::SetSUnk34)
        .def_property("back_away_event_animation_id", &CharacterPart::GetBackAwayEventAnimationId,
            &CharacterPart::SetBackAwayEventAnimationId)
        .def_property("s_unk_3c", &CharacterPart::GetSUnk3C, &CharacterPart::SetSUnk3C)
        .def_property("special_effect_set_param_ids",
            [](const CharacterPart& self) {
                auto ids = self.GetSpecialEffectSetParamIds();
                py::list result;
                for (auto id : ids) result.append(id);
                return result;
            },
            [](CharacterPart& self, const py::list& ids) {
                std::array<int, 4> arr{};
                for (size_t i = 0; i < 4 && i < py::len(ids); ++i)
                    arr[i] = ids[i].cast<int>();
                self.SetSpecialEffectSetParamIds(arr);
            })
        .def_property("s_unk_84", &CharacterPart::GetSUnk84, &CharacterPart::SetSUnk84)
        .def_readwrite("draw_info_1", &CharacterPart::drawInfo1)
        .def_readwrite("gparam", &CharacterPart::gparam)
        .def_readwrite("unk_struct_8", &CharacterPart::unkStruct8)
        .def_readwrite("tile_load_config", &CharacterPart::tileLoadConfig);

    py::class_<DummyCharacterPart, CharacterPart>(m, "DummyCharacterPart")
        .def(py::init<>())
        .def(py::init([](const std::string& n) {
            return std::make_unique<DummyCharacterPart>(UTF8ToUTF16(n)); }), py::arg("name"));

    py::class_<PlayerStartPart, Part>(m, "PlayerStartPart")
        .def(py::init<>())
        .def(py::init([](const std::string& n) {
            return std::make_unique<PlayerStartPart>(UTF8ToUTF16(n)); }), py::arg("name"))
        .def_property("s_unk_00", &PlayerStartPart::GetSUnk00, &PlayerStartPart::SetSUnk00)
        .def_readwrite("draw_info_1", &PlayerStartPart::drawInfo1)
        .def_readwrite("unk_struct_8", &PlayerStartPart::unkStruct8)
        .def_readwrite("tile_load_config", &PlayerStartPart::tileLoadConfig);

    py::class_<CollisionPart, Part>(m, "CollisionPart")
        .def(py::init<>())
        .def(py::init([](const std::string& n) {
            return std::make_unique<CollisionPart>(UTF8ToUTF16(n)); }), py::arg("name"))
        .def_property("hit_filter_id", &CollisionPart::GetHitFilterId, &CollisionPart::SetHitFilterId)
        .def_property("s_unk_01", &CollisionPart::GetSUnk01, &CollisionPart::SetSUnk01)
        .def_property("s_unk_02", &CollisionPart::GetSUnk02, &CollisionPart::SetSUnk02)
        .def_property("s_unk_03", &CollisionPart::GetSUnk03, &CollisionPart::SetSUnk03)
        .def_property("s_unk_04", &CollisionPart::GetSUnk04, &CollisionPart::SetSUnk04)
        .def_property("s_unk_14", &CollisionPart::GetSUnk14, &CollisionPart::SetSUnk14)
        .def_property("s_unk_18", &CollisionPart::GetSUnk18, &CollisionPart::SetSUnk18)
        .def_property("s_unk_1c", &CollisionPart::GetSUnk1C, &CollisionPart::SetSUnk1C)
        .def_property("play_region_id", &CollisionPart::GetPlayRegionId, &CollisionPart::SetPlayRegionId)
        .def_property("s_unk_24", &CollisionPart::GetSUnk24, &CollisionPart::SetSUnk24)
        .def_property("s_unk_26", &CollisionPart::GetSUnk26, &CollisionPart::SetSUnk26)
        .def_property("s_unk_30", &CollisionPart::GetSUnk30, &CollisionPart::SetSUnk30)
        .def_property("s_unk_34", &CollisionPart::GetSUnk34, &CollisionPart::SetSUnk34)
        .def_property("s_unk_35", &CollisionPart::GetSUnk35, &CollisionPart::SetSUnk35)
        .def_property("disable_torrent", &CollisionPart::isDisableTorrent, &CollisionPart::SetDisableTorrent)
        .def_property("s_unk_3c", &CollisionPart::GetSUnk3C, &CollisionPart::SetSUnk3C)
        .def_property("s_unk_3e", &CollisionPart::GetSUnk3E, &CollisionPart::SetSUnk3E)
        .def_property("s_unk_40", &CollisionPart::GetSUnk40, &CollisionPart::SetSUnk40)
        .def_property("enable_fast_travel_flag_id", &CollisionPart::GetEnableFastTravelFlagId,
            &CollisionPart::SetEnableFastTravelFlagId)
        .def_property("s_unk_4c", &CollisionPart::GetSUnk4C, &CollisionPart::SetSUnk4C)
        .def_property("s_unk_4e", &CollisionPart::GetSUnk4E, &CollisionPart::SetSUnk4E)
        .def_readwrite("draw_info_1", &CollisionPart::drawInfo1)
        .def_readwrite("draw_info_2", &CollisionPart::drawInfo2)
        .def_readwrite("gparam", &CollisionPart::gparam)
        .def_readwrite("scene_gparam", &CollisionPart::sceneGparam)
        .def_readwrite("unk_struct_8", &CollisionPart::unkStruct8)
        .def_readwrite("tile_load_config", &CollisionPart::tileLoadConfig)
        .def_readwrite("unk_struct_11", &CollisionPart::unkStruct11);

    py::class_<DummyAssetPart, Part>(m, "DummyAssetPart")
        .def(py::init<>())
        .def(py::init([](const std::string& n) {
            return std::make_unique<DummyAssetPart>(UTF8ToUTF16(n)); }), py::arg("name"))
        .def_property("s_unk_18", &DummyAssetPart::GetSUnk18, &DummyAssetPart::SetSUnk18)
        .def_readwrite("draw_info_1", &DummyAssetPart::drawInfo1)
        .def_readwrite("gparam", &DummyAssetPart::gparam)
        .def_readwrite("unk_struct_8", &DummyAssetPart::unkStruct8)
        .def_readwrite("tile_load_config", &DummyAssetPart::tileLoadConfig);

    py::class_<ConnectCollisionPart, Part>(m, "ConnectCollisionPart")
        .def(py::init<>())
        .def(py::init([](const std::string& n) {
            return std::make_unique<ConnectCollisionPart>(UTF8ToUTF16(n)); }), py::arg("name"))
        .def_property("collision",
            [](const ConnectCollisionPart& self) -> CollisionPart* { return self.GetCollision(); },
            [](ConnectCollisionPart& self, CollisionPart* c) { self.SetCollision(c); },
            py::return_value_policy::reference)
        .def_property("connected_map_id",
            [](const ConnectCollisionPart& self) {
                auto id = self.GetConnectedMapId();
                return py::make_tuple(id[0], id[1], id[2], id[3]);
            },
            [](ConnectCollisionPart& self, const py::tuple& t) {
                self.SetConnectedMapId({t[0].cast<int8_t>(), t[1].cast<int8_t>(),
                                        t[2].cast<int8_t>(), t[3].cast<int8_t>()});
            })
        .def_property("s_unk_08", &ConnectCollisionPart::GetSUnk08, &ConnectCollisionPart::SetSUnk08)
        .def_property("s_unk_09", &ConnectCollisionPart::IsSUnk09, &ConnectCollisionPart::SetSUnk09)
        .def_property("s_unk_0a", &ConnectCollisionPart::GetSUnk0A, &ConnectCollisionPart::SetSUnk0A)
        .def_property("s_unk_0b", &ConnectCollisionPart::IsSUnk0B, &ConnectCollisionPart::SetSUnk0B)
        .def_readwrite("draw_info_1", &ConnectCollisionPart::drawInfo1)
        .def_readwrite("draw_info_2", &ConnectCollisionPart::drawInfo2)
        .def_readwrite("unk_struct_8", &ConnectCollisionPart::unkStruct8)
        .def_readwrite("tile_load_config", &ConnectCollisionPart::tileLoadConfig)
        .def_readwrite("unk_struct_11", &ConnectCollisionPart::unkStruct11);

    py::class_<AssetPart, Part>(m, "AssetPart")
        .def(py::init<>())
        .def(py::init([](const std::string& n) {
            return std::make_unique<AssetPart>(UTF8ToUTF16(n)); }), py::arg("name"))
        .def_property("s_unk_02", &AssetPart::GetSUnk02, &AssetPart::SetSUnk02)
        .def_property("s_unk_10", &AssetPart::GetSUnk10, &AssetPart::SetSUnk10)
        .def_property("s_unk_11", &AssetPart::IsSUnk11, &AssetPart::SetSUnk11)
        .def_property("s_unk_12", &AssetPart::GetSUnk12, &AssetPart::SetSUnk12)
        .def_property("sfx_param_relative_id", &AssetPart::GetSfxParamRelativeId,
            &AssetPart::SetSfxParamRelativeId)
        .def_property("s_unk_1e", &AssetPart::GetSUnk1E, &AssetPart::SetSUnk1E)
        .def_property("s_unk_24", &AssetPart::GetSUnk24, &AssetPart::SetSUnk24)
        .def_property("s_unk_28", &AssetPart::GetSUnk28, &AssetPart::SetSUnk28)
        .def_property("s_unk_30", &AssetPart::GetSUnk30, &AssetPart::SetSUnk30)
        .def_property("s_unk_34", &AssetPart::GetSUnk34, &AssetPart::SetSUnk34)
        .def("get_draw_parent_part", &AssetPart::GetDrawParentPartAtIndex, py::arg("index"),
            py::return_value_policy::reference)
        .def("set_draw_parent_part",
            static_cast<void(AssetPart::*)(int, Part*)>(&AssetPart::SetDrawParentPartAtIndex),
            py::arg("index"), py::arg("part"))
        .def_property("s_unk_50", &AssetPart::IsSUnk50, &AssetPart::SetSUnk50)
        .def_property("s_unk_51", &AssetPart::GetSUnk51, &AssetPart::SetSUnk51)
        .def_property("s_unk_53", &AssetPart::GetSUnk53, &AssetPart::SetSUnk53)
        .def_property("s_unk_54", &AssetPart::GetSUnk54, &AssetPart::SetSUnk54)
        .def_property("s_unk_58", &AssetPart::GetSUnk58, &AssetPart::SetSUnk58)
        .def_property("s_unk_5c", &AssetPart::GetSUnk5C, &AssetPart::SetSUnk5C)
        .def_property("s_unk_60", &AssetPart::GetSUnk60, &AssetPart::SetSUnk60)
        .def_property("s_unk_64", &AssetPart::GetSUnk64, &AssetPart::SetSUnk64)
        .def_readwrite("draw_info_1", &AssetPart::drawInfo1)
        .def_readwrite("draw_info_2", &AssetPart::drawInfo2)
        .def_readwrite("gparam", &AssetPart::gparam)
        .def_readwrite("grass_config", &AssetPart::grassConfig)
        .def_readwrite("unk_struct_8", &AssetPart::unkStruct8)
        .def_readwrite("unk_struct_9", &AssetPart::unkStruct9)
        .def_readwrite("tile_load_config", &AssetPart::tileLoadConfig)
        .def_readwrite("unk_struct_11", &AssetPart::unkStruct11)
        .def_readwrite("extra_data_1", &AssetPart::extraData1)
        .def_readwrite("extra_data_2", &AssetPart::extraData2)
        .def_readwrite("extra_data_3", &AssetPart::extraData3)
        .def_readwrite("extra_data_4", &AssetPart::extraData4);

    // ====================================================================
    // ENTRY PARAMS
    // ====================================================================

    py::class_<ModelParam>(m, "ModelParam")
        .def_property_readonly("version", &ModelParam::GetVersion)
        .def("get_all_entries", [](py::object& self) {
            auto& param = py::cast<ModelParam&>(self);
            py::list result;
            for (auto* e : param.GetAllEntries())
                result.append(py::cast(e, py::return_value_policy::reference_internal, self));
            return result;
        })
        .def("__len__", &ModelParam::GetSize)
        .def("add_entry", [](ModelParam& self, std::unique_ptr<Model> entry) {
            self.AddEntry(std::move(entry));
        })
        .def("remove_entry", &ModelParam::RemoveEntry, py::arg("entry"));

    py::class_<EventParam>(m, "EventParam")
        .def_property_readonly("version", &EventParam::GetVersion)
        .def("get_all_entries", [](py::object& self) {
            auto& param = py::cast<EventParam&>(self);
            py::list result;
            for (auto* e : param.GetAllEntries())
                result.append(py::cast(e, py::return_value_policy::reference_internal, self));
            return result;
        })
        .def("__len__", &EventParam::GetSize)
        .def("add_entry", [](EventParam& self, std::unique_ptr<Event> entry) {
            self.AddEntry(std::move(entry));
        })
        .def("remove_entry", &EventParam::RemoveEntry, py::arg("entry"));

    py::class_<RegionParam>(m, "RegionParam")
        .def_property_readonly("version", &RegionParam::GetVersion)
        .def("get_all_entries", [](py::object& self) {
            auto& param = py::cast<RegionParam&>(self);
            py::list result;
            for (auto* e : param.GetAllEntries())
                result.append(py::cast(e, py::return_value_policy::reference_internal, self));
            return result;
        })
        .def("__len__", &RegionParam::GetSize)
        .def("add_entry", [](RegionParam& self, std::unique_ptr<Region> entry) {
            self.AddEntry(std::move(entry));
        })
        .def("remove_entry", &RegionParam::RemoveEntry, py::arg("entry"));

    py::class_<RouteParam>(m, "RouteParam")
        .def_property_readonly("version", &RouteParam::GetVersion)
        .def("get_all_entries", [](py::object& self) {
            auto& param = py::cast<RouteParam&>(self);
            py::list result;
            for (auto* e : param.GetAllEntries())
                result.append(py::cast(e, py::return_value_policy::reference_internal, self));
            return result;
        })
        .def("__len__", &RouteParam::GetSize)
        .def("add_entry", [](RouteParam& self, std::unique_ptr<Route> entry) {
            self.AddEntry(std::move(entry));
        })
        .def("remove_entry", &RouteParam::RemoveEntry, py::arg("entry"));

    py::class_<PartParam>(m, "PartParam")
        .def_property_readonly("version", &PartParam::GetVersion)
        .def("get_all_entries", [](py::object& self) {
            auto& param = py::cast<PartParam&>(self);
            py::list result;
            for (auto* e : param.GetAllEntries())
                result.append(py::cast(e, py::return_value_policy::reference_internal, self));
            return result;
        })
        .def("__len__", &PartParam::GetSize)
        .def("add_entry", [](PartParam& self, std::unique_ptr<Part> entry) {
            self.AddEntry(std::move(entry));
        })
        .def("remove_entry", &PartParam::RemoveEntry, py::arg("entry"));

    // ====================================================================
    // MSB (top-level)
    // ====================================================================

    py::class_<MSB>(m, "MSB")
        .def(py::init<>(), "Create an empty MSB.")

        .def_property_readonly("model_param",
            [](py::object& self) -> ModelParam& {
                return py::cast<MSB&>(self).GetModelParam();
            }, py::return_value_policy::reference_internal)
        .def_property_readonly("event_param",
            [](py::object& self) -> EventParam& {
                return py::cast<MSB&>(self).GetEventParam();
            }, py::return_value_policy::reference_internal)
        .def_property_readonly("region_param",
            [](py::object& self) -> RegionParam& {
                return py::cast<MSB&>(self).GetRegionParam();
            }, py::return_value_policy::reference_internal)
        .def_property_readonly("route_param",
            [](py::object& self) -> RouteParam& {
                return py::cast<MSB&>(self).GetRouteParam();
            }, py::return_value_policy::reference_internal)
        .def_property_readonly("part_param",
            [](py::object& self) -> PartParam& {
                return py::cast<MSB&>(self).GetPartParam();
            }, py::return_value_policy::reference_internal)

        // Convenience flat-list accessors.
        .def_property_readonly("models", [](py::object& self) {
            auto& msb = py::cast<MSB&>(self);
            py::list result;
            for (auto* e : msb.GetModelParam().GetAllEntries())
                result.append(py::cast(e, py::return_value_policy::reference_internal, self));
            return result;
        })
        .def_property_readonly("events", [](py::object& self) {
            auto& msb = py::cast<MSB&>(self);
            py::list result;
            for (auto* e : msb.GetEventParam().GetAllEntries())
                result.append(py::cast(e, py::return_value_policy::reference_internal, self));
            return result;
        })
        .def_property_readonly("regions", [](py::object& self) {
            auto& msb = py::cast<MSB&>(self);
            py::list result;
            for (auto* e : msb.GetRegionParam().GetAllEntries())
                result.append(py::cast(e, py::return_value_policy::reference_internal, self));
            return result;
        })
        .def_property_readonly("routes", [](py::object& self) {
            auto& msb = py::cast<MSB&>(self);
            py::list result;
            for (auto* e : msb.GetRouteParam().GetAllEntries())
                result.append(py::cast(e, py::return_value_policy::reference_internal, self));
            return result;
        })
        .def_property_readonly("parts", [](py::object& self) {
            auto& msb = py::cast<MSB&>(self);
            py::list result;
            for (auto* e : msb.GetPartParam().GetAllEntries())
                result.append(py::cast(e, py::return_value_policy::reference_internal, self));
            return result;
        })

        // ---- I/O with automatic DCX handling ----

        .def_static("from_bytes", [](const py::bytes& data) {
            std::string raw_str = data;
            const auto* raw_data = reinterpret_cast<const std::byte*>(raw_str.data());
            const std::size_t raw_size = raw_str.size();

            std::unique_ptr<MSB> msb;
            {
                py::gil_scoped_release release;

                auto temp_path = std::filesystem::temp_directory_path() / "firelink_msb_from_bytes.msb";

                if (IsDCX(raw_data, raw_size))
                {
                    DCXResult dcx_result = DecompressDCX(raw_data, raw_size);
                    std::ofstream tmp(temp_path, std::ios::binary);
                    tmp.write(reinterpret_cast<const char*>(dcx_result.data.data()),
                        static_cast<std::streamsize>(dcx_result.data.size()));
                    tmp.close();
                }
                else
                {
                    std::ofstream tmp(temp_path, std::ios::binary);
                    tmp.write(raw_str.data(), static_cast<std::streamsize>(raw_size));
                    tmp.close();
                }

                msb = MSB::FromPath(temp_path);
                std::filesystem::remove(temp_path);
            }
            return msb;
        },
        py::arg("data"),
        "Read an MSB from raw bytes, automatically decompressing DCX if needed.")

        .def("to_bytes", [](MSB& self, int dcx_type) {
            std::vector<std::byte> output;
            {
                py::gil_scoped_release release;

                auto temp_path = std::filesystem::temp_directory_path() / "firelink_msb_to_bytes.msb";
                self.WriteToFilePath(temp_path);

                std::vector<std::byte> raw = BinaryReadWrite::ReadFileBytes(temp_path.string());
                std::filesystem::remove(temp_path);

                if (dcx_type != 0)
                    output = CompressDCX(raw.data(), raw.size(), static_cast<DCXType>(dcx_type));
                else
                    output = std::move(raw);
            }
            return py::bytes(reinterpret_cast<const char*>(output.data()), output.size());
        },
        py::arg("dcx_type") = 0,
        "Serialize this MSB to bytes, optionally with DCX compression.\n\n"
        "dcx_type: integer DCXType value. 0 = no compression.")

        .def_static("from_path", [](const py::object& path_obj) {
            py::object os = py::module_::import("os");
            std::string path_str = os.attr("fspath")(path_obj).cast<std::string>();

            std::unique_ptr<MSB> msb;
            {
                py::gil_scoped_release release;

                const std::vector<std::byte> raw = BinaryReadWrite::ReadFileBytes(path_str);

                if (IsDCX(raw.data(), raw.size()))
                {
                    DCXResult dcx_result = DecompressDCX(raw.data(), raw.size());
                    // Write decompressed data to a temp file for MSB stream-based parsing.
                    auto temp_path = std::filesystem::temp_directory_path() / "firelink_msb_temp.msb";
                    {
                        std::ofstream tmp(temp_path, std::ios::binary);
                        tmp.write(reinterpret_cast<const char*>(dcx_result.data.data()),
                            static_cast<std::streamsize>(dcx_result.data.size()));
                    }
                    msb = MSB::FromPath(temp_path);
                    std::filesystem::remove(temp_path);
                }
                else
                {
                    msb = MSB::FromPath(path_str);
                }
            }
            return msb;
        },
        py::arg("path"),
        "Read an MSB from a file path, automatically decompressing DCX if needed.")

        .def("write_to_path", [](MSB& self, const py::object& path_obj, int dcx_type) {
            py::object os = py::module_::import("os");
            std::string path_str = os.attr("fspath")(path_obj).cast<std::string>();

            py::gil_scoped_release release;

            if (dcx_type == 0)
            {
                self.WriteToFilePath(path_str);
            }
            else
            {
                auto temp_path = std::filesystem::temp_directory_path() / "firelink_msb_temp_write.msb";
                self.WriteToFilePath(temp_path);

                std::vector<std::byte> raw = BinaryReadWrite::ReadFileBytes(temp_path.string());
                std::filesystem::remove(temp_path);

                std::vector<std::byte> compressed = CompressDCX(
                    raw.data(), raw.size(), static_cast<DCXType>(dcx_type));

                std::ofstream out(path_str, std::ios::binary);
                if (!out) throw std::runtime_error("Could not open file for writing: " + path_str);
                out.write(reinterpret_cast<const char*>(compressed.data()),
                    static_cast<std::streamsize>(compressed.size()));
            }
        },
        py::arg("path"),
        py::arg("dcx_type") = 0,
        "Write this MSB to a file path, optionally with DCX compression.\n\n"
        "dcx_type: integer DCXType value. 0 = no compression.")

        .def("__repr__", [](MSB& self) {
            return "<MSB: " +
                std::to_string(self.GetModelParam().GetSize()) + " models, " +
                std::to_string(self.GetEventParam().GetSize()) + " events, " +
                std::to_string(self.GetRegionParam().GetSize()) + " regions, " +
                std::to_string(self.GetRouteParam().GetSize()) + " routes, " +
                std::to_string(self.GetPartParam().GetSize()) + " parts>";
        });
}
