// Unit tests for PS4 GNF texture swizzle / deswizzle.
//
// Tests:
//  1. Morton index correctness — spot-checks against known values.
//  2. Round-trip: Swizzle(Deswizzle(x)) == x  and  Deswizzle(Swizzle(x)) == x
//     for several format / dimension combinations.
//  3. Known-value test: a hand-crafted 8×8 BC7 texture verifies exact block
//     placement after one deswizzle pass.
//  4. Bloodborne TPF: if a PS4 .tpf.dcx is present in test_resources/, read it,
//     deswizzle every texture, re-swizzle, and compare to the original bytes.

#include <doctest/doctest.h>

#include <FirelinkTestHelpers.h>
#include <FirelinkCore/Swizzle.h>
#include <FirelinkCore/TPF.h>

#include <algorithm>
#include <cstring>
#include <numeric>
#include <vector>

using namespace Firelink;

namespace
{
    // Fill a vector with ascending byte values (wrapping at 256).
    std::vector<std::byte> MakeLinearData(std::size_t size)
    {
        std::vector<std::byte> v(size);
        for (std::size_t i = 0; i < size; ++i)
            v[i] = std::byte(i & 0xFF);
        return v;
    }

    // AlignUp helper mirrored from the implementation so tests are self-contained.
    int AlignUp8(int v) { return (v + 7) & ~7; }

    // Expected swizzled size for one mip (no mip chain).
    std::size_t ExpectedSwizzledSize(int w, int h, int ppb, int bpb)
    {
        const int wb = std::max(1, (w + ppb - 1) / ppb);
        const int hb = std::max(1, (h + ppb - 1) / ppb);
        return static_cast<std::size_t>(AlignUp8(wb) * AlignUp8(hb) * bpb);
    }

    std::size_t ExpectedLinearSize(int w, int h, int ppb, int bpb)
    {
        const int wb = std::max(1, (w + ppb - 1) / ppb);
        const int hb = std::max(1, (h + ppb - 1) / ppb);
        return static_cast<std::size_t>(wb * hb * bpb);
    }
} // namespace

// =============================================================================
// Morton index
// =============================================================================

TEST_CASE("Swizzle: MortonIndex spot-checks")
{
    // The Z-order (Morton) index interleaves x bits in even positions and y
    // bits in odd positions: result bits = x0 y0 x1 y1 x2 y2 (LSB first).
    //   (0,0) -> 0b000000 = 0
    //   (1,0) -> 0b000001 = 1
    //   (0,1) -> 0b000010 = 2
    //   (1,1) -> 0b000011 = 3
    //   (2,0) -> 0b000100 = 4
    //   (3,0) -> 0b000101 = 5
    //   (7,7) -> 0b111111 = 63

    // We verify indirectly: swizzle a BC7 8×8 texture (1×1 blocks ≡ 1 tile
    // of exactly 64 blocks), then check that block (x, y) ends up at
    // swizzled position MortonIndex(x, y).

    // 8×8 blocks, 16 bytes / block → linear size = 64 * 16 = 1024
    constexpr int W = 32, H = 32; // 8×8 BC7 blocks
    constexpr int ppb = 4, bpb = 16;
    constexpr int wBlocks = W / ppb, hBlocks = H / ppb; // 8, 8
    constexpr int linSize = wBlocks * hBlocks * bpb;    // 1024
    constexpr int swSize  = linSize;                    // AlignUp8(8)*AlignUp8(8)*16 = 1024

    // Fill each block with its linear index (block number) as a repeated byte.
    std::vector<std::byte> linear(linSize);
    for (int b = 0; b < wBlocks * hBlocks; ++b)
        std::fill_n(linear.data() + b * bpb, bpb, std::byte(b & 0xFF));

    auto swizzled = SwizzlePS4(linear.data(), linear.size(), W, H, 1, 1, bpb, ppb);
    REQUIRE(swizzled.size() == swSize);

    // Verify each block lands at the Morton position.
    auto mortonIndex = [](int x, int y) {
        int r = 0;
        for (int bit = 0; bit < 3; ++bit)
        {
            r |= ((x >> bit) & 1) << (bit * 2);
            r |= ((y >> bit) & 1) << (bit * 2 + 1);
        }
        return r;
    };

    for (int by = 0; by < hBlocks; ++by)
    {
        for (int bx = 0; bx < wBlocks; ++bx)
        {
            const int linearIdx   = by * wBlocks + bx;
            const int mortonIdx   = mortonIndex(bx, by);
            const auto expected = static_cast<std::byte>(linearIdx & 0xFF);
            CHECK(swizzled[mortonIdx * bpb] == expected);
        }
    }
}

// =============================================================================
// Round-trip tests
// =============================================================================

TEST_CASE("Swizzle: deswizzle->swizzle round-trip (BC7, 32×32, 1 mip)")
{
    constexpr int W = 32, H = 32, mips = 1, faces = 1;
    constexpr int bpb = 16, ppb = 4;

    const std::size_t swSize = ExpectedSwizzledSize(W, H, ppb, bpb);
    auto swizzled = MakeLinearData(swSize);

    auto linear   = DeswizzlePS4(swizzled.data(), swizzled.size(), W, H, mips, faces, bpb, ppb);
    auto reswiz   = SwizzlePS4  (linear.data(),   linear.size(),   W, H, mips, faces, bpb, ppb);

    REQUIRE(reswiz.size() == swSize);
    CHECK(std::memcmp(reswiz.data(), swizzled.data(), swSize) == 0);
}

TEST_CASE("Swizzle: swizzle->deswizzle round-trip (BC1, 64×64, 3 mips)")
{
    constexpr int W = 64, H = 64, mips = 3, faces = 1;
    constexpr int bpb = 8, ppb = 4;

    const std::size_t linSize = ExpectedLinearSize(W, H, ppb, bpb)
                              + ExpectedLinearSize(W/2, H/2, ppb, bpb)
                              + ExpectedLinearSize(W/4, H/4, ppb, bpb);
    auto linear = MakeLinearData(linSize);

    auto swizzled = SwizzlePS4  (linear.data(),   linear.size(),   W, H, mips, faces, bpb, ppb);
    auto result   = DeswizzlePS4(swizzled.data(), swizzled.size(), W, H, mips, faces, bpb, ppb);

    REQUIRE(result.size() == linSize);
    CHECK(std::memcmp(result.data(), linear.data(), linSize) == 0);
}

TEST_CASE("Swizzle: round-trip (BC5, 16×16, 1 mip, 6 faces / cubemap)")
{
    constexpr int W = 16, H = 16, mips = 1, faces = 6;
    constexpr int bpb = 16, ppb = 4;

    const std::size_t linSize = faces * ExpectedLinearSize(W, H, ppb, bpb);
    auto linear = MakeLinearData(linSize);

    auto swizzled = SwizzlePS4  (linear.data(),   linear.size(),   W, H, mips, faces, bpb, ppb);
    auto result   = DeswizzlePS4(swizzled.data(), swizzled.size(), W, H, mips, faces, bpb, ppb);

    REQUIRE(result.size() == linSize);
    CHECK(std::memcmp(result.data(), linear.data(), linSize) == 0);
}

TEST_CASE("Swizzle: round-trip (R8G8B8A8_UNORM uncompressed, 32×32, 1 mip)")
{
    // Uncompressed: pixelsPerBlock = 1, bytesPerBlock = 4.
    constexpr int W = 32, H = 32, mips = 1, faces = 1;
    constexpr int bpb = 4, ppb = 1;

    const std::size_t linSize = ExpectedLinearSize(W, H, ppb, bpb);
    auto linear = MakeLinearData(linSize);

    auto swizzled = SwizzlePS4  (linear.data(),   linear.size(),   W, H, mips, faces, bpb, ppb);
    auto result   = DeswizzlePS4(swizzled.data(), swizzled.size(), W, H, mips, faces, bpb, ppb);

    REQUIRE(result.size() == linSize);
    CHECK(std::memcmp(result.data(), linear.data(), linSize) == 0);
}

// Non-power-of-two dimensions that aren't already multiples of 8 blocks.
TEST_CASE("Swizzle: round-trip (BC7, 20×12, 1 mip — non-multiple-of-8 block grid)")
{
    // 20×12 pixels → 5×3 blocks for BC7 (ppb=4)
    // Swizzled grid = AlignUp(5,8) × AlignUp(3,8) = 8×8 blocks
    constexpr int W = 20, H = 12, mips = 1, faces = 1;
    constexpr int bpb = 16, ppb = 4;

    const std::size_t linSize = ExpectedLinearSize(W, H, ppb, bpb);
    auto linear = MakeLinearData(linSize);

    auto swizzled = SwizzlePS4  (linear.data(),   linear.size(),   W, H, mips, faces, bpb, ppb);
    auto result   = DeswizzlePS4(swizzled.data(), swizzled.size(), W, H, mips, faces, bpb, ppb);

    REQUIRE(result.size() == linSize);
    CHECK(std::memcmp(result.data(), linear.data(), linSize) == 0);
}

// =============================================================================
// DXGI_FORMAT overloads
// =============================================================================

#ifdef _WIN32

TEST_CASE("Swizzle: PS4BytesPerBlock / PS4PixelsPerBlock")
{
    CHECK(PS4BytesPerBlock (DXGI_FORMAT_BC1_UNORM)      == 8);
    CHECK(PS4BytesPerBlock (DXGI_FORMAT_BC7_UNORM)      == 16);
    CHECK(PS4BytesPerBlock (DXGI_FORMAT_R8G8B8A8_UNORM) == 4);
    CHECK(PS4BytesPerBlock (DXGI_FORMAT_UNKNOWN)         == 0);

    CHECK(PS4PixelsPerBlock(DXGI_FORMAT_BC1_UNORM)      == 4);
    CHECK(PS4PixelsPerBlock(DXGI_FORMAT_BC7_UNORM)      == 4);
    CHECK(PS4PixelsPerBlock(DXGI_FORMAT_R8G8B8A8_UNORM) == 1);
}

TEST_CASE("Swizzle: DXGI_FORMAT overload round-trip (BC7, 32×32)")
{
    constexpr int W = 32, H = 32, mips = 1, faces = 1;
    const std::size_t swSize = ExpectedSwizzledSize(W, H, 4, 16);
    auto swizzled = MakeLinearData(swSize);

    auto linear = DeswizzlePS4(swizzled.data(), swizzled.size(),
                               W, H, mips, faces, DXGI_FORMAT_BC7_UNORM);
    auto reswiz  = SwizzlePS4 (linear.data(),   linear.size(),
                               W, H, mips, faces, DXGI_FORMAT_BC7_UNORM);

    REQUIRE(reswiz.size() == swSize);
    CHECK(std::memcmp(reswiz.data(), swizzled.data(), swSize) == 0);
}

TEST_CASE("Swizzle: unsupported DXGI_FORMAT throws")
{
    std::byte dummy{};
    CHECK_THROWS((void)DeswizzlePS4(&dummy, 1, 4, 4, 1, 1, DXGI_FORMAT_UNKNOWN));
    CHECK_THROWS((void)SwizzlePS4  (&dummy, 1, 4, 4, 1, 1, DXGI_FORMAT_UNKNOWN));
}

#endif // _WIN32

// =============================================================================
// Bloodborne PS4 TPF integration test
// =============================================================================

TEST_CASE("Swizzle: Bloodborne PS4 TPF deswizzle -> reswizzle round-trip")
{
    const auto tpf = TPF::FromPath(GetResourcePath("bloodborne/m21_00_ground_051_a.tpf.dcx"));
    if (!tpf)
    {
        MESSAGE("Skipping — bloodborne TPF not available");
        return;
    }

    // Confirm this is a PS4 TPF.
    if (tpf->GetPlatform() != TPFPlatform::PS4)
    {
        MESSAGE("Skipping — TPF platform is not PS4: "
                << static_cast<int>(tpf->GetPlatform()));
        return;
    }

    MESSAGE("Bloodborne TPF has " << tpf->TextureCount() << " texture(s)");

    for (std::size_t i = 0; i < tpf->TextureCount(); ++i)
    {
        const auto& tex = tpf->GetTexture(i);

        if (!tex.console_info.has_value())
        {
            MESSAGE("  [" << i << "] " << tex.stem << " — no console_info, skipping");
            continue;
        }

        const auto& ci      = *tex.console_info;
        const int   width   = ci.width;
        const int   height  = ci.height;
        const int   mips    = tex.mipmap_count > 0 ? tex.mipmap_count : 1;
        const int   faces   = (tex.texture_type == TextureType::Cubemap) ? 6 : 1;

        MESSAGE("  [" << i << "] " << tex.stem
                << " " << width << "x" << height
                << " mips=" << mips
                << " faces=" << faces
                << " dxgi=" << ci.dxgi_format
                << " rawBytes=" << tex.data.size());

        const auto dxgi = static_cast<DXGI_FORMAT>(ci.dxgi_format);
        const int  bpb  = PS4BytesPerBlock(dxgi);
        const int  ppb  = PS4PixelsPerBlock(dxgi);

        if (bpb == 0)
        {
            MESSAGE("    Unsupported DXGI_FORMAT " << ci.dxgi_format << ", skipping");
            continue;
        }

        // Deswizzle should not throw and produce a smaller or equal buffer.
        std::vector<std::byte> linear;
        CHECK_NOTHROW(linear = DeswizzlePS4(
            tex.data.data(), tex.data.size(),
            width, height, mips, faces, bpb, ppb));

        // Re-swizzle: should reproduce the original swizzled bytes exactly.
        std::vector<std::byte> reswizzled;
        CHECK_NOTHROW(reswizzled = SwizzlePS4(
            linear.data(), linear.size(),
            width, height, mips, faces, bpb, ppb));

        REQUIRE(reswizzled.size() <= tex.data.size());
        // Only compare the bytes our algorithm produced — tex.data may have
        // trailing GNF alignment padding beyond the last mip tile.
        CHECK(std::memcmp(reswizzled.data(), tex.data.data(), reswizzled.size()) == 0);
    }
}

