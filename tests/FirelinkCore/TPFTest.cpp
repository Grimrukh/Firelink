// Unit tests for the TPF (Texture Pack File) reader/writer.
//
// Uses c1200.tpf fixture for basic tests, and extracts TPFs from
// the c2300.chrbnd/chrtpfbdt split binder for additional coverage.

#include <doctest/doctest.h>

#include <FirelinkTestHelpers.h>
#include <FirelinkCore/Binder.h>
#include <FirelinkCore/DDS.h>
#include <FirelinkCore/Paths.h>
#include <FirelinkCore/TPF.h>
#include "FirelinkCoreTestHelpers.h"

#include <cstdint>
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

    CHECK(tpf->GetPlatform() == TPFPlatform::PC);
    CHECK(tpf->Textures().size() > 0);
    MESSAGE("c1200.tpf texture count: " << tpf->Textures().size());

    for (const auto& tex : tpf->Textures())
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

    TPF::CPtr reread = TPF::FromBytes(written);
    REQUIRE(reread->Textures().size() == tpf->Textures().size());
    CHECK(reread->GetPlatform() == tpf->GetPlatform());
    CHECK(reread->GetFlags() == tpf->GetFlags());
    CHECK(reread->GetEncodingType() == tpf->GetEncodingType());

    for (std::size_t i = 0; i < tpf->Textures().size(); ++i)
    {
        const auto& a = tpf->Textures()[i];
        const auto& b = reread->Textures()[i];
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
    for (auto& t : tpf->Textures())
        if (t.texture_flags == 2 || t.texture_flags == 3)
            any_compressed = true;

    auto written1 = tpf->ToBytes();
    TPF::CPtr reread = TPF::FromBytes(written1);
    auto written2 = reread->ToBytes();

    if (!any_compressed)
    {
        CHECK(written1.size() == written2.size());
        if (written1.size() == written2.size())
            CHECK(std::memcmp(written1.data(), written2.data(), written1.size()) == 0);
    }
    else
    {
        TPF::CPtr reread2 = TPF::FromBytes(written2);
        CHECK(reread2->Textures().size() == tpf->Textures().size());
    }
}

// ---------------------------------------------------------------------------
// TPF extracted from split binder c2300.chrtpfbhd/bdt
// ---------------------------------------------------------------------------

TEST_CASE("TPF: read TPF from c2300 split binder")
{
    // Load split binder.
    auto binder = LoadSplitChrtpfbxf(
        "darksouls1r/c2300.chrbnd", "darksouls1r/c2300.chrtpfbdt");

    REQUIRE(binder->Entries().size() > 0);

    // Find and parse first TPF entry.
    for (auto& entry : binder->Entries())
    {
        TPF::CPtr tpf = TPF::FromBytes(entry->GetData());
        CHECK(tpf->GetPlatform() == TPFPlatform::PC);
        CHECK(tpf->Textures().size() > 0);
        if (!tpf->Textures().empty())
        {
            CHECK(!tpf->GetTexture(0).stem.empty());
            CHECK(!tpf->GetTexture(0).data.empty());
            MESSAGE("First TPF from split binder: " << tpf->GetTexture(0).stem
                << " (" << tpf->GetTexture(0).data.size() << " bytes)");
        }

        // Round-trip this tpf.
        auto written = tpf->ToBytes();
        TPF::CPtr reread = TPF::FromBytes(written);
        CHECK(reread->Textures().size() == tpf->Textures().size());
        return; // tested one TPF, done
    }

    MESSAGE("WARNING: No TPF entries found in c2300 split binder");
}

// ---------------------------------------------------------------------------
// Headerless console TPF: Demon's Souls (PS3) c1030.tpf
//
// PS3/Xbox 360 TPFs store the bare mip chain with no DDS header at all; the
// dimensions and format live in the TPF entry instead. `TPFTexture::ToDDS()`
// has to rebuild the header, or DirectXTex rejects the data outright with
// E_FAIL from LoadFromDDSMemory.
// ---------------------------------------------------------------------------

TEST_CASE("TPF: read headerless PS3 c1030.tpf (Demon's Souls)")
{
    auto tpf = LoadTPF("demonssouls/c1030.tpf");
    if (!tpf)
    {
        MESSAGE("Skipping — demonssouls/c1030.tpf not available");
        return;
    }

    CHECK(tpf->GetPlatform() == TPFPlatform::PS3);
    CHECK(tpf->GetFlags() == 1);
    CHECK(tpf->GetEncodingType() == 0);  // Shift-JIS stems
    REQUIRE(tpf->Textures().size() == 6);

    for (const auto& tex : tpf->Textures())
    {
        CAPTURE(tex.stem);
        CHECK(tex.stem.starts_with("c1030_"));
        CHECK(!tex.data.empty());
        CHECK(tex.platform == TPFPlatform::PS3);
        // Every PS3 texture here is headerless DXT1.
        CHECK(!tex.HasDDSHeader());
        CHECK(tex.format == 0);
        REQUIRE(tex.console_info.has_value());
        CHECK(tex.console_info->dxgi_format == DXGI_FORMAT_BC1_UNORM);
    }

    CHECK(tpf->FindTexture("c1030_demon") != nullptr);
    CHECK(tpf->FindTexture("c1030_demon_n") != nullptr);
    CHECK(tpf->FindTexture("c1030_demon_s") != nullptr);
    CHECK(tpf->FindTexture("c1030_ax") != nullptr);
    CHECK(tpf->FindTexture("c1030_ax_n") != nullptr);
    CHECK(tpf->FindTexture("c1030_ax_s") != nullptr);

    const auto* demon = tpf->FindTexture("c1030_demon");
    REQUIRE(demon != nullptr);
    CHECK(demon->console_info->width == 1024);
    CHECK(demon->console_info->height == 1024);
    CHECK(demon->mipmap_count == 11);

    // c1030_demon_n stores a full mip chain but reports a mipmap count of 0,
    // which means "derive the full chain from the dimensions".
    const auto* demonN = tpf->FindTexture("c1030_demon_n");
    REQUIRE(demonN != nullptr);
    CHECK(demonN->mipmap_count == 0);
}

TEST_CASE("TPF: headerless PS3 textures get a rebuilt DDS header and convert")
{
    auto tpf = LoadTPF("demonssouls/c1030.tpf");
    if (!tpf)
    {
        MESSAGE("Skipping — demonssouls/c1030.tpf not available");
        return;
    }

    for (const auto& tex : tpf->Textures())
    {
        CAPTURE(tex.stem);

        DDS dds;
        REQUIRE_NOTHROW(dds = tex.ToDDS());

        // Header is prepended: 128 bytes of magic + DDS_HEADER, no DX10 chunk for DXT1.
        REQUIRE(dds.GetSize() == tex.data.size() + 128);
        const auto& bytes = dds.GetBytes();
        CHECK(std::memcmp(bytes.data(), "DDS ", 4) == 0);
        CHECK(std::memcmp(bytes.data() + 84, "DXT1", 4) == 0);

        // The point of the exercise: DirectXTex can now decode it.
        auto png = dds.ToPNG();
        CHECK(!png.empty());

        auto tga = dds.ToTGA();
        CHECK(!tga.empty());
    }
}

TEST_CASE("TPF: rebuilt DDS header reports the real dimensions and mip count")
{
    auto tpf = LoadTPF("demonssouls/c1030.tpf");
    if (!tpf)
    {
        MESSAGE("Skipping — demonssouls/c1030.tpf not available");
        return;
    }

    const auto ReadU32 = [](const std::vector<std::byte>& b, const std::size_t offset)
    {
        std::uint32_t value = 0;
        std::memcpy(&value, b.data() + offset, sizeof(value));
        return value;
    };

    struct Expected { const char* stem; std::uint32_t size; std::uint32_t mips; };
    for (const auto& [stem, size, mips] : {
             Expected{"c1030_demon",   1024, 11},
             Expected{"c1030_demon_n", 1024, 11},  // mipmap_count 0 -> full chain
             Expected{"c1030_ax",       512, 10},
         })
    {
        CAPTURE(stem);
        const auto* tex = tpf->FindTexture(stem);
        REQUIRE(tex != nullptr);

        const DDS dds = tex->ToDDS();
        const auto& b = dds.GetBytes();
        CHECK(ReadU32(b, 12) == size);   // dwHeight
        CHECK(ReadU32(b, 16) == size);   // dwWidth
        CHECK(ReadU32(b, 28) == mips);   // dwMipMapCount
    }
}

TEST_CASE("TPF: ToDDS passes through textures that already have a header")
{
    auto tpf = LoadTPF("darksouls1r/c1200.tpf");
    if (!tpf || tpf->Textures().empty())
    {
        MESSAGE("Skipping — c1200.tpf not available");
        return;
    }

    for (const auto& tex : tpf->Textures())
    {
        CAPTURE(tex.stem);
        CHECK(tex.HasDDSHeader());
        const DDS dds = tex.ToDDS();
        REQUIRE(dds.GetSize() == tex.data.size());
        CHECK(std::memcmp(dds.GetBytes().data(), tex.data.data(), tex.data.size()) == 0);
    }
}

TEST_CASE("TPF: round-trip c1030.tpf (headerless PS3, big-endian)")
{
    auto tpf = LoadTPF("demonssouls/c1030.tpf");
    if (!tpf)
    {
        MESSAGE("Skipping — demonssouls/c1030.tpf not available");
        return;
    }

    const auto written = tpf->ToBytes();
    REQUIRE(written.size() >= 4);
    CHECK(std::memcmp(written.data(), "TPF\0", 4) == 0);

    const TPF::CPtr reread = TPF::FromBytes(written);
    CHECK(reread->GetPlatform() == tpf->GetPlatform());
    CHECK(reread->GetFlags() == tpf->GetFlags());
    REQUIRE(reread->Textures().size() == tpf->Textures().size());

    for (std::size_t i = 0; i < tpf->Textures().size(); ++i)
    {
        const auto& a = tpf->Textures()[i];
        const auto& b = reread->Textures()[i];
        CAPTURE(a.stem);
        CHECK(b.stem == a.stem);
        CHECK(b.format == a.format);
        CHECK(b.mipmap_count == a.mipmap_count);
        REQUIRE(b.console_info.has_value());
        CHECK(b.console_info->width == a.console_info->width);
        CHECK(b.console_info->height == a.console_info->height);
        CHECK(b.console_info->unk1 == a.console_info->unk1);
        CHECK(b.console_info->unk2 == a.console_info->unk2);
        REQUIRE(b.data.size() == a.data.size());
        CHECK(std::memcmp(b.data.data(), a.data.data(), a.data.size()) == 0);
    }
}

// ---------------------------------------------------------------------------
// Every texture in every loose TPF fixture must reach a decodable DDS
// ---------------------------------------------------------------------------

TEST_CASE("TPF: every texture in every loose TPF fixture converts to PNG")
{
    for (const char* name : {
             "darksouls1r/c1200.tpf",
             "darksouls1r/parts/Common_Body.tpf",
             "demonssouls/c1030.tpf",
         })
    {
        CAPTURE(name);
        auto tpf = LoadTPF(name);
        if (!tpf)
        {
            MESSAGE("Skipping — " << name << " not available");
            continue;
        }

        CHECK(tpf->Textures().size() > 0);
        for (const auto& tex : tpf->Textures())
        {
            CAPTURE(tex.stem);
            DDS dds;
            REQUIRE_NOTHROW(dds = tex.ToDDS());
            CHECK(std::memcmp(dds.GetBytes().data(), "DDS ", 4) == 0);
            CHECK(!dds.ToPNG().empty());
        }
    }
}

// ---------------------------------------------------------------------------
// FindTexture
// ---------------------------------------------------------------------------

TEST_CASE("TPF: FindTexture case-insensitive")
{
    auto tpf = LoadTPF("darksouls1r/c1200.tpf");
    if (!tpf || tpf->Textures().empty())
    {
        MESSAGE("Skipping — c1200.tpf not available");
        return;
    }

    std::string stem = tpf->GetTexture(0).stem;
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
    const auto* tiny = reinterpret_cast<const std::byte*>("XYZ");
    CHECK_THROWS((void)TPF::FromBytes(tiny, sizeof(tiny)));
}

// ---------------------------------------------------------------------------
// UTF-16 texture stems (Elden Ring TEXBND TPFs)
// ---------------------------------------------------------------------------

TEST_CASE("TPF: UTF-16 texture stems are decoded, not left as raw wide bytes")
{
    if (!IsOodleAvailable())
    {
        MESSAGE("Oodle not available; skipping Elden Ring TEXBND TPF test");
        return;
    }

    const auto texbnd = Binder::FromPath(GetResourcePath("eldenring/c6070_h.texbnd.dcx"));
    const auto tpfEntries = texbnd->FindEntriesByNameRegex(R"(.*\.tpf)", /*fullMatch*/ true);
    REQUIRE(tpfEntries.size() == 1);

    const auto tpf = TPF::FromBytes(tpfEntries[0]->GetData());
    // Encoding type 1 means stems are stored as UTF-16 in the file.
    REQUIRE(tpf->GetEncodingType() == 1);
    REQUIRE(tpf->Textures().size() == 4);

    for (const auto& tex : tpf->Textures())
    {
        // The decoding bug left every stem as raw UTF-16 bytes, i.e. twice as long
        // as it should be, with a null byte after each ASCII character.
        CHECK(tex.stem.find('\0') == std::string::npos);
        CHECK(tex.stem.starts_with("c6070_"));
    }

    CHECK(tpf->FindTexture("c6070_a") != nullptr);
    CHECK(tpf->FindTexture("c6070_n") != nullptr);
    CHECK(tpf->FindTexture("c6070_v") != nullptr);
    CHECK(tpf->FindTexture("c6070_1m") != nullptr);
}

TEST_CASE("TPF: UTF-16 texture stems survive a write round-trip")
{
    if (!IsOodleAvailable())
    {
        MESSAGE("Oodle not available; skipping Elden Ring TEXBND TPF round-trip test");
        return;
    }

    const auto texbnd = Binder::FromPath(GetResourcePath("eldenring/c6070_h.texbnd.dcx"));
    const auto tpfEntries = texbnd->FindEntriesByNameRegex(R"(.*\.tpf)", /*fullMatch*/ true);
    REQUIRE(tpfEntries.size() == 1);

    const auto tpf = TPF::FromBytes(tpfEntries[0]->GetData());

    const auto written = tpf->ToBytes();
    const TPF::CPtr reread = TPF::FromBytes(written);

    CHECK(reread->GetEncodingType() == tpf->GetEncodingType());
    REQUIRE(reread->Textures().size() == tpf->Textures().size());
    for (std::size_t i = 0; i < tpf->Textures().size(); ++i)
    {
        const auto& a = tpf->Textures()[i];
        const auto& b = reread->Textures()[i];
        CHECK(b.stem == a.stem);
        CHECK(b.stem.find('\0') == std::string::npos);
        REQUIRE(b.data.size() == a.data.size());
        CHECK(std::memcmp(b.data.data(), a.data.data(), a.data.size()) == 0);
    }

    // Writing the re-read TPF again must be byte-identical (stems are re-encoded the same way).
    const auto written2 = reread->ToBytes();
    REQUIRE(written2.size() == written.size());
    CHECK(std::memcmp(written2.data(), written.data(), written.size()) == 0);
}
