// Test FLVER write → re-read round-trip for FLVER2 fixtures.
//
// For each fixture, we:
//   1. Read the original FLVER from bytes.
//   2. Write it back to bytes via ToBytes().
//   3. Re-read the written bytes.
//   4. Compare the re-read FLVER against the original.
//
// We also test that writing and re-reading a programmatically constructed
// minimal FLVER0 produces identical results.

#include <doctest/doctest.h>

#include <FirelinkTestHelpers.h>
#include <FirelinkFLVER/FLVER.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

using namespace Firelink;

namespace
{
    // TODO: Add a FLVER0 fixture from DeS.
    const char* FLVER2_FIXTURES[] = {
        "eldenring/c2010.flver",
        "darksouls1r/m5020B2A10.flver",
        "darksouls1r/m5030B2A10.flver",
        "darksouls1r/m8101B2A10.flver",
    };

    // Compare two Bone vectors.
    void check_bones_equal(const std::vector<Bone>& a, const std::vector<Bone>& b)
    {
        REQUIRE(a.size() == b.size());
        for (std::size_t i = 0; i < a.size(); ++i)
        {
            CHECK(a[i].name == b[i].name);
            CHECK(a[i].translate.x == doctest::Approx(b[i].translate.x));
            CHECK(a[i].translate.y == doctest::Approx(b[i].translate.y));
            CHECK(a[i].translate.z == doctest::Approx(b[i].translate.z));
            CHECK(a[i].rotate.x == doctest::Approx(b[i].rotate.x));
            CHECK(a[i].rotate.y == doctest::Approx(b[i].rotate.y));
            CHECK(a[i].rotate.z == doctest::Approx(b[i].rotate.z));
            CHECK(a[i].scale.x == doctest::Approx(b[i].scale.x));
            CHECK(a[i].scale.y == doctest::Approx(b[i].scale.y));
            CHECK(a[i].scale.z == doctest::Approx(b[i].scale.z));
            CHECK(a[i].parent_bone_index == b[i].parent_bone_index);
            CHECK(a[i].child_bone_index == b[i].child_bone_index);
            CHECK(a[i].next_sibling_bone_index == b[i].next_sibling_bone_index);
            CHECK(a[i].previous_sibling_bone_index == b[i].previous_sibling_bone_index);
            CHECK(a[i].usage_flags == b[i].usage_flags);
        }
    }

    // Compare two Dummy vectors.
    void check_dummies_equal(const std::vector<Dummy>& a, const std::vector<Dummy>& b)
    {
        REQUIRE(a.size() == b.size());
        for (std::size_t i = 0; i < a.size(); ++i)
        {
            CHECK(a[i].translate.x == doctest::Approx(b[i].translate.x));
            CHECK(a[i].translate.y == doctest::Approx(b[i].translate.y));
            CHECK(a[i].translate.z == doctest::Approx(b[i].translate.z));
            CHECK(a[i].color.r == b[i].color.r);
            CHECK(a[i].color.g == b[i].color.g);
            CHECK(a[i].color.b == b[i].color.b);
            CHECK(a[i].color.a == b[i].color.a);
            CHECK(a[i].reference_id == b[i].reference_id);
            CHECK(a[i].parent_bone_index == b[i].parent_bone_index);
            CHECK(a[i].attach_bone_index == b[i].attach_bone_index);
        }
    }

    // Compare two Material instances (name, mat_def_path, texture count, GX item count).
    void check_material_equal(const Material& a, const Material& b)
    {
        CHECK(a.name == b.name);
        CHECK(a.mat_def_path == b.mat_def_path);
        CHECK(a.flags == b.flags);
        CHECK(a.f2_unk_x18 == b.f2_unk_x18);
        REQUIRE(a.textures.size() == b.textures.size());
        for (std::size_t t = 0; t < a.textures.size(); ++t)
        {
            CHECK(a.textures[t].path == b.textures[t].path);
            CHECK(a.textures[t].texture_type == b.textures[t].texture_type);
        }
        CHECK(a.gx_items.size() == b.gx_items.size());
    }

    // Compare two VertexArray instances: layout types, vertex count, and sampled decompressed data.
    void check_vertex_array_equal(const VertexArray& a, const VertexArray& b, bool approx_data = true)
    {
        CHECK(a.vertex_count == b.vertex_count);

        // Filter out Ignore types (added by layout repair) before comparing.
        auto filter_types = [](const VertexArrayLayout& layout) {
            std::vector<VertexDataType> result;
            for (const auto& t : layout.types)
                if (t.usage != VertexUsage::Ignore)
                    result.push_back(t);
            return result;
        };
        auto a_types = filter_types(a.layout);
        auto b_types = filter_types(b.layout);
        REQUIRE(a_types.size() == b_types.size());
        for (std::size_t t = 0; t < a_types.size(); ++t)
        {
            CHECK(a_types[t].usage == b_types[t].usage);
            CHECK(a_types[t].format == b_types[t].format);
            CHECK(a_types[t].instance_index == b_types[t].instance_index);
        }

        // Compare decompressed data sizes.
        CHECK(a.decompressed_data.size() == b.decompressed_data.size());

        if (approx_data && !a.decompressed_data.empty() && a.decompressed_data.size() == b.decompressed_data.size())
        {
            // Sample comparison: check first, middle, and last vertex data match within tolerance.
            // Vertex data is float-based after decompression, so lossy compression/decompression
            // means we check approximate equality of float values.
            auto stride = a.layout.decompressed_vertex_size;
            if (stride > 0 && a.vertex_count > 0)
            {
                std::vector<std::uint32_t> sample_vertices = {0};
                if (a.vertex_count > 2) sample_vertices.push_back(a.vertex_count / 2);
                if (a.vertex_count > 1) sample_vertices.push_back(a.vertex_count - 1);

                for (auto vi : sample_vertices)
                {
                    auto offset = static_cast<std::size_t>(vi) * stride;
                    // Compare as floats where possible.
                    auto float_count = stride / sizeof(float);
                    const auto* af = reinterpret_cast<const float*>(a.decompressed_data.data() + offset);
                    const auto* bf = reinterpret_cast<const float*>(b.decompressed_data.data() + offset);
                    for (std::size_t fi = 0; fi < float_count; ++fi)
                    {
                        // Lossy codecs (int→float→int→float) can lose precision.
                        // Use a generous tolerance.
                        // NaN == NaN is always false, so handle it explicitly.
                        if (std::isnan(af[fi]) && std::isnan(bf[fi])) continue;
                        CHECK(af[fi] == doctest::Approx(bf[fi]).epsilon(0.02));
                    }
                }
            }
        }
    }

    // Full FLVER structural comparison.
    void check_flver_equal(const FLVER& orig, const FLVER& reread)
    {
        CHECK(static_cast<std::uint32_t>(orig.version) == static_cast<std::uint32_t>(reread.version));
        CHECK(orig.big_endian == reread.big_endian);
        CHECK(orig.unicode == reread.unicode);

        CHECK(orig.bounding_box_min.x == doctest::Approx(reread.bounding_box_min.x));
        CHECK(orig.bounding_box_min.y == doctest::Approx(reread.bounding_box_min.y));
        CHECK(orig.bounding_box_min.z == doctest::Approx(reread.bounding_box_min.z));
        CHECK(orig.bounding_box_max.x == doctest::Approx(reread.bounding_box_max.x));
        CHECK(orig.bounding_box_max.y == doctest::Approx(reread.bounding_box_max.y));
        CHECK(orig.bounding_box_max.z == doctest::Approx(reread.bounding_box_max.z));

        check_bones_equal(orig.bones, reread.bones);
        check_dummies_equal(orig.dummies, reread.dummies);

        REQUIRE(orig.meshes.size() == reread.meshes.size());
        for (std::size_t mi = 0; mi < orig.meshes.size(); ++mi)
        {
            const auto& om = orig.meshes[mi];
            const auto& rm = reread.meshes[mi];

            CHECK(om.is_dynamic == rm.is_dynamic);
            CHECK(om.default_bone_index == rm.default_bone_index);

            check_material_equal(om.material, rm.material);

            // Face sets.
            REQUIRE(om.face_sets.size() == rm.face_sets.size());
            for (std::size_t fi = 0; fi < om.face_sets.size(); ++fi)
            {
                const auto& ofs = om.face_sets[fi];
                const auto& rfs = rm.face_sets[fi];
                CHECK(ofs.is_triangle_strip == rfs.is_triangle_strip);
                CHECK(ofs.use_backface_culling == rfs.use_backface_culling);
                CHECK(ofs.flags == rfs.flags);
                CHECK(ofs.vertex_indices.size() == rfs.vertex_indices.size());
                // Compare all vertex indices.
                if (ofs.vertex_indices.size() == rfs.vertex_indices.size())
                {
                    bool indices_match = true;
                    for (std::size_t ii = 0; ii < ofs.vertex_indices.size(); ++ii)
                    {
                        if (ofs.vertex_indices[ii] != rfs.vertex_indices[ii])
                        {
                            indices_match = false;
                            break;
                        }
                    }
                    CHECK(indices_match);
                }
            }

            // Vertex arrays.
            REQUIRE(om.vertex_arrays.size() == rm.vertex_arrays.size());
            for (std::size_t ai = 0; ai < om.vertex_arrays.size(); ++ai)
            {
                check_vertex_array_equal(om.vertex_arrays[ai], rm.vertex_arrays[ai]);
            }
        }
    }
} // namespace

// ---------------------------------------------------------------------------
// FLVER2 round-trip tests
// ---------------------------------------------------------------------------

TEST_CASE("FLVER2 round-trip: write and re-read")
{
    for (const auto* name : FLVER2_FIXTURES)
    {
        SUBCASE(name)
        {
            auto path = GetResourcePath(name);
            auto buf = LoadFile(path);
            if (buf.empty())
            {
                MESSAGE("Skipping " << name << " — fixture not found");
                return;
            }

            // 1. Read original.
            FLVER orig = FLVER::FromBytes(buf.data(), buf.size());

            // 2. Write to bytes.
            std::vector<std::byte> written = orig.ToBytes();
            REQUIRE(written.size() > 128); // at least a header

            // 3. Re-read from written bytes.
            FLVER reread = FLVER::FromBytes(written.data(), written.size());

            // 4. Compare.
            check_flver_equal(orig, reread);
        }
    }
}

TEST_CASE("FLVER2 round-trip: DCX compressed fixture")
{
    auto path = GetResourcePath("m0100B2A10.flver.dcx");
    auto raw = LoadFile(path);
    if (raw.empty())
    {
        MESSAGE("Skipping — fixture not found");
        return;
    }

    // Read (handles DCX internally).
    FLVER orig = FLVER::FromBytes(raw.data(), raw.size());

    // Write (produces uncompressed FLVER bytes).
    std::vector<std::byte> written = orig.ToBytes();
    REQUIRE(written.size() > 128);

    // Re-read the uncompressed FLVER.
    FLVER reread = FLVER::FromBytes(written.data(), written.size());

    check_flver_equal(orig, reread);
}

TEST_CASE("FLVER2 round-trip: double write produces identical bytes")
{
    // Write → re-read → write again. The two written byte buffers should be identical,
    // proving the writer is deterministic.
    for (const auto* name : FLVER2_FIXTURES)
    {
        SUBCASE(name)
        {
            auto path = GetResourcePath(name);
            auto buf = LoadFile(path);
            if (buf.empty())
            {
                MESSAGE("Skipping " << name << " — fixture not found");
                return;
            }

            FLVER orig = FLVER::FromBytes(buf.data(), buf.size());
            std::vector<std::byte> written1 = orig.ToBytes();

            // First write may normalize (e.g. strip Ignore layout types from repair).
            // Determinism is proven by write2 == write3.
            FLVER reread1 = FLVER::FromBytes(written1.data(), written1.size());
            std::vector<std::byte> written2 = reread1.ToBytes();

            FLVER reread2 = FLVER::FromBytes(written2.data(), written2.size());
            std::vector<std::byte> written3 = reread2.ToBytes();

            // The second and third written buffers should be byte-identical.
            CHECK(written2.size() == written3.size());
            if (written2.size() == written3.size())
            {
                CHECK(std::memcmp(written2.data(), written3.data(), written2.size()) == 0);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// FLVER0 synthetic round-trip test
// ---------------------------------------------------------------------------

TEST_CASE("FLVER0 round-trip: synthetic minimal FLVER")
{
    // Build a minimal FLVER0 in memory, write it, re-read it, and check equality.
    FLVER flver;
    flver.version = FLVERVersion::DemonsSouls;
    flver.big_endian = false;
    flver.unicode = false;
    flver.f0_unk_x4a = 1;
    flver.f0_unk_x4b = 0;
    flver.f0_unk_x4c = 65535;
    flver.f0_unk_x5c = 0;
    flver.bounding_box_min = {-1.f, -1.f, -1.f};
    flver.bounding_box_max = {1.f, 1.f, 1.f};

    // One bone.
    Bone bone;
    bone.name = "root";
    bone.translate = Vector3::Zero();
    bone.rotate = EulerRad::Zero();
    bone.scale = Vector3::One();
    flver.bones.push_back(bone);

    // One dummy.
    Dummy dummy;
    dummy.translate = {0.f, 1.f, 0.f};
    dummy.reference_id = 200;
    dummy.parent_bone_index = 0;
    flver.dummies.push_back(dummy);

    // One mesh with a simple triangle, using a minimal layout (position only).
    Mesh mesh;
    mesh.is_dynamic = false;
    mesh.default_bone_index = 0;
    mesh.bone_indices.resize(28, -1);
    mesh.bone_indices[0] = 0;

    // Material.
    mesh.material.name = "test_mat";
    mesh.material.mat_def_path = "test.mtd";
    Texture tex;
    tex.path = "test_diffuse.tga";
    tex.texture_type = std::nullopt; // FLVER0 may have no texture type
    mesh.material.textures.push_back(tex);

    // Layout: just position (Float3).
    VertexArrayLayout layout;
    VertexDataType pos_dt;
    pos_dt.usage = VertexUsage::Position;
    pos_dt.format = VertexDataFormatEnum::ThreeFloats;
    pos_dt.instance_index = 0;
    pos_dt.unk_x00 = 0;
    pos_dt.data_offset = 0;
    layout.types.push_back(pos_dt);
    layout.compressed_vertex_size = 12;
    layout.decompressed_vertex_size = 12; // position is 3 floats, no codec needed

    // Vertex array: 3 vertices (a triangle).
    VertexArray va;
    va.layout = layout;
    va.vertex_count = 3;
    va.decompressed_data.resize(3 * 12, std::byte{0});

    // Vertex 0: (0, 0, 0)
    float v0[] = {0.f, 0.f, 0.f};
    std::memcpy(va.decompressed_data.data() + 0, v0, 12);
    // Vertex 1: (1, 0, 0)
    float v1[] = {1.f, 0.f, 0.f};
    std::memcpy(va.decompressed_data.data() + 12, v1, 12);
    // Vertex 2: (0, 1, 0)
    float v2[] = {0.f, 1.f, 0.f};
    std::memcpy(va.decompressed_data.data() + 24, v2, 12);

    mesh.vertex_arrays.push_back(va);

    // Face set: one triangle (indices 0, 1, 2) as a strip.
    FaceSet fs;
    fs.flags = 0;
    fs.is_triangle_strip = true;
    fs.use_backface_culling = true;
    fs.unk_x06 = 0;
    fs.vertex_indices = {0, 1, 2};
    mesh.face_sets.push_back(fs);

    flver.meshes.push_back(mesh);

    // Write.
    std::vector<std::byte> written = flver.ToBytes();
    REQUIRE(written.size() > 128);

    // Re-read.
    FLVER reread = FLVER::FromBytes(written.data(), written.size());

    // Verify.
    CHECK(static_cast<std::uint32_t>(reread.version) == static_cast<std::uint32_t>(FLVERVersion::DemonsSouls));
    CHECK(reread.big_endian == false);
    CHECK(reread.unicode == false);
    CHECK(reread.bones.size() == 1);
    CHECK(reread.bones[0].name == "root");
    CHECK(reread.dummies.size() == 1);
    CHECK(reread.dummies[0].reference_id == 200);
    REQUIRE(reread.meshes.size() == 1);

    const auto& rm = reread.meshes[0];
    CHECK(rm.material.name == "test_mat");
    CHECK(rm.material.mat_def_path == "test.mtd");
    REQUIRE(rm.material.textures.size() == 1);
    CHECK(rm.material.textures[0].path == "test_diffuse.tga");

    REQUIRE(rm.face_sets.size() == 1);
    CHECK(rm.face_sets[0].vertex_indices.size() == 3);
    CHECK(rm.face_sets[0].vertex_indices[0] == 0);
    CHECK(rm.face_sets[0].vertex_indices[1] == 1);
    CHECK(rm.face_sets[0].vertex_indices[2] == 2);

    REQUIRE(rm.vertex_arrays.size() == 1);
    CHECK(rm.vertex_arrays[0].vertex_count == 3);

    // Verify vertex positions survived the round-trip.
    auto stride = rm.vertex_arrays[0].layout.decompressed_vertex_size;
    REQUIRE(stride == 12);
    const auto* data = reinterpret_cast<const float*>(rm.vertex_arrays[0].decompressed_data.data());
    CHECK(data[0] == doctest::Approx(0.f));  // v0.x
    CHECK(data[1] == doctest::Approx(0.f));  // v0.y
    CHECK(data[2] == doctest::Approx(0.f));  // v0.z
    CHECK(data[3] == doctest::Approx(1.f));  // v1.x
    CHECK(data[4] == doctest::Approx(0.f));  // v1.y
    CHECK(data[5] == doctest::Approx(0.f));  // v1.z
    CHECK(data[6] == doctest::Approx(0.f));  // v2.x
    CHECK(data[7] == doctest::Approx(1.f));  // v2.y
    CHECK(data[8] == doctest::Approx(0.f));  // v2.z

    // Double-write: second write should produce byte-identical output.
    std::vector<std::byte> written2 = reread.ToBytes();
    CHECK(written.size() == written2.size());
    if (written.size() == written2.size())
    {
        CHECK(std::memcmp(written.data(), written2.data(), written.size()) == 0);
    }
}

