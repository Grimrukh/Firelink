// Unit tests for the Binder (BND3/BND4) archive reader/writer.
//
// Uses *.bnd test fixtures to verify:
//   - Entry count and entry properties
//   - Round-trip: read -> write -> re-read produces identical entries

#include <doctest/doctest.h>

#include <FirelinkTestHelpers.h>
#include <FirelinkCore/Binder.h>
#include <FirelinkCore/TPF.h>
#include "FirelinkCoreTestHelpers.h"

#include <filesystem>
#include <vector>

using namespace Firelink;

// ---------------------------------------------------------------------------
// Fixture: c2300.chrbnd (Elden Ring character binder, BND4, DCX_KRAK)
// ---------------------------------------------------------------------------

TEST_CASE("Binder: read c2300.chrbnd")
{
    auto binder = Binder::FromPath(GetResourcePath("darksouls1r/c2300.chrbnd"));
    if (!binder)
    {
        MESSAGE("Skipping — c2300.chrbnd not available (missing fixture or Oodle DLL)");
        return;
    }

    CHECK((binder->GetVersion() == BinderVersion::V3 || binder->GetVersion() == BinderVersion::V4));
    CHECK(binder->Entries().size() > 0);
    MESSAGE("c2300.chrbnd version: " << static_cast<int>(binder->GetVersion()));
    MESSAGE("c2300.chrbnd entry count: " << binder->Entries().size());

    // All entries should have IDs and paths.
    for (const auto& e : binder->Entries())
    {
        CHECK(e->GetEntryID() >= 0);
        CHECK(!e->GetPath().empty());
        CHECK(!e->GetData().empty());
    }
}

TEST_CASE("Binder: round-trip c2300.chrbnd")
{
    auto binder = Binder::FromPath(GetResourcePath("darksouls1r/c2300.chrbnd"));
    if (!binder)
    {
        MESSAGE("Skipping — c2300.chrbnd not available");
        return;
    }

    // Write -> re-read.
    auto written = binder->ToBytes();
    REQUIRE(!written.empty());

    const Binder::CPtr reread = Binder::FromBytes(written);
    REQUIRE(reread->Entries().size() == binder->Entries().size());

    for (std::size_t i = 0; i < binder->Entries().size(); ++i)
    {
        const auto& a = *binder->Entries()[i];
        const auto& b = *reread->Entries()[i];
        CHECK(a.GetEntryID() == b.GetEntryID());
        CHECK(a.GetPath() == b.GetPath());
        CHECK(a.GetFlags() == b.GetFlags());
        CHECK(a.GetData().size() == b.GetData().size());
        if (a.GetData().size() == b.GetData().size())
            CHECK(std::memcmp(a.GetData().data(), b.GetData().data(), a.GetData().size()) == 0);
    }
}

// ---------------------------------------------------------------------------
// Fixture: c2010.anibnd (Elden Ring animation binder, BND4, DCX_KRAK)
// ---------------------------------------------------------------------------

TEST_CASE("Binder: read c2010.anibnd")
{
    auto binder = Binder::FromPath(GetResourcePath("eldenring/c2010.anibnd"));
    if (!binder)
    {
        MESSAGE("Skipping — c2010.anibnd not available");
        return;
    }

    CHECK((binder->GetVersion() == BinderVersion::V3 || binder->GetVersion() == BinderVersion::V4));
    CHECK(binder->Entries().size() > 0);
    MESSAGE("c2010.anibnd version: " << static_cast<int>(binder->GetVersion()));
    MESSAGE("c2010.anibnd entry count: " << binder->Entries().size());
}

TEST_CASE("Binder: round-trip c2010.anibnd")
{
    auto binder = Binder::FromPath(GetResourcePath("eldenring/c2010.anibnd"));
    if (!binder)
    {
        MESSAGE("Skipping — c2010.anibnd not available");
        return;
    }

    auto written = binder->ToBytes();
    REQUIRE(!written.empty());

    Binder::CPtr reread = Binder::FromBytes(written);
    REQUIRE(reread->Entries().size() == binder->Entries().size());

    for (std::size_t i = 0; i < binder->Entries().size(); ++i)
    {
        const auto& a = *binder->Entries()[i];
        const auto& b = *reread->Entries()[i];
        CHECK(a.GetEntryID() == b.GetEntryID());
        CHECK(a.GetPath() == b.GetPath());
        CHECK(a.GetData().size() == b.GetData().size());
    }
}

// ---------------------------------------------------------------------------
// Double-write determinism
// ---------------------------------------------------------------------------

TEST_CASE("Binder: double-write produces identical bytes")
{
    auto binder = Binder::FromPath(GetResourcePath("darksouls1r/c2300.chrbnd"));
    if (!binder)
    {
        MESSAGE("Skipping — c2300.chrbnd not available");
        return;
    }

    auto written1 = binder->ToBytes();
    Binder::CPtr reread = Binder::FromBytes(written1);
    auto written2 = reread->ToBytes();

    CHECK(written1.size() == written2.size());
    if (written1.size() == written2.size())
        CHECK(std::memcmp(written1.data(), written2.data(), written1.size()) == 0);
}

// ---------------------------------------------------------------------------
// Split binder: c2300.chrbnd (BHD) + c2300.chrtpfbdt (texture pack BXF)
// ---------------------------------------------------------------------------

namespace
{

} // namespace

TEST_CASE("Binder: read split c2300.chrtpfbhd + chrtpfbdt")
{
    auto binder = LoadSplitChrtpfbxf("darksouls1r/c2300.chrbnd", "darksouls1r/c2300.chrtpfbdt");
    if (!binder)
    {
        MESSAGE("Skipping — c2300.chrbnd/chrtpfbdt not available (missing fixture or Oodle DLL)");
        return;
    }

    CHECK(binder->Entries().size() > 0);
    MESSAGE("c2300 split binder version: " << static_cast<int>(binder->GetVersion()));
    MESSAGE("c2300 split binder entry count: " << binder->Entries().size());

    // Entries should have paths and non-empty data.
    for (const auto& e : binder->Entries())
    {
        CHECK(!e->GetPath().empty());
        CHECK(!e->GetData().empty());
    }
}

TEST_CASE("Binder: split c2300.chrtpfbhd contains TPF entries")
{
    auto binder = LoadSplitChrtpfbxf("darksouls1r/c2300.chrbnd", "darksouls1r/c2300.chrtpfbdt");
    if (!binder)
    {
        MESSAGE("Skipping — c2300.chrbnd/chrtpfbdt not available");
        return;
    }

    // At least some entries should be TPF files.
    int tpf_count = 0;
    for (auto& entry : binder->Entries())
    {
        auto name = entry->GetPathName();
        bool is_tpf = false;

        // Check for .tpf or DCX-compressed .tpf
        if (entry->GetData().size() >= 4)
        {
            if (std::memcmp(entry->GetData().data(), "TPF\0", 4) == 0)
                is_tpf = true;
            else if (IsDCX(entry->GetData().data(), entry->GetData().size()))
            {
                // Try to decompress and check for TPF magic.
                try
                {
                    auto inner = DecompressDCX(entry->GetData().data(), entry->GetData().size());
                    if (inner.data.size() >= 4 && std::memcmp(inner.data.data(), "TPF\0", 4) == 0)
                        is_tpf = true;
                }
                catch (...) {}
            }
        }

        if (is_tpf)
        {
            tpf_count++;
            // Parse the first TPF to verify it works.
            if (tpf_count == 1)
            {
                const std::byte* tpf_data = entry->GetData().data();
                std::size_t tpf_size = entry->GetData().size();
                std::vector<std::byte> decompressed;

                if (IsDCX(tpf_data, tpf_size))
                {
                    auto inner = DecompressDCX(tpf_data, tpf_size);
                    decompressed = std::move(inner.data);
                    tpf_data = decompressed.data();
                    tpf_size = decompressed.size();
                }

                TPF::CPtr tpf = TPF::FromBytes(tpf_data, tpf_size);
                CHECK(tpf->Textures().size() > 0);
                MESSAGE("  First TPF has " << tpf->Textures().size() << " texture(s): " << tpf->Textures()[0].stem);
            }
        }
    }

    CHECK(tpf_count > 0);
    MESSAGE("Found " << tpf_count << " TPF entries in split binder");
}

// ---------------------------------------------------------------------------
// Entry-finding methods (c2300.chrbnd contains c2300.flver, c2300.hkx, c2300.tpf)
// ---------------------------------------------------------------------------

TEST_CASE("Binder: FindEntryByID returns correct entry")
{
    auto binder = Binder::FromPath(GetResourcePath("darksouls1r/c2300.chrbnd"));
    if (!binder) { MESSAGE("Skipping"); return; }

    // Grab the ID of the first entry and look it up.
    const auto& first = binder->Entries().front();
    auto found = binder->FindEntryByID(first->GetEntryID());
    CHECK(found->GetEntryID() == first->GetEntryID());
    CHECK(found->GetPath() == first->GetPath());

    // Non-existent ID throws.
    CHECK_THROWS_AS((void)binder->FindEntryByID(-999), BinderEntryNotFoundError);
}

TEST_CASE("Binder: FindEntryByName finds known entries")
{
    auto binder = Binder::FromPath(GetResourcePath("darksouls1r/c2300.chrbnd"));
    if (!binder) { MESSAGE("Skipping"); return; }

    auto flver = binder->FindEntryByName("c2300.flver");
    CHECK(flver->GetPathName() == "c2300.flver");
    CHECK(!flver->GetData().empty());

    auto hkx = binder->FindEntryByName("c2300.hkx");
    CHECK(hkx->GetPathName() == "c2300.hkx");

    // Unknown name throws.
    CHECK_THROWS_AS((void)binder->FindEntryByName("does_not_exist.xyz"), BinderEntryNotFoundError);
}

TEST_CASE("Binder: FindEntryByNameRegex finds a unique entry")
{
    auto binder = Binder::FromPath(GetResourcePath("darksouls1r/c2300.chrbnd"));
    if (!binder) { MESSAGE("Skipping"); return; }

    auto flver = binder->FindEntryByNameRegex(R"(.*\.flver)");
    CHECK(flver->GetPathName() == "c2300.flver");

    // Full-match variant.
    auto hkx = binder->FindEntryByNameRegex(R"(c2300\.hkx)", true);
    CHECK(hkx->GetPathName() == "c2300.hkx");
}

TEST_CASE("Binder: FindEntryByNameRegex throws on ambiguous pattern")
{
    auto binder = Binder::FromPath(GetResourcePath("darksouls1r/c2300.chrbnd"));
    if (!binder) { MESSAGE("Skipping"); return; }

    CHECK_THROWS_AS(
        (void)binder->FindEntryByNameRegex(R"(c2300\..+)"),
        MultipleBinderEntriesFoundError);
}

TEST_CASE("Binder: FindEntriesByNameRegex returns all matching entries")
{
    auto binder = Binder::FromPath(GetResourcePath("darksouls1r/c2300.chrbnd"));
    if (!binder) { MESSAGE("Skipping"); return; }

    auto matches = binder->FindEntriesByNameRegex(R"(c2300\..+)");
    CHECK(matches.size() >= 3);

    for (const auto& e : matches)
    {
        REQUIRE(e != nullptr);
        CHECK(e->GetPathStem() == "c2300");
    }

    auto none = binder->FindEntriesByNameRegex(R"(zzz_no_match)");
    CHECK(none.empty());
}

TEST_CASE("Binder: FindEntryByFilter finds a unique entry")
{
    auto binder = Binder::FromPath(GetResourcePath("darksouls1r/c2300.chrbnd"));
    if (!binder) { MESSAGE("Skipping"); return; }

    auto chrtpfbhd = binder->FindEntryByFilter(
        [](const BinderEntry& e) { return e.GetPathName() == "c2300.chrtpfbhd"; });
    CHECK(chrtpfbhd->GetPathName() == "c2300.chrtpfbhd");

    // Filter that matches nothing throws.
    CHECK_THROWS_AS(
        (void)binder->FindEntryByFilter([](const BinderEntry&) { return false; }),
        BinderEntryNotFoundError);
}

TEST_CASE("Binder: FindEntryByFilter throws on ambiguous filter")
{
    auto binder = Binder::FromPath(GetResourcePath("darksouls1r/c2300.chrbnd"));
    if (!binder) { MESSAGE("Skipping"); return; }

    CHECK_THROWS_AS(
        (void)binder->FindEntryByFilter([](const BinderEntry&) { return true; }),
        MultipleBinderEntriesFoundError);
}

TEST_CASE("Binder: FindEntriesByFilter returns all matching entries")
{
    auto binder = Binder::FromPath(GetResourcePath("darksouls1r/c2300.chrbnd"));
    if (!binder) { MESSAGE("Skipping"); return; }

    auto all = binder->FindEntriesByFilter([](const BinderEntry& e) { return !e.GetData().empty(); });
    CHECK(all.size() == binder->Entries().size());

    auto c2300 = binder->FindEntriesByFilter(
        [](const BinderEntry& e) { return e.GetPathStem() == "c2300"; });
    CHECK(c2300.size() >= 3);

    auto none = binder->FindEntriesByFilter([](const BinderEntry&) { return false; });
    CHECK(none.empty());
}

// ---------------------------------------------------------------------------
// Edge cases
// ---------------------------------------------------------------------------

TEST_CASE("Binder: FromBytes throws on invalid data")
{
    const auto* tiny = reinterpret_cast<const std::byte*>("XYZ");
    CHECK_THROWS_AS((void)Binder::FromBytes(tiny, sizeof(tiny)), BinderError);
}

TEST_CASE("Binder: FromBytes throws on truncated data")
{
    CHECK_THROWS((void)Binder::FromBytes(nullptr, 0));
}

