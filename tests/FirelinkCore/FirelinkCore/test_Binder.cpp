// Unit tests for the Binder (BND3/BND4) archive reader/writer.
//
// Uses *.bnd.dcx test fixtures to verify:
//   - Parsing DCX-compressed BND files
//   - Entry count and entry properties
//   - Round-trip: read → write → re-read produces identical entries

#include <doctest/doctest.h>

#include <FirelinkTestHelpers.h>
#include <FirelinkCore/Binder.h>
#include <FirelinkCore/DCX.h>
#include <FirelinkCore/Oodle.h>
#include <FirelinkCore/TPF.h>

#include <filesystem>
#include <fstream>
#include <vector>

using namespace Firelink;

namespace
{


    // Decompress a DCX file and parse as Binder.
    std::optional<Binder> LoadBinderDCX(const char* name)
    {
        auto path = GetResourcePath(name);
        auto raw = LoadFile(path);
        if (raw.empty()) return std::nullopt;

        // Detect DCX type; skip if KRAK and no Oodle.
        auto dcx_type = DetectDCX(raw.data(), raw.size());
        if (dcx_type == DCXType::DCX_KRAK && !Oodle::IsAvailable())
            return std::nullopt;

        auto result = DecompressDCX(raw.data(), raw.size());
        return Binder::FromBytes(result.data.data(), result.data.size());
    }
} // namespace

// ---------------------------------------------------------------------------
// Fixture: c2300.chrbnd.dcx (Elden Ring character binder, BND4, DCX_KRAK)
// ---------------------------------------------------------------------------

TEST_CASE("Binder: read c2300.chrbnd.dcx")
{
    auto binder = LoadBinderDCX("darksouls1r/c2300.chrbnd.dcx");
    if (!binder.has_value())
    {
        MESSAGE("Skipping — c2300.chrbnd.dcx not available (missing fixture or Oodle DLL)");
        return;
    }

    CHECK((binder->version == BinderVersion::V3 || binder->version == BinderVersion::V4));
    CHECK(binder->entries.size() > 0);
    MESSAGE("c2300.chrbnd.dcx version: " << static_cast<int>(binder->version));
    MESSAGE("c2300.chrbnd.dcx entry count: " << binder->entries.size());

    // All entries should have IDs and paths.
    for (const auto& e : binder->entries)
    {
        CHECK(e.entry_id >= 0);
        CHECK(!e.path.empty());
        CHECK(!e.data.empty());
    }
}

TEST_CASE("Binder: round-trip c2300.chrbnd.dcx")
{
    auto binder = LoadBinderDCX("darksouls1r/c2300.chrbnd.dcx");
    if (!binder.has_value())
    {
        MESSAGE("Skipping — c2300.chrbnd.dcx not available");
        return;
    }

    // Write → re-read.
    auto written = binder->ToBytes();
    REQUIRE(!written.empty());

    Binder reread = Binder::FromBytes(written.data(), written.size());
    REQUIRE(reread.entries.size() == binder->entries.size());

    for (std::size_t i = 0; i < binder->entries.size(); ++i)
    {
        const auto& a = binder->entries[i];
        const auto& b = reread.entries[i];
        CHECK(a.entry_id == b.entry_id);
        CHECK(a.path == b.path);
        CHECK(a.flags == b.flags);
        CHECK(a.data.size() == b.data.size());
        if (a.data.size() == b.data.size())
            CHECK(std::memcmp(a.data.data(), b.data.data(), a.data.size()) == 0);
    }
}

// ---------------------------------------------------------------------------
// Fixture: c2010.anibnd.dcx (Elden Ring animation binder, BND4, DCX_KRAK)
// ---------------------------------------------------------------------------

TEST_CASE("Binder: read c2010.anibnd.dcx")
{
    auto binder = LoadBinderDCX("eldenring/c2010.anibnd.dcx");
    if (!binder.has_value())
    {
        MESSAGE("Skipping — c2010.anibnd.dcx not available");
        return;
    }

    CHECK((binder->version == BinderVersion::V3 || binder->version == BinderVersion::V4));
    CHECK(binder->entries.size() > 0);
    MESSAGE("c2010.anibnd.dcx version: " << static_cast<int>(binder->version));
    MESSAGE("c2010.anibnd.dcx entry count: " << binder->entries.size());
}

TEST_CASE("Binder: round-trip c2010.anibnd.dcx")
{
    auto binder = LoadBinderDCX("eldenring/c2010.anibnd.dcx");
    if (!binder.has_value())
    {
        MESSAGE("Skipping — c2010.anibnd.dcx not available");
        return;
    }

    auto written = binder->ToBytes();
    REQUIRE(!written.empty());

    Binder reread = Binder::FromBytes(written.data(), written.size());
    REQUIRE(reread.entries.size() == binder->entries.size());

    for (std::size_t i = 0; i < binder->entries.size(); ++i)
    {
        const auto& a = binder->entries[i];
        const auto& b = reread.entries[i];
        CHECK(a.entry_id == b.entry_id);
        CHECK(a.path == b.path);
        CHECK(a.data.size() == b.data.size());
    }
}

// ---------------------------------------------------------------------------
// Double-write determinism
// ---------------------------------------------------------------------------

TEST_CASE("Binder: double-write produces identical bytes")
{
    auto binder = LoadBinderDCX("darksouls1r/c2300.chrbnd.dcx");
    if (!binder.has_value())
    {
        MESSAGE("Skipping — c2300.chrbnd.dcx not available");
        return;
    }

    auto written1 = binder->ToBytes();
    Binder reread = Binder::FromBytes(written1.data(), written1.size());
    auto written2 = reread.ToBytes();

    CHECK(written1.size() == written2.size());
    if (written1.size() == written2.size())
        CHECK(std::memcmp(written1.data(), written2.data(), written1.size()) == 0);
}

// ---------------------------------------------------------------------------
// Split binder: c2300.chrtpfbhd + c2300.chrtpfbdt (texture pack BXF)
// ---------------------------------------------------------------------------

namespace
{
    std::optional<Binder> load_split_binder(const char* bhd_name, const char* bdt_name)
    {
        auto bhd_path = GetResourcePath(bhd_name);
        auto bdt_path = GetResourcePath(bdt_name);
        auto bhd_raw = LoadFile(bhd_path);
        auto bdt_raw = LoadFile(bdt_path);
        if (bhd_raw.empty() || bdt_raw.empty()) return std::nullopt;

        // Both files may be DCX-compressed.
        std::vector<std::byte> bhd_data, bdt_data;
        if (IsDCX(bhd_raw.data(), bhd_raw.size()))
        {
            auto dcx_type = DetectDCX(bhd_raw.data(), bhd_raw.size());
            if (dcx_type == DCXType::DCX_KRAK && !Oodle::IsAvailable())
                return std::nullopt;
            bhd_data = DecompressDCX(bhd_raw.data(), bhd_raw.size()).data;
        }
        else
        {
            bhd_data = std::move(bhd_raw);
        }

        if (IsDCX(bdt_raw.data(), bdt_raw.size()))
        {
            auto dcx_type = DetectDCX(bdt_raw.data(), bdt_raw.size());
            if (dcx_type == DCXType::DCX_KRAK && !Oodle::IsAvailable())
                return std::nullopt;
            bdt_data = DecompressDCX(bdt_raw.data(), bdt_raw.size()).data;
        }
        else
        {
            bdt_data = std::move(bdt_raw);
        }

        return Binder::FromSplitBytes(
            bhd_data.data(), bhd_data.size(),
            bdt_data.data(), bdt_data.size());
    }
} // namespace

TEST_CASE("Binder: read split c2300.chrtpfbhd + chrtpfbdt")
{
    auto binder = load_split_binder("darksouls1r/c2300.chrtpfbhd", "darksouls1r/c2300.chrtpfbdt");
    if (!binder.has_value())
    {
        MESSAGE("Skipping — c2300.chrtpfbhd/bdt not available (missing fixture or Oodle DLL)");
        return;
    }

    CHECK(binder->entries.size() > 0);
    MESSAGE("c2300 split binder version: " << static_cast<int>(binder->version));
    MESSAGE("c2300 split binder entry count: " << binder->entries.size());

    // Entries should have paths and non-empty data.
    for (const auto& e : binder->entries)
    {
        CHECK(!e.path.empty());
        CHECK(!e.data.empty());
    }
}

TEST_CASE("Binder: split c2300.chrtpfbhd contains TPF entries")
{
    // TODO: CHRTPFBHD lives inside CHRBND.
    auto binder = load_split_binder("darksouls1r/c2300.chrtpfbhd", "darksouls1r/c2300.chrtpfbdt");
    if (!binder.has_value())
    {
        MESSAGE("Skipping — c2300.chrtpfbhd/bdt not available");
        return;
    }

    // At least some entries should be TPF files.
    int tpf_count = 0;
    for (auto& entry : binder->entries)
    {
        auto name = entry.name();
        bool is_tpf = false;

        // Check for .tpf or .tpf.dcx
        if (entry.data.size() >= 4)
        {
            if (std::memcmp(entry.data.data(), "TPF\0", 4) == 0)
                is_tpf = true;
            else if (IsDCX(entry.data.data(), entry.data.size()))
            {
                // Try to decompress and check for TPF magic.
                try
                {
                    auto inner = DecompressDCX(entry.data.data(), entry.data.size());
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
                const std::byte* tpf_data = entry.data.data();
                std::size_t tpf_size = entry.data.size();
                std::vector<std::byte> decompressed;

                if (IsDCX(tpf_data, tpf_size))
                {
                    auto inner = DecompressDCX(tpf_data, tpf_size);
                    decompressed = std::move(inner.data);
                    tpf_data = decompressed.data();
                    tpf_size = decompressed.size();
                }

                TPF tpf = TPF::FromBytes(tpf_data, tpf_size);
                CHECK(tpf.textures.size() > 0);
                MESSAGE("  First TPF has " << tpf.textures.size() << " texture(s): " << tpf.textures[0].stem);
            }
        }
    }

    CHECK(tpf_count > 0);
    MESSAGE("Found " << tpf_count << " TPF entries in split binder");
}

// ---------------------------------------------------------------------------
// Edge cases
// ---------------------------------------------------------------------------

TEST_CASE("Binder: FromBytes throws on invalid data")
{
    std::byte tiny[] = {std::byte('X'), std::byte('Y'), std::byte('Z'), std::byte('\0')};
    CHECK_THROWS_AS((void)Binder::FromBytes(tiny, sizeof(tiny)), BinderError);
}

TEST_CASE("Binder: FromBytes throws on truncated data")
{
    CHECK_THROWS((void)Binder::FromBytes(nullptr, 0));
}

