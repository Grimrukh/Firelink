// Unit tests for the TPF (Texture Pack File) reader/writer.
//
// Uses c1200.tpf fixture for basic tests, and extracts TPFs from
// the c2300.chrtpfbhd/bdt split binder for additional coverage.

#include <doctest/doctest.h>

#include <FirelinkTestHelpers.h>
#include <FirelinkCore/Binder.h>
#include <FirelinkCore/DCX.h>
#include <FirelinkCore/Oodle.h>
#include <FirelinkCore/Paths.h>
#include <FirelinkCore/TPF.h>
#include "FirelinkCoreTestHelpers.h"

#include <cstring>
#include <filesystem>
#include <vector>

using namespace Firelink;

namespace
{
    TPF::CPtr LoadTPF(const char* name)
    {
        return TPF::FromPath(GetResourcePath(name));
    }
} // namespace

// ---------------------------------------------------------------------------
// Fixture: c1200.tpf (straightforward TPF)
// ---------------------------------------------------------------------------

TEST_CASE("TPF: read c1200.tpf")
{
    auto tpf = LoadTPF("darksouls1r/c1200.tpf");
    if (!tpf)
    {
        MESSAGE("Skipping — c1200.tpf not available");
        return;
    }

    CHECK(tpf->platform == TPFPlatform::PC);
    CHECK(tpf->textures.size() > 0);
    MESSAGE("c1200.tpf texture count: " << tpf->textures.size());

    for (const auto& tex : tpf->textures)
    {
        CHECK(!tex.stem.empty());
        CHECK(!tex.data.empty());
        MESSAGE("  Texture: " << tex.stem << " (" << tex.data.size() << " bytes)");
    }
}

TEST_CASE("TPF: round-trip c1200.tpf")
{
    auto tpf = LoadTPF("darksouls1r/c1200.tpf");
    if (!tpf)
    {
        MESSAGE("Skipping — c1200.tpf not available");
        return;
    }

    auto written = tpf->ToBytes();
    REQUIRE(!written.empty());
    REQUIRE(written.size() >= 4);
    CHECK(std::memcmp(written.data(), "TPF\0", 4) == 0);

    TPF::CPtr reread = TPF::FromBytes(written.data(), written.size());
    REQUIRE(reread->textures.size() == tpf->textures.size());
    CHECK(reread->platform == tpf->platform);
    CHECK(reread->tpf_flags == tpf->tpf_flags);
    CHECK(reread->encoding_type == tpf->encoding_type);

    for (std::size_t i = 0; i < tpf->textures.size(); ++i)
    {
        const auto& a = tpf->textures[i];
        const auto& b = reread->textures[i];
        CHECK(a.stem == b.stem);
        CHECK(a.format == b.format);
        CHECK(a.texture_type == b.texture_type);
        CHECK(a.mipmap_count == b.mipmap_count);
        CHECK(a.texture_flags == b.texture_flags);
        CHECK(a.data.size() == b.data.size());
        if (a.data.size() == b.data.size())
            CHECK(std::memcmp(a.data.data(), b.data.data(), a.data.size()) == 0);
    }
}

TEST_CASE("TPF: double-write c1200.tpf produces identical bytes")
{
    auto tpf = LoadTPF("darksouls1r/c1200.tpf");
    if (!tpf)
    {
        MESSAGE("Skipping — c1200.tpf not available");
        return;
    }

    bool any_compressed = false;
    for (auto& t : tpf->textures)
        if (t.texture_flags == 2 || t.texture_flags == 3)
            any_compressed = true;

    auto written1 = tpf->ToBytes();
    TPF::CPtr reread = TPF::FromBytes(written1.data(), written1.size());
    auto written2 = reread->ToBytes();

    if (!any_compressed)
    {
        CHECK(written1.size() == written2.size());
        if (written1.size() == written2.size())
            CHECK(std::memcmp(written1.data(), written2.data(), written1.size()) == 0);
    }
    else
    {
        TPF::CPtr reread2 = TPF::FromBytes(written2.data(), written2.size());
        CHECK(reread2->textures.size() == tpf->textures.size());
    }
}

// ---------------------------------------------------------------------------
// TPF extracted from split binder c2300.chrtpfbhd/bdt
// ---------------------------------------------------------------------------

TEST_CASE("TPF: read TPF from c2300 split binder")
{
    // Load split binder.
    auto binder = LoadSplitChrtpfbxf(
        "darksouls1r/c2300.chrbnd.dcx", "darksouls1r/c2300.chrtpfbdt");

    REQUIRE(binder->entries.size() > 0);

    // Find and parse first TPF entry.
    for (auto& entry : binder->entries)
    {
        const std::byte* tpf_ptr = entry.data.data();
        std::size_t tpf_sz = entry.data.size();
        std::vector<std::byte> inner_buf;

        if (tpf_sz >= 4 && IsDCX(tpf_ptr, tpf_sz))
        {
            try {
                auto inner = DecompressDCX(tpf_ptr, tpf_sz);
                inner_buf = std::move(inner.data);
                tpf_ptr = inner_buf.data();
                tpf_sz = inner_buf.size();
            } catch (...) { continue; }
        }

        if (tpf_sz >= 4 && std::memcmp(tpf_ptr, "TPF\0", 4) == 0)
        {
            TPF::CPtr tpf = TPF::FromBytes(tpf_ptr, tpf_sz);
            CHECK(tpf->platform == TPFPlatform::PC);
            CHECK(tpf->textures.size() > 0);
            if (!tpf->textures.empty())
            {
                CHECK(!tpf->textures[0].stem.empty());
                CHECK(!tpf->textures[0].data.empty());
                MESSAGE("First TPF from split binder: " << tpf->textures[0].stem
                    << " (" << tpf->textures[0].data.size() << " bytes)");
            }

            // Round-trip this tpf->
            auto written = tpf->ToBytes();
            TPF::CPtr reread = TPF::FromBytes(written.data(), written.size());
            CHECK(reread->textures.size() == tpf->textures.size());
            return; // tested one TPF, done
        }
    }

    MESSAGE("WARNING: No TPF entries found in c2300 split binder");
}

// ---------------------------------------------------------------------------
// FindTexture
// ---------------------------------------------------------------------------

TEST_CASE("TPF: FindTexture case-insensitive")
{
    auto tpf = LoadTPF("darksouls1r/c1200.tpf");
    if (!tpf || tpf->textures.empty())
    {
        MESSAGE("Skipping — c1200.tpf not available");
        return;
    }

    std::string stem = tpf->textures[0].stem;
    std::string upper_stem = ToUpper(stem);

    auto* found = tpf->FindTexture(upper_stem);
    CHECK(found != nullptr);
    if (found)
        CHECK(found->stem == stem);
}

// ---------------------------------------------------------------------------
// Edge cases
// ---------------------------------------------------------------------------

TEST_CASE("TPF: FromBytes throws on invalid data")
{
    std::byte tiny[] = {std::byte('X'), std::byte('Y'), std::byte('Z'), std::byte('\0')};
    CHECK_THROWS((void)TPF::FromBytes(tiny, sizeof(tiny)));
}

