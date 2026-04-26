// Unit tests for DDS conversion (DDS → TGA, DDS → PNG).
//
// Converts every .dds fixture. to both TGA and PNG
// and verifies the output is non-empty and starts with the expected
// format signature.

#include <doctest/doctest.h>

#include <FirelinkTestHelpers.h>
#include <FirelinkCore/DDS.h>
#include <FirelinkCore/Logging.h>

#include <filesystem>
#include <fstream>
#include <vector>

using namespace Firelink;

namespace
{
    /// Collect every .dds file in the test resources directory.
    std::vector<std::filesystem::path> collect_dds_files()
    {
        std::vector<std::filesystem::path> files;
        for (const std::string& relative_dir : { "darksouls1r/dds", "eldenring/dds" })
        {
            auto dir = GetResourcePath(relative_dir);
            if (!std::filesystem::is_directory(dir)) return files;

            for (const auto& entry : std::filesystem::directory_iterator(dir))
            {
                if (entry.is_regular_file() && entry.path().extension() == ".dds")
                    files.push_back(entry.path());
            }
        }

        std::sort(files.begin(), files.end());
        return files;
    }

    // TGA files start with an 18-byte header. Byte 2 is the image type:
    //   2 = uncompressed true-color, 3 = uncompressed grayscale, etc.
    bool LooksLikeTGA(const std::vector<std::byte>& data)
    {
        if (data.size() < 18) return false;
        const auto imageType = static_cast<uint8_t>(data[2]);
        // Uncompressed true-color (2), uncompressed grayscale (3), or
        // RLE true-color (10), RLE grayscale (11).
        return imageType == 2 || imageType == 3 || imageType == 10 || imageType == 11;
    }

    // PNG files start with an 8-byte magic: 89 50 4E 47 0D 0A 1A 0A
    bool LooksLikePNG(const std::vector<std::byte>& data)
    {
        static constexpr std::byte magic[] = {
            std::byte{0x89}, std::byte{0x50}, std::byte{0x4E}, std::byte{0x47},
            std::byte{0x0D}, std::byte{0x0A}, std::byte{0x1A}, std::byte{0x0A},
        };
        if (data.size() < sizeof(magic)) return false;
        return std::memcmp(data.data(), magic, sizeof(magic)) == 0;
    }

    // DDS files start with a 4-byte magic: "DDS " (0x20534444)
    bool LooksLikeDDS(const std::vector<std::byte>& data)
    {
        if (data.size() < 4) return false;
        return data[0] == std::byte{'D'}
            && data[1] == std::byte{'D'}
            && data[2] == std::byte{'S'}
            && data[3] == std::byte{' '};
    }
} // namespace

// ---------------------------------------------------------------------------
// DDS → TGA
// ---------------------------------------------------------------------------

TEST_CASE("DDS: ConvertDDSToTGA converts every fixture")
{
    auto files = collect_dds_files();
    if (files.empty())
    {
        MESSAGE("Skipping — no .dds fixtures found.");
        return;
    }

    for (const auto& path : files)
    {
        CAPTURE(path.filename().string());

        Info("Loading DDS: " + path.string());
        auto dds = LoadFile(path);
        REQUIRE(!dds.empty());

        Info("Converting DDS to TGA: " + path.string());
        auto tga = ConvertDDSToTGA(dds.data(), dds.size());
        CHECK(!tga.empty());
        CHECK(LooksLikeTGA(tga));
    }
}

// ---------------------------------------------------------------------------
// DDS → PNG
// ---------------------------------------------------------------------------

TEST_CASE("DDS: ConvertDDSToPNG converts every fixture")
{
    auto files = collect_dds_files();
    if (files.empty())
    {
        MESSAGE("Skipping — no .dds fixtures found.");
        return;
    }

    for (const auto& path : files)
    {
        CAPTURE(path.filename().string());

        auto dds = LoadFile(path);
        REQUIRE(!dds.empty());

        auto png = ConvertDDSToPNG(dds.data(), dds.size());
        CHECK(!png.empty());
        CHECK(LooksLikePNG(png));
    }
}

// ---------------------------------------------------------------------------
// Round-trip: DDS → TGA → DDS (BC7_UNORM)
// ---------------------------------------------------------------------------

TEST_CASE("DDS: round-trip DDS -> TGA -> DDS produces valid DDS")
{
    auto files = collect_dds_files();
    if (files.empty())
    {
        MESSAGE("Skipping — no .dds fixtures found.");
        return;
    }

    for (const auto& path : files)
    {
        CAPTURE(path.filename().string());

        auto dds = LoadFile(path);
        REQUIRE(!dds.empty());

        auto tga = ConvertDDSToTGA(dds.data(), dds.size());
        REQUIRE(!tga.empty());

        auto roundtrip = ConvertTGAToDDS(tga.data(), tga.size(),
                                         DXGI_FORMAT_BC7_UNORM);
        CHECK(!roundtrip.empty());
        CHECK(LooksLikeDDS(roundtrip));
    }
}

// ---------------------------------------------------------------------------
// Round-trip: DDS → PNG → DDS (BC7_UNORM)
// ---------------------------------------------------------------------------

TEST_CASE("DDS: round-trip DDS -> PNG -> DDS produces valid DDS")
{
    auto files = collect_dds_files();
    if (files.empty())
    {
        MESSAGE("Skipping — no .dds fixtures found.");
        return;
    }

    for (const auto& path : files)
    {
        CAPTURE(path.filename().string());

        auto dds = LoadFile(path);
        REQUIRE(!dds.empty());

        auto png = ConvertDDSToPNG(dds.data(), dds.size());
        REQUIRE(!png.empty());

        auto roundtrip = ConvertPNGToDDS(png.data(), png.size(),
                                         DXGI_FORMAT_BC7_UNORM);
        CHECK(!roundtrip.empty());
        CHECK(LooksLikeDDS(roundtrip));
    }
}

