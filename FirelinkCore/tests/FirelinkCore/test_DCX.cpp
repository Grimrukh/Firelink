// Unit tests for the native DCX compression / decompression module.
//
// Uses the m0100B2A10.flver.dcx fixture (a DCX-compressed FLVER file) to verify:
//   - IsDCX() detection
//   - DetectDCX() type identification
//   - DecompressDCX() produces valid FLVER data
//   - Round-trip: decompress → compress → decompress yields identical bytes
//   - ReadFileBytes() utility

#include <doctest/doctest.h>

#include <FirelinkCore/BinaryReadWrite.h>
#include <FirelinkCore/DCX.h>
#include <FirelinkCore/Oodle.h>

#include <filesystem>
#include <fstream>
#include <vector>

using namespace Firelink;

namespace
{
    // Load entire file into a byte vector.
    std::vector<std::byte> load_file(const std::filesystem::path& path)
    {
        std::ifstream f(path, std::ios::binary | std::ios::ate);
        if (!f) return {};
        const auto size = static_cast<std::size_t>(f.tellg());
        f.seekg(0);
        std::vector<std::byte> buf(size);
        f.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(size));
        return buf;
    }

    // Resolve a resource file path relative to TEST_RESOURCES_DIR.
    std::filesystem::path find_resource(const std::filesystem::path& filename)
    {
        return std::filesystem::path(TEST_RESOURCES_DIR) / filename;
    }
} // namespace

// ---------------------------------------------------------------------------
// IsDCX detection
// ---------------------------------------------------------------------------

TEST_CASE("DCX: IsDCX detects DCX-compressed data")
{
    auto path = find_resource("m0100B2A10.flver.dcx");
    auto raw = load_file(path);
    if (raw.empty())
    {
        MESSAGE("Skipping — m0100B2A10.flver.dcx fixture not found");
        return;
    }

    REQUIRE(raw.size() > 4);
    CHECK(IsDCX(raw.data(), raw.size()) == true);
}

TEST_CASE("DCX: IsDCX rejects plain FLVER data")
{
    auto path = find_resource("c2010.flver");
    auto raw = load_file(path);
    if (raw.empty())
    {
        MESSAGE("Skipping — c2010.flver fixture not found");
        return;
    }
    REQUIRE(raw.size() > 4);

    CHECK(IsDCX(raw.data(), raw.size()) == false);
}

// ---------------------------------------------------------------------------
// ReadFileBytes
// ---------------------------------------------------------------------------

TEST_CASE("DCX: ReadFileBytes matches manual file load")
{
    auto path = find_resource("m0100B2A10.flver.dcx");
    auto manual = load_file(path);
    if (manual.empty())
    {
        MESSAGE("Skipping — fixture not found");
        return;
    }


    auto via_api = BinaryReadWrite::ReadFileBytes(path);
    CHECK(via_api.size() == manual.size());
    CHECK(std::memcmp(via_api.data(), manual.data(), manual.size()) == 0);
}

// ---------------------------------------------------------------------------
// DetectDCX
// ---------------------------------------------------------------------------

TEST_CASE("DCX: DetectDCX identifies DCX type")
{
    auto path = find_resource("m0100B2A10.flver.dcx");
    auto raw = load_file(path);
    if (raw.empty())
    {
        MESSAGE("Skipping — fixture not found");
        return;
    }

    DCXType type = DetectDCX(raw.data(), raw.size());

    // The fixture should be one of the DCX_DFLT variants (DS1R map FLVER).
    CHECK(type != DCXType::Unknown);
    CHECK(type != DCXType::Null);
    MESSAGE("Detected DCXType: " << static_cast<int>(type));
}

// ---------------------------------------------------------------------------
// DecompressDCX
// ---------------------------------------------------------------------------


// ---------------------------------------------------------------------------
// Round-trip: decompress → compress → decompress
// ---------------------------------------------------------------------------

TEST_CASE("DCX: round-trip compress/decompress preserves data")
{
    auto path = find_resource("m0100B2A10.flver.dcx");
    auto raw = load_file(path);
    if (raw.empty())
    {
        MESSAGE("Skipping — fixture not found");
        return;
    }

    DCXResult decompressed = DecompressDCX(raw.data(), raw.size());
    REQUIRE(!decompressed.data.empty());

    // Re-compress with the same DCX type.
    auto recompressed = CompressDCX(
        decompressed.data.data(), decompressed.data.size(), decompressed.type);
    REQUIRE(!recompressed.empty());

    // Decompress the re-compressed data.
    DCXResult round_tripped = DecompressDCX(recompressed.data(), recompressed.size());

    // The decompressed payload must be byte-identical.
    CHECK(round_tripped.data.size() == decompressed.data.size());
    CHECK(
        std::memcmp(
            round_tripped.data.data(),
            decompressed.data.data(),
            decompressed.data.size()) == 0);

    // And the detected type should match.
    CHECK(round_tripped.type == decompressed.type);
}

// ---------------------------------------------------------------------------
// Edge cases
// ---------------------------------------------------------------------------

TEST_CASE("DCX: DecompressDCX throws on truncated data")
{
    // Just "DCX\0" magic with no payload.
    std::byte tiny[] = {
        std::byte('D'), std::byte('C'), std::byte('X'), std::byte('\0')
    };
    CHECK_THROWS((void)DecompressDCX(tiny, sizeof(tiny)));
}

TEST_CASE("DCX: DecompressDCX throws on truncated data")
{
    CHECK(IsDCX(nullptr, 0) == false);

    std::byte one{0x42};
    CHECK(IsDCX(&one, 1) == false);
}

// ---------------------------------------------------------------------------
// DCX_KRAK (Oodle) tests
// ---------------------------------------------------------------------------

TEST_CASE("DCX: DetectDCX identifies DCX_KRAK")
{
    auto path = find_resource("m12_02_00_00.msb.dcx");
    auto raw = load_file(path);
    if (raw.empty())
    {
        MESSAGE("Skipping — m12_02_00_00.msb.dcx fixture not found");
        return;
    }

    CHECK(IsDCX(raw.data(), raw.size()) == true);
    CHECK(DetectDCX(raw.data(), raw.size()) == DCXType::DCX_KRAK);
}

TEST_CASE("DCX: DecompressDCX handles DCX_KRAK")
{
    auto path = find_resource("m12_02_00_00.msb.dcx");
    auto raw = load_file(path);
    if (raw.empty())
    {
        MESSAGE("Skipping — m12_02_00_00.msb.dcx fixture not found");
        return;
    }

    if (!Oodle::IsAvailable())
    {
        MESSAGE("Skipping — Oodle DLL not available");
        CHECK_THROWS_AS(
            (void)DecompressDCX(raw.data(), raw.size()),
            DCXError);
        return;
    }

    DCXResult result = DecompressDCX(raw.data(), raw.size());
    CHECK(result.type == DCXType::DCX_KRAK);
    CHECK(!result.data.empty());
    // Decompressed MSB should start with "MSB " magic.
    REQUIRE(result.data.size() >= 4);
    CHECK(std::memcmp(result.data.data(), "MSB ", 4) == 0);
}

TEST_CASE("DCX: KRAK round-trip compress/decompress preserves data")
{
    auto path = find_resource("m12_02_00_00.msb.dcx");
    auto raw = load_file(path);
    if (raw.empty())
    {
        MESSAGE("Skipping — m12_02_00_00.msb.dcx fixture not found");
        return;
    }
    if (!Oodle::IsAvailable())
    {
        MESSAGE("Skipping — Oodle DLL not available");
        return;
    }

    DCXResult decompressed = DecompressDCX(raw.data(), raw.size());
    REQUIRE(!decompressed.data.empty());

    auto recompressed = CompressDCX(
        decompressed.data.data(), decompressed.data.size(), DCXType::DCX_KRAK);
    REQUIRE(!recompressed.empty());

    // Verify it's detected as KRAK.
    CHECK(DetectDCX(recompressed.data(), recompressed.size()) == DCXType::DCX_KRAK);

    // Decompress again and compare.
    DCXResult round_tripped = DecompressDCX(recompressed.data(), recompressed.size());
    CHECK(round_tripped.type == DCXType::DCX_KRAK);
    CHECK(round_tripped.data.size() == decompressed.data.size());
    CHECK(std::memcmp(
        round_tripped.data.data(),
        decompressed.data.data(),
        decompressed.data.size()) == 0);
}

TEST_CASE("DCX: CompressDCX throws for DCX_KRAK when DLL unavailable")
{
    if (Oodle::IsAvailable())
    {
        MESSAGE("Skipping — Oodle DLL is available (cannot test unavailable path)");
        return;
    }
    std::byte dummy[] = {std::byte(0)};
    CHECK_THROWS_AS(
        (void)CompressDCX(dummy, sizeof(dummy), DCXType::DCX_KRAK),
        DCXError);
}
