// Unit tests for TextureFinder.
//
// Tests lazy texture discovery and caching using test resource fixtures.

#include <doctest/doctest.h>

#include <FirelinkTestHelpers.h>

#include <FirelinkFLVER/TextureFinder.h>

#include <FirelinkCore/Paths.h>

#include <filesystem>
#include <string>
#include <vector>

using namespace Firelink;
namespace fs = std::filesystem;

namespace
{
}

// ---------------------------------------------------------------------------
// Basic construction
// ---------------------------------------------------------------------------

TEST_CASE("TextureFinder: construct with invalid root doesn't throw")
{
    // Should not throw even with a nonexistent directory — it just won't find any textures.
    TextureFinder mgr(GameType::EldenRing, "C:/nonexistent/path");
    CHECK(mgr.CachedTextureCount() == 0);
}

// ---------------------------------------------------------------------------
// DSR character textures via CHRBND + CHRTPFBDT
// ---------------------------------------------------------------------------

TEST_CASE("TextureFinder: DSR character texture loading")
{
    // Character with CHRTPFBHD:
    static const std::string CHR_NAME = "c2060";

    // The test resources directory acts as a fake game data root.
    // c2300.chrbnd and c2300.chrtpfbdt are in the resources directory.
    auto res = GetResourcePath("darksouls1r");

    // Load the CHRBND so we can pass it as the flver_binder.
    auto chrbnd_path = GetResourcePath(std::format("darksouls1r/{}.chrbnd.dcx", CHR_NAME));
    auto raw = LoadFile(chrbnd_path);
    if (raw.empty())
    {
        MESSAGE(std::format("Skipping — {}.chrbnd.dcx not found", CHR_NAME));
        return;
    }

    const auto chrbnd = Binder::FromPath(chrbnd_path);

    TextureFinder mgr(GameType::DarkSoulsDSR, res.string());
    mgr.RegisterFLVERSources(chrbnd_path, chrbnd.get());

    auto pendingTpfNames = mgr.PendingTPFStems();
    CHECK(pendingTpfNames.size() > 0);

    const auto lowerCaseTexture = mgr.GetTexture("c2060_weapon_n");
    CHECK(lowerCaseTexture != nullptr);

    const auto mixedCaseTexture = mgr.GetTexture("c2060_Wp_a_0702_longspear_n");
    CHECK(mixedCaseTexture != nullptr);
}

// ---------------------------------------------------------------------------
// Standalone TPF texture lookup
// ---------------------------------------------------------------------------

TEST_CASE("TextureFinder: standalone TPF via loose file registration")
{
    auto tpf_path = GetResourcePath("darksouls1r/c1200.tpf");
    if (!fs::is_regular_file(tpf_path))
    {
        MESSAGE("Skipping — c1200.tpf not found");
        return;
    }

    // Create a manager and manually simulate loose TPF registration.
    // Since c1200.tpf is a loose TPF in the resources dir, we can test by using
    // a character FLVER source that would look for TPFs in the same directory.
    TextureFinder mgr(GameType::DarkSoulsDSR, GetResourcePath("darksouls1r"));

    // Register as if a loose character FLVER was in the resource dir.
    // NOTE: c1200.flver does not actually exist in test resources.
    auto fake_flver_path = GetResourcePath("darksouls1r/c1200.flver");
    mgr.RegisterFLVERSources(fake_flver_path);

    // Now try to find a texture from c1200.tpf. We need to know at least
    // one texture stem from it.
    auto raw = LoadFile(tpf_path);
    auto tpf = TPF::FromBytes(raw);
    REQUIRE(tpf->Textures().size() > 0);

    auto first_stem = tpf->GetTexture(0).stem;
    MESSAGE("Looking for texture: " << first_stem);

    // The manager should find it via lazy loading.
    auto* tex = mgr.GetTexture(first_stem);
    CHECK(tex != nullptr);
    if (tex)
    {
        CHECK(!tex->data.empty());
        CHECK(tex->stem == first_stem);
        MESSAGE("Found texture: " << tex->stem << " (" << tex->data.size() << " bytes)");
    }
}

// ---------------------------------------------------------------------------
// Case insensitivity
// ---------------------------------------------------------------------------

TEST_CASE("TextureFinder: case-insensitive texture lookup")
{
    auto tpf_path = GetResourcePath("darksouls1r/c1200.tpf");
    if (!fs::is_regular_file(tpf_path))
    {
        MESSAGE("Skipping — c1200.tpf not found");
        return;
    }

    TextureFinder mgr(GameType::DarkSoulsDSR, GetResourcePath("darksouls1r"));
    mgr.RegisterFLVERSources(GetResourcePath("darksouls1r/c1200.flver"));

    auto raw = LoadFile(tpf_path);
    auto tpf = TPF::FromBytes(raw);
    REQUIRE(tpf->Textures().size() > 0);

    // Search with upper-case stem.
    auto stem = tpf->GetTexture(0).stem;
    std::string upper_stem = ToUpper(stem);

    auto* tex = mgr.GetTexture(upper_stem);
    CHECK(tex != nullptr);
}

// ---------------------------------------------------------------------------
// Common 'parts' textures found in DS1R
// ---------------------------------------------------------------------------

TEST_CASE("TextureFinder: Common*.tpf 'parts' textures found for DS1R")
{
    TextureFinder mgr(GameType::DarkSoulsDSR, GetResourcePath("darksouls1r"));

    // Search with true stem.
    auto* tex = mgr.GetTexture("BD_M_Body_M_s");
    CHECK(tex != nullptr);
    // Search with lower-case stem.
    tex = mgr.GetTexture("bd_m_body_m_s");
    CHECK(tex != nullptr);
}

// ---------------------------------------------------------------------------
// Elden Ring character textures via CHRBND + adjacent TEXBND
// ---------------------------------------------------------------------------

namespace
{
    // All four texture stems inside 'c6070_h.texbnd.dcx'. The low-res TEXBND holds
    // the same four stems with an '_l' suffix.
    const std::vector<std::string> C6070_HI_RES_STEMS = {
        "c6070_a", "c6070_n", "c6070_v", "c6070_1m",
    };
} // namespace

TEST_CASE("TextureFinder: Elden Ring character textures found in adjacent TEXBND")
{
    if (!IsOodleAvailable())
    {
        MESSAGE("Oodle not available; skipping Elden Ring TEXBND test");
        return;
    }

    // The test resources directory acts as a fake game data root: 'c6070.chrbnd.dcx'
    // and its 'c6070_h/_l.texbnd.dcx' siblings all sit in 'eldenring'.
    const auto res = GetResourcePath("eldenring");
    const auto chrbndPath = GetResourcePath("eldenring/c6070.chrbnd.dcx");
    REQUIRE(fs::is_regular_file(chrbndPath));

    const auto chrbnd = Binder::FromPath(chrbndPath);

    TextureFinder mgr(GameType::EldenRing, res);
    mgr.RegisterFLVERSources(chrbndPath, chrbnd.get(), /*preferHiRes*/ true);

    // TEXBND TPFs are multi-texture, so they are loaded (not left pending) on registration.
    CHECK(mgr.CachedTextureCount() == C6070_HI_RES_STEMS.size());

    for (const auto& stem : C6070_HI_RES_STEMS)
    {
        const auto* tex = mgr.GetTexture(stem, "c6070");
        CHECK_MESSAGE(tex != nullptr, "Texture not found: " << stem);
        if (tex)
        {
            CHECK(tex->stem == stem);
            CHECK(!tex->data.empty());
        }
    }

    // Stems are matched case-insensitively.
    CHECK(mgr.GetTexture("C6070_A", "c6070") != nullptr);

    // Low-res stems are not in the hi-res TEXBND.
    CHECK(mgr.GetTexture("c6070_a_l", "c6070") == nullptr);
}

TEST_CASE("TextureFinder: Elden Ring low-res TEXBND used when hi-res not preferred")
{
    if (!IsOodleAvailable())
    {
        MESSAGE("Oodle not available; skipping Elden Ring TEXBND test");
        return;
    }

    const auto res = GetResourcePath("eldenring");
    const auto chrbndPath = GetResourcePath("eldenring/c6070.chrbnd.dcx");
    REQUIRE(fs::is_regular_file(chrbndPath));

    const auto chrbnd = Binder::FromPath(chrbndPath);

    TextureFinder mgr(GameType::EldenRing, res);
    mgr.RegisterFLVERSources(chrbndPath, chrbnd.get(), /*preferHiRes*/ false);

    for (const auto& stem : C6070_HI_RES_STEMS)
    {
        const auto lowResStem = stem + "_l";
        const auto* tex = mgr.GetTexture(lowResStem, "c6070");
        CHECK_MESSAGE(tex != nullptr, "Texture not found: " << lowResStem);
    }

    // Hi-res stems are not in the low-res TEXBND.
    CHECK(mgr.GetTexture("c6070_a", "c6070") == nullptr);
}

// ---------------------------------------------------------------------------
// Texture not found returns nullptr
// ---------------------------------------------------------------------------

TEST_CASE("TextureFinder: returns nullptr for missing texture")
{
    TextureFinder mgr(GameType::EldenRing, GetResourcePath("eldenring"));
    auto* tex = mgr.GetTexture("completely_nonexistent_texture_12345");
    CHECK(tex == nullptr);
}

