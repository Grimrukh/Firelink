// Unit tests for TextureFinder.
//
// Tests lazy texture discovery and caching using test resource fixtures.

#include <doctest/doctest.h>

#include <FirelinkTestHelpers.h>

#include <FirelinkFLVER/TextureFinder.h>

#include <FirelinkCore/DCX.h>
#include <FirelinkCore/Paths.h>

#include <filesystem>
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
    if (!IsOodleAvailable())
    {
        MESSAGE("Skipping — Oodle DLL not available");
        return;
    }

    // The test resources directory acts as a fake game data root.
    // c2300.chrbnd.dcx and c2300.chrtpfbdt are in the resources directory.
    auto res = GetResourcePath("darksouls1r");

    // Load the CHRBND so we can pass it as the flver_binder.
    auto chrbnd_path = GetResourcePath("darksouls1r/c2300.chrbnd.dcx");
    auto raw = LoadFile(chrbnd_path);
    if (raw.empty())
    {
        MESSAGE("Skipping — c2300.chrbnd.dcx not found");
        return;
    }

    auto dcx_result = DecompressDCX(raw.data(), raw.size());
    auto chrbnd = Binder::FromBytes(dcx_result.data.data(), dcx_result.data.size());

    TextureFinder mgr(GameType::DarkSoulsDSR, res.string());
    mgr.RegisterFLVERSources(chrbnd_path, chrbnd.get());

    // The CHRBND itself may contain TPF entries. Also, the CHRTPFBDT should be discovered.
    // Try to get any texture. We don't know exact names, but we can check the cache grew.
    MESSAGE("Cached textures after registering c2300: " << mgr.CachedTextureCount());
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
    TextureFinder mgr(GameType::DemonsSouls, GetResourcePath("darksouls1r"));

    // Register as if a loose character FLVER was in the resource dir.
    auto fake_flver_path = GetResourcePath("darksouls1r/c1200.flver");
    mgr.RegisterFLVERSources(fake_flver_path);

    // Now try to find a texture from c1200.tpf. We need to know at least
    // one texture stem from it.
    auto raw = LoadFile(tpf_path);
    auto tpf = TPF::FromBytes(raw.data(), raw.size());
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

    TextureFinder mgr(GameType::DemonsSouls, GetResourcePath("darksouls1r"));
    mgr.RegisterFLVERSources(GetResourcePath("darksouls1r/c1200.flver"));

    auto raw = LoadFile(tpf_path);
    auto tpf = TPF::FromBytes(raw.data(), raw.size());
    REQUIRE(tpf->Textures().size() > 0);

    // Search with upper-case stem.
    auto stem = tpf->GetTexture(0).stem;
    std::string upper_stem = ToUpper(stem);

    auto* tex = mgr.GetTexture(upper_stem);
    CHECK(tex != nullptr);
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

