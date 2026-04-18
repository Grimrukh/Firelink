// test_er_msb.cpp — comprehensive doctest suite for Elden Ring MSB read/write.
//
// Uses the bundled Siofra River MSB (m12_02_00_00.msb, DCX removed) to verify:
//   1.  MSB loads without error.
//   2.  All param counts are positive.
//   3.  Known subtype distributions are non-empty.
//   4.  Names, types, and cross-references resolve correctly.
//   5.  String conversion operator works on entries.
//   6.  Draw-parent cross-references can be mutated.
//   7.  Write -> re-read produces identical param counts and entry data.
//   8.  Double write produces byte-identical files (stable serialization).

#include <doctest/doctest.h>

#include <FirelinkCore/BinaryReadWrite.h>
#include <FirelinkERMaps/MapStudio/EntryParam.h>
#include <FirelinkERMaps/MapStudio/MSB.h>
#include <FirelinkERMaps/MapStudio/Model.h>
#include <FirelinkERMaps/MapStudio/Event.h>
#include <FirelinkERMaps/MapStudio/Region.h>
#include <FirelinkERMaps/MapStudio/Part.h>
#include <FirelinkTestHelpers.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

using namespace Firelink;
using namespace Firelink::EldenRing::Maps::MapStudio;

// ============================================================================
// Helpers
// ============================================================================

namespace
{
    /// Resolve the bundled MSB resource path.
    std::filesystem::path GetMSBPath()
    {
        std::filesystem::path p = std::filesystem::path(TEST_RESOURCES_DIR) / "eldenring/m12_02_00_00.msb.dcx";
        REQUIRE_MESSAGE(exists(p), "MSB resource not found: ", p.string());
        return p;
    }

    /// Lazily load and cache the original MSB (shared across read-only tests).
    MSB& GetOriginalMSB()
    {
        static std::unique_ptr<MSB> msb = [] {
            const auto start = std::chrono::high_resolution_clock::now();
            auto ptr = MSB::FromPath(GetMSBPath());
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now() - start);
            MESSAGE("MSB loaded in " << elapsed.count() << " ms");
            return ptr;
        }();
        return *msb;
    }

    /// Create (and cache) a temp output directory for round-trip files.
    const std::filesystem::path& GetOutDir()
    {
        static const std::filesystem::path dir = [] {
            auto d = std::filesystem::temp_directory_path() / "firelink_msb_test";
            create_directories(d);
            return d;
        }();
        return dir;
    }
} // namespace

// ============================================================================
// Basic loading
// ============================================================================

TEST_CASE("MSB: loads Siofra River (m12_02_00_00) without error")
{
    CHECK_NOTHROW(GetOriginalMSB());
}

// ============================================================================
// Param counts
// ============================================================================

TEST_CASE("MSB: all param counts are positive")
{
    auto& msb = GetOriginalMSB();
    CHECK(msb.GetModelParam().GetSize()  > 0);
    CHECK(msb.GetEventParam().GetSize()  > 0);
    CHECK(msb.GetRegionParam().GetSize() > 0);
    CHECK(msb.GetRouteParam().GetSize()  > 0);
    CHECK(msb.GetPartParam().GetSize()   > 0);
}

// ============================================================================
// Subtype distributions
// ============================================================================

TEST_CASE("MSB: model subtype distribution is non-empty")
{
    auto& msb = GetOriginalMSB();
    auto all = msb.GetModelParam().GetAllEntries();

    int mapPiece = 0, character = 0, collision = 0, asset = 0;
    for (const auto* m : all)
    {
        switch (m->GetType())
        {
            case ModelType::MapPiece:  ++mapPiece;  break;
            case ModelType::Character: ++character;  break;
            case ModelType::Collision: ++collision;  break;
            case ModelType::Asset:     ++asset;      break;
            default: break;
        }
    }
    CHECK(mapPiece  > 0);
    CHECK(character > 0);
    CHECK(collision > 0);
    CHECK(asset     > 0);
}

TEST_CASE("MSB: part subtype distribution is non-empty")
{
    auto& msb = GetOriginalMSB();
    auto all = msb.GetPartParam().GetAllEntries();

    int mapPiece = 0, character = 0, collision = 0, asset = 0;
    for (const auto* p : all)
    {
        switch (p->GetType())
        {
            case PartType::MapPiece:  ++mapPiece;  break;
            case PartType::Character: ++character;  break;
            case PartType::Collision: ++collision;  break;
            case PartType::Asset:     ++asset;      break;
            default: break;
        }
    }
    CHECK(mapPiece  > 0);
    CHECK(character > 0);
    CHECK(collision > 0);
    CHECK(asset     > 0);
}

TEST_CASE("MSB: region count > 10 for Stormveil")
{
    auto& msb = GetOriginalMSB();
    CHECK(msb.GetRegionParam().GetAllEntries().size() > 10);
}

TEST_CASE("MSB: at least one Treasure event")
{
    auto& msb = GetOriginalMSB();
    auto all = msb.GetEventParam().GetAllEntries();
    int treasures = 0;
    for (const auto* e : all)
        if (e->GetType() == EventType::Treasure) ++treasures;
    CHECK(treasures > 0);
}

// ============================================================================
// Entry names and cross-references
// ============================================================================

TEST_CASE("MSB: all entry names are non-empty except Routes")
{
    auto& msb = GetOriginalMSB();

    for (const auto* m : msb.GetModelParam().GetAllEntries())
        CHECK_FALSE(m->GetNameUTF8().empty());
    for (const auto* e : msb.GetEventParam().GetAllEntries())
        CHECK_FALSE(e->GetNameUTF8().empty());
    for (const auto* r : msb.GetRegionParam().GetAllEntries())
        CHECK_FALSE(r->GetNameUTF8().empty());
    for (const auto* p : msb.GetPartParam().GetAllEntries())
        CHECK_FALSE(p->GetNameUTF8().empty());
}

TEST_CASE("MSB: every part has a model reference")
{
    auto& msb = GetOriginalMSB();
    for (const auto* p : msb.GetPartParam().GetAllEntries())
        CHECK_MESSAGE(p->GetModel() != nullptr, "Part '", p->GetNameUTF8(), "' has no model");
}

TEST_CASE("MSB: first part's model exists in model list")
{
    auto& msb = GetOriginalMSB();
    auto allParts  = msb.GetPartParam().GetAllEntries();
    auto allModels = msb.GetModelParam().GetAllEntries();
    REQUIRE_FALSE(allParts.empty());
    REQUIRE(allParts[0]->GetModel() != nullptr);

    const std::string modelName = allParts[0]->GetModel()->GetNameUTF8();
    bool found = false;
    for (const auto* m : allModels)
        if (m->GetNameUTF8() == modelName) { found = true; break; }
    CHECK(found);
}

// ============================================================================
// std::string conversion operator (from old er_msb_test.cpp)
// ============================================================================

TEST_CASE("MSB: entry std::string conversion produces non-empty output")
{
    auto& msb = GetOriginalMSB();
    auto allParts = msb.GetPartParam().GetAllEntries();
    REQUIRE_FALSE(allParts.empty());

    // Part -> string
    CHECK_FALSE(std::string(*allParts[0]).empty());

    // Character -> string
    auto characters = msb.GetPartParam().GetSubtypeEntries<CharacterPart>();
    REQUIRE_FALSE(characters.empty());
    CHECK_FALSE(std::string(*characters[0]).empty());

    // Model -> string
    if (const auto* model = characters[0]->GetModel())
        CHECK_FALSE(std::string(*model).empty());
}

// ============================================================================
// Character part fields
// ============================================================================

TEST_CASE("MSB: character part fields are valid")
{
    auto& msb = GetOriginalMSB();
    auto characters = msb.GetPartParam().GetSubtypeEntries<CharacterPart>();
    REQUIRE_FALSE(characters.empty());

    const auto* chr = characters[0];

    SUBCASE("translate is finite")
    {
        auto [x, y, z] = chr->GetTranslate();
        CHECK(isfinite(x));
        CHECK(isfinite(y));
        CHECK(isfinite(z));
    }

    SUBCASE("scale is positive")
    {
        auto [sx, sy, sz] = chr->GetScale();
        CHECK(sx > 0);
        CHECK(sy > 0);
        CHECK(sz > 0);
    }
}

// ============================================================================
// Collision part fields
// ============================================================================

TEST_CASE("MSB: collision part fields are valid")
{
    auto& msb = GetOriginalMSB();
    auto collisions = msb.GetPartParam().GetSubtypeEntries<CollisionPart>();
    REQUIRE_FALSE(collisions.empty());

    const auto* col = collisions[0];
    const uint8_t hf = col->GetHitFilterId();
    CHECK(hf >= 8);
    CHECK(hf <= 29);
}

// ============================================================================
// Region shapes
// ============================================================================

TEST_CASE("MSB: all regions have a shape object")
{
    auto& msb = GetOriginalMSB();
    auto allRegions = msb.GetRegionParam().GetAllEntries();
    for (auto* r : allRegions)
        CHECK_MESSAGE(r->GetShape() != nullptr, "Region '", r->GetNameUTF8(), "' has no shape");
}

// ============================================================================
// Draw-parent mutation (from old er_msb_test.cpp)
// ============================================================================

TEST_CASE("MSB: draw parent can be cleared and reassigned")
{
    // Load a fresh copy so mutations don't pollute other tests.
    auto msb = MSB::FromPath(GetMSBPath());
    auto allParts   = msb->GetPartParam().GetAllEntries();
    auto characters = msb->GetPartParam().GetSubtypeEntries<CharacterPart>();
    REQUIRE_FALSE(allParts.empty());
    REQUIRE_FALSE(characters.empty());

    auto* chr = characters[0];

    SUBCASE("clear draw parent")
    {
        chr->SetDrawParent(nullptr);
        CHECK(chr->GetDrawParent() == nullptr);
    }

    SUBCASE("set draw parent to another part")
    {
        auto* firstPart = allParts[0];
        chr->SetDrawParent(firstPart);
        REQUIRE(chr->GetDrawParent() != nullptr);
        CHECK(chr->GetDrawParent()->GetNameUTF8() == firstPart->GetNameUTF8());
    }
}

// ============================================================================
// Round-trip: write -> re-read -> compare
// ============================================================================

TEST_CASE("MSB: round-trip preserves param counts")
{
    auto& original = GetOriginalMSB();
    const auto writePath = GetOutDir() / "m12_02_00_00_roundtrip.msb";
    original.WriteToFilePath(writePath);
    auto reloaded = MSB::FromPath(writePath);
    REQUIRE(reloaded != nullptr);

    CHECK(original.GetModelParam().GetSize()  == reloaded->GetModelParam().GetSize());
    CHECK(original.GetEventParam().GetSize()  == reloaded->GetEventParam().GetSize());
    CHECK(original.GetRegionParam().GetSize() == reloaded->GetRegionParam().GetSize());
    CHECK(original.GetRouteParam().GetSize()  == reloaded->GetRouteParam().GetSize());
    CHECK(original.GetPartParam().GetSize()   == reloaded->GetPartParam().GetSize());
}

TEST_CASE("MSB: round-trip preserves entry names")
{
    auto& original = GetOriginalMSB();
    const auto writePath = GetOutDir() / "m12_02_00_00_roundtrip.msb";
    original.WriteToFilePath(writePath);
    auto reloaded = MSB::FromPath(writePath);
    REQUIRE(reloaded != nullptr);

    SUBCASE("part names")
    {
        auto partsA = original.GetPartParam().GetAllEntries();
        auto partsB = reloaded->GetPartParam().GetAllEntries();
        REQUIRE(partsA.size() == partsB.size());
        for (size_t i = 0; i < partsA.size(); ++i)
            CHECK(partsA[i]->GetNameUTF8() == partsB[i]->GetNameUTF8());
    }

    SUBCASE("model names")
    {
        auto a = original.GetModelParam().GetAllEntries();
        auto b = reloaded->GetModelParam().GetAllEntries();
        REQUIRE(a.size() == b.size());
        for (size_t i = 0; i < a.size(); ++i)
            CHECK(a[i]->GetNameUTF8() == b[i]->GetNameUTF8());
    }

    SUBCASE("event names")
    {
        auto a = original.GetEventParam().GetAllEntries();
        auto b = reloaded->GetEventParam().GetAllEntries();
        REQUIRE(a.size() == b.size());
        for (size_t i = 0; i < a.size(); ++i)
            CHECK(a[i]->GetNameUTF8() == b[i]->GetNameUTF8());
    }

    SUBCASE("region names")
    {
        auto a = original.GetRegionParam().GetAllEntries();
        auto b = reloaded->GetRegionParam().GetAllEntries();
        REQUIRE(a.size() == b.size());
        for (size_t i = 0; i < a.size(); ++i)
            CHECK(a[i]->GetNameUTF8() == b[i]->GetNameUTF8());
    }
}

TEST_CASE("MSB: round-trip preserves character fields")
{
    auto& original = GetOriginalMSB();
    const auto writePath = GetOutDir() / "m12_02_00_00_roundtrip.msb";
    original.WriteToFilePath(writePath);
    auto reloaded = MSB::FromPath(writePath);
    REQUIRE(reloaded != nullptr);

    auto charsOrig     = original.GetPartParam().GetSubtypeEntries<CharacterPart>();
    auto charsReloaded = reloaded->GetPartParam().GetSubtypeEntries<CharacterPart>();
    REQUIRE(charsOrig.size() == charsReloaded.size());

    for (size_t i = 0; i < charsOrig.size(); ++i)
    {
        CHECK(charsOrig[i]->GetAiId()        == charsReloaded[i]->GetAiId());
        CHECK(charsOrig[i]->GetCharacterId() == charsReloaded[i]->GetCharacterId());
        CHECK(charsOrig[i]->GetTalkId()      == charsReloaded[i]->GetTalkId());
        CHECK(charsOrig[i]->GetEntityId()    == charsReloaded[i]->GetEntityId());

        // Translate.
        auto [ox, oy, oz] = charsOrig[i]->GetTranslate();
        auto [rx, ry, rz] = charsReloaded[i]->GetTranslate();
        CHECK(ox == rx);
        CHECK(oy == ry);
        CHECK(oz == rz);

        // Draw parent cross-reference.
        const auto* dpOrig     = charsOrig[i]->GetDrawParent();
        const auto* dpReloaded = charsReloaded[i]->GetDrawParent();
        if (dpOrig)
        {
            REQUIRE(dpReloaded != nullptr);
            CHECK(dpOrig->GetNameUTF8() == dpReloaded->GetNameUTF8());
        }
        else
        {
            CHECK(dpReloaded == nullptr);
        }
    }
}

TEST_CASE("MSB: round-trip preserves collision fields")
{
    auto& original = GetOriginalMSB();
    const auto writePath = GetOutDir() / "m12_02_00_00_roundtrip.msb";
    original.WriteToFilePath(writePath);
    auto reloaded = MSB::FromPath(writePath);
    REQUIRE(reloaded != nullptr);

    auto colsOrig     = original.GetPartParam().GetSubtypeEntries<CollisionPart>();
    auto colsReloaded = reloaded->GetPartParam().GetSubtypeEntries<CollisionPart>();
    REQUIRE(colsOrig.size() == colsReloaded.size());

    for (size_t i = 0; i < colsOrig.size(); ++i)
    {
        CHECK(colsOrig[i]->GetHitFilterId()  == colsReloaded[i]->GetHitFilterId());
        CHECK(colsOrig[i]->GetPlayRegionId() == colsReloaded[i]->GetPlayRegionId());
        CHECK(colsOrig[i]->GetEntityId()     == colsReloaded[i]->GetEntityId());
    }
}

TEST_CASE("MSB: round-trip preserves asset fields")
{
    auto& original = GetOriginalMSB();
    const auto writePath = GetOutDir() / "m12_02_00_00_roundtrip.msb";
    original.WriteToFilePath(writePath);
    auto reloaded = MSB::FromPath(writePath);
    REQUIRE(reloaded != nullptr);

    auto assetsOrig     = original.GetPartParam().GetSubtypeEntries<AssetPart>();
    auto assetsReloaded = reloaded->GetPartParam().GetSubtypeEntries<AssetPart>();
    REQUIRE(assetsOrig.size() == assetsReloaded.size());

    for (size_t i = 0; i < assetsOrig.size(); ++i)
    {
        CHECK(assetsOrig[i]->GetEntityId()           == assetsReloaded[i]->GetEntityId());
        CHECK(assetsOrig[i]->GetSfxParamRelativeId() == assetsReloaded[i]->GetSfxParamRelativeId());
    }
}

TEST_CASE("MSB: round-trip preserves region fields")
{
    auto& original = GetOriginalMSB();
    const auto writePath = GetOutDir() / "m12_02_00_00_roundtrip.msb";
    original.WriteToFilePath(writePath);
    auto reloaded = MSB::FromPath(writePath);
    REQUIRE(reloaded != nullptr);

    auto regionsOrig     = original.GetRegionParam().GetAllEntries();
    auto regionsReloaded = reloaded->GetRegionParam().GetAllEntries();
    REQUIRE(regionsOrig.size() == regionsReloaded.size());

    for (size_t i = 0; i < regionsOrig.size(); ++i)
    {
        CHECK(regionsOrig[i]->GetEntityId()  == regionsReloaded[i]->GetEntityId());
        CHECK(regionsOrig[i]->GetType()      == regionsReloaded[i]->GetType());
        CHECK(regionsOrig[i]->GetShapeType() == regionsReloaded[i]->GetShapeType());
    }
}

TEST_CASE("MSB: round-trip preserves event fields")
{
    auto& original = GetOriginalMSB();
    const auto writePath = GetOutDir() / "m12_02_00_00_roundtrip.msb";
    original.WriteToFilePath(writePath);
    auto reloaded = MSB::FromPath(writePath);
    REQUIRE(reloaded != nullptr);

    auto eventsOrig     = original.GetEventParam().GetAllEntries();
    auto eventsReloaded = reloaded->GetEventParam().GetAllEntries();
    REQUIRE(eventsOrig.size() == eventsReloaded.size());

    for (size_t i = 0; i < eventsOrig.size(); ++i)
    {
        CHECK(eventsOrig[i]->GetEntityId() == eventsReloaded[i]->GetEntityId());
        CHECK(eventsOrig[i]->GetType()     == eventsReloaded[i]->GetType());
    }
}

// ============================================================================
// Stable serialization (double write)
// ============================================================================

TEST_CASE("MSB: double write produces byte-identical files")
{
    auto& original = GetOriginalMSB();
    const auto path1 = GetOutDir() / "stable_write_1.msb";
    const auto path2 = GetOutDir() / "stable_write_2.msb";

    // First write.
    original.WriteToFilePath(path1);
    REQUIRE(exists(path1));

    // Re-read and second write.
    auto reloaded = MSB::FromPath(path1);
    REQUIRE(reloaded != nullptr);
    reloaded->WriteToFilePath(path2);
    REQUIRE(exists(path2));

    // Byte-for-byte comparison.
    auto bytes1 = LoadFile(path1);
    auto bytes2 = LoadFile(path2);
    REQUIRE(bytes1.size() == bytes2.size());

    bool identical = (memcmp(bytes1.data(), bytes2.data(), bytes1.size()) == 0);
    if (!identical)
    {
        for (size_t i = 0; i < bytes1.size(); ++i)
        {
            if (bytes1[i] != bytes2[i])
            {
                FAIL_CHECK("First diff at byte offset 0x", std::hex, i,
                           ": 0x", static_cast<int>(bytes1[i]),
                           " vs 0x", static_cast<int>(bytes2[i]));
                break;
            }
        }
    }
    CHECK(identical);
}
