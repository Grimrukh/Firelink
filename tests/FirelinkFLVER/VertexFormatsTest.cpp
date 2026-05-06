// Tests for the vertex format table and decompress/compress codecs.

#include <doctest/doctest.h>

#include <FirelinkFLVER/VertexFormats.h>

#include <array>
#include <string>
#include <vector>

using namespace Firelink;

TEST_CASE("FindVertexFormatInfo returns Position/3f")
{
    auto info = FindVertexFormatInfo(VertexUsage::Position, VertexDataFormatEnum::ThreeFloats);
    REQUIRE(info.has_value());
    CHECK(info->field_count == 1);
    CHECK(info->GetTotalCompressedSize() == 12);
    CHECK(info->GetTotalDecompressedSize() == 12);
    CHECK(info->fields[0].codec == VertexCodec::Identity);
}

TEST_CASE("FindVertexFormatInfo returns nullopt for unknown pair")
{
    auto info = FindVertexFormatInfo(VertexUsage::Color, VertexDataFormatEnum::ThreeFloats);
    CHECK_FALSE(info.has_value());
}

TEST_CASE("FindVertexFormatInfo returns Normal/0x10 as dual-field")
{
    auto info = FindVertexFormatInfo(VertexUsage::Normal, VertexDataFormatEnum::FourBytesA);
    REQUIRE(info.has_value());
    CHECK(info->field_count == 2);
    CHECK(info->GetTotalCompressedSize() == 4); // 3 bytes + 1 byte
    CHECK(std::string(info->fields[0].name) == "normal");
    CHECK(info->fields[0].codec == VertexCodec::SignedIntTo127Float);
    CHECK(std::string(info->fields[1].name) == "normal_w");
    CHECK(info->fields[1].codec == VertexCodec::Identity);
}

TEST_CASE("DecompressField identity copies positions")
{
    auto info = FindVertexFormatInfo(
        VertexUsage::Position, VertexDataFormatEnum::ThreeFloats).value();
    const auto& spec = info.fields[0];

    constexpr std::size_t vertex_count = 3;
    constexpr std::size_t stride = 20;
    constexpr std::size_t position_offset = 4;

    std::array<std::byte, stride * vertex_count> compressed{};
    auto write_vec = [&](std::size_t v, float x, float y, float z)
    {
        float xyz[3] = {x, y, z};
        std::memcpy(&compressed[v * stride + position_offset], xyz, 12);
    };
    write_vec(0, 1.f, 2.f, 3.f);
    write_vec(1, 4.f, 5.f, 6.f);
    write_vec(2, 7.f, 8.f, 9.f);

    std::vector<std::byte> out(vertex_count * spec.GetDecompressedSize());
    DecompressField(
        spec, compressed.data(), out.data(),
        vertex_count, stride, position_offset, 2048.f);

    float positions[9];
    std::memcpy(positions, out.data(), sizeof(positions));
    CHECK(positions[0] == doctest::Approx(1.f));
    CHECK(positions[1] == doctest::Approx(2.f));
    CHECK(positions[2] == doctest::Approx(3.f));
    CHECK(positions[3] == doctest::Approx(4.f));
    CHECK(positions[6] == doctest::Approx(7.f));
    CHECK(positions[8] == doctest::Approx(9.f));
}

TEST_CASE("compress/decompress roundtrip for position floats")
{
    auto info = FindVertexFormatInfo(
        VertexUsage::Position, VertexDataFormatEnum::ThreeFloats).value();
    const auto& spec = info.fields[0];

    constexpr std::size_t vertex_count = 2;
    constexpr std::size_t stride = 12;

    float input[6] = {1.5f, 2.5f, 3.5f, -1.f, -2.f, -3.f};

    std::vector<std::byte> compressed(vertex_count * stride);
    CompressField(
        spec, reinterpret_cast<const std::byte*>(input),
        compressed.data(), vertex_count, stride, 0, 2048.f);

    std::vector<std::byte> decompressed(vertex_count * spec.GetDecompressedSize());
    DecompressField(
        spec, compressed.data(), decompressed.data(),
        vertex_count, stride, 0, 2048.f);

    float output[6];
    std::memcpy(output, decompressed.data(), sizeof(output));
    for (int i = 0; i < 6; ++i)
    {
        CHECK(output[i] == doctest::Approx(input[i]));
    }
}

TEST_CASE("SignedIntTo127Float decompress for normals")
{
    auto info = FindVertexFormatInfo(VertexUsage::Normal, VertexDataFormatEnum::FourBytesA);
    REQUIRE(info.has_value());
    const auto& normal_spec = info->fields[0]; // 3 U8 -> 3 F32

    // 1 vertex, stride = 4 (3 normal bytes + 1 normal_w)
    std::uint8_t raw[4] = {0, 127, 254, 200}; // normal[3] + normal_w
    std::vector<std::byte> out(3 * sizeof(float));

    DecompressField(
        normal_spec,
        reinterpret_cast<const std::byte*>(raw), out.data(),
        1, 4, 0, 0.f);

    float normals[3];
    std::memcpy(normals, out.data(), sizeof(normals));
    CHECK(normals[0] == doctest::Approx(-1.0f)); // (0 - 127) / 127
    CHECK(normals[1] == doctest::Approx(0.0f)); // (127 - 127) / 127
    CHECK(normals[2] == doctest::Approx(1.0f)); // (254 - 127) / 127
}

TEST_CASE("UvFactor decompress for UVs")
{
    auto info = FindVertexFormatInfo(VertexUsage::UV, VertexDataFormatEnum::TwoShorts);
    REQUIRE(info.has_value());
    const auto& uv_spec = info->fields[0];

    std::int16_t raw[2] = {2048, -1024};
    std::vector<std::byte> out(2 * sizeof(float));

    DecompressField(
        uv_spec,
        reinterpret_cast<const std::byte*>(raw), out.data(),
        1, 4, 0, 1024.f);

    float uvs[2];
    std::memcpy(uvs, out.data(), sizeof(uvs));
    CHECK(uvs[0] == doctest::Approx(2.0f));
    CHECK(uvs[1] == doctest::Approx(-1.0f));
}

TEST_CASE("IdentityWiden for BoneIndices U8 -> S32")
{
    auto info = FindVertexFormatInfo(VertexUsage::BoneIndices, VertexDataFormatEnum::FourBytesB);
    REQUIRE(info.has_value());
    const auto& spec = info->fields[0];

    std::uint8_t raw[4] = {0, 10, 200, 255};
    std::vector<std::byte> out(4 * sizeof(std::int32_t));

    DecompressField(
        spec,
        reinterpret_cast<const std::byte*>(raw), out.data(),
        1, 4, 0, 0.f);

    std::int32_t indices[4];
    std::memcpy(indices, out.data(), sizeof(indices));
    CHECK(indices[0] == 0);
    CHECK(indices[1] == 10);
    CHECK(indices[2] == 200);
    CHECK(indices[3] == 255);
}
