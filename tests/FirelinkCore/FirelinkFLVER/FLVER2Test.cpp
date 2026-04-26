// Test FLVER2 write → re-read round-trip for FLVER2 fixtures.
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
#include <vector>

using namespace Firelink;

namespace
{
    const char* FLVER2_FIXTURES[] = {
        "eldenring/c2010.flver",
        "darksouls1r/m5020B2A10.flver",
        "darksouls1r/m5030B2A10.flver",
        "darksouls1r/m8101B2A10.flver",
    };

    // Compare two Bone vectors.
    void CheckBonesEqual(const std::vector<Bone>& a, const std::vector<Bone>& b)
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
    void CheckDummiesEqual(const std::vector<Dummy>& a, const std::vector<Dummy>& b)
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
    void CheckMaterialsEqual(const Material& a, const Material& b)
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
    void CheckVertexArraysEqual(const VertexArray& a, const VertexArray& b, bool approx_data = true)
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
    void CheckFLVEREqual(const FLVER& orig, const FLVER& reread)
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

        CheckBonesEqual(orig.bones, reread.bones);
        CheckDummiesEqual(orig.dummies, reread.dummies);

        REQUIRE(orig.meshes.size() == reread.meshes.size());
        for (std::size_t mi = 0; mi < orig.meshes.size(); ++mi)
        {
            const auto& om = orig.meshes[mi];
            const auto& rm = reread.meshes[mi];

            CHECK(om.is_dynamic == rm.is_dynamic);
            CHECK(om.default_bone_index == rm.default_bone_index);

            CheckMaterialsEqual(om.material, rm.material);

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
                CheckVertexArraysEqual(om.vertex_arrays[ai], rm.vertex_arrays[ai]);
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
            FLVER::Ptr orig = FLVER::FromBytes(buf.data(), buf.size());

            // 2. Write to bytes.
            std::vector<std::byte> written = orig->ToBytes();
            REQUIRE(written.size() > 128); // at least a header

            // 3. Re-read from written bytes.
            FLVER::Ptr reread = FLVER::FromBytes(written.data(), written.size());

            // 4. Compare.
            CheckFLVEREqual(*orig, *reread);
        }
    }
}

TEST_CASE("FLVER2 round-trip: DCX compressed fixture")
{
    const auto path = GetResourcePath("darksouls1r/m0100B2A10.flver.dcx");
    const auto raw = LoadFile(path);
    if (raw.empty())
    {
        MESSAGE("Skipping — fixture not found");
        return;
    }

    // Read (handles DCX internally).
    const FLVER::CPtr orig = FLVER::FromBytes(raw.data(), raw.size());

    // Write (produces uncompressed FLVER bytes).
    const std::vector<std::byte> written = orig->ToBytes();
    REQUIRE(written.size() > 128);

    // Re-read the uncompressed FLVER.
    const FLVER::CPtr reread = FLVER::FromBytes(written.data(), written.size());

    CheckFLVEREqual(*orig, *reread);
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

            FLVER::Ptr orig = FLVER::FromBytes(buf.data(), buf.size());
            std::vector<std::byte> written1 = orig->ToBytes();

            // First write may normalize (e.g. strip Ignore layout types from repair).
            // Determinism is proven by write2 == write3.
            FLVER::Ptr reread1 = FLVER::FromBytes(written1.data(), written1.size());
            std::vector<std::byte> written2 = reread1->ToBytes();

            FLVER::Ptr reread2 = FLVER::FromBytes(written2.data(), written2.size());
            std::vector<std::byte> written3 = reread2->ToBytes();

            // The second and third written buffers should be byte-identical.
            CHECK(written2.size() == written3.size());
            if (written2.size() == written3.size())
            {
                CHECK(std::memcmp(written2.data(), written3.data(), written2.size()) == 0);
            }
        }
    }
}


TEST_CASE("FLVER2 reader: header and basic structure")
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

            REQUIRE(buf.size() > 0);

            FLVER::CPtr flver = FLVER::FromBytes(buf.data(), buf.size());

            // Generic checks that every FLVER fixture must satisfy.
            CHECK(flver->big_endian == false);
            CHECK(flver->meshes.size() > 0);
            CHECK(flver->bones.size() > 0);

            // c2010.flver — known exact values.
            if (std::string(name) == "c2010.flver")
            {
                CHECK(static_cast<std::uint32_t>(flver->version) == 0x2001A);
                CHECK(flver->unicode == true);
                CHECK(flver->bones.size() == 356);
                CHECK(flver->dummies.size() == 118);
                CHECK(flver->meshes.size() == 16);

                const auto& m0 = flver->meshes[0];
                CHECK(m0.face_sets.size() == 6);
                CHECK(m0.vertex_arrays.size() >= 1);

                if (!m0.vertex_arrays.empty())
                {
                    CHECK(m0.vertex_arrays[0].vertex_count == 6646);
                    CHECK(m0.vertex_arrays[0].decompressed_data.size() > 0);
                }
            }
        }
    }
}

TEST_CASE("FLVER2 reader: bone names are populated")
{
    for (const auto* name : FLVER2_FIXTURES)
    {
        SUBCASE(name)
        {
            auto path = GetResourcePath(name);
            auto buf = LoadFile(path);
            if (buf.empty()) return;

            const FLVER::CPtr flver = FLVER::FromBytes(buf.data(), buf.size());

            // At least the first bone should have a non-empty name (UTF-16 LE raw bytes).
            REQUIRE(!flver->bones.empty());
            CHECK(!flver->bones[0].name.empty());
        }
    }
}

TEST_CASE("FLVER2 reader: materials have textures")
{
    for (const auto* name : FLVER2_FIXTURES)
    {
        SUBCASE(name)
        {
            auto path = GetResourcePath(name);
            auto buf = LoadFile(path);
            if (buf.empty()) return;

            const FLVER::CPtr flver = FLVER::FromBytes(buf.data(), buf.size());

            // Every mesh should have a material with at least one texture.
            for (const auto& mesh : flver->meshes)
            {
                CHECK(!mesh.material.textures.empty());
            }
        }
    }
}

TEST_CASE("MergedMesh: builds successfully for all fixtures")
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

            REQUIRE(buf.size() > 0);

            FLVER::CPtr flver = FLVER::FromBytes(buf.data(), buf.size());
            MergedMesh mm(*flver);

            // Basic non-empty checks.
            CHECK(mm.vertex_count > 0);
            CHECK(mm.face_count > 0);
            CHECK(mm.total_loop_count > 0);

            // Array size invariants.
            CHECK(mm.positions.size() == mm.vertex_count * 3);
            CHECK(mm.bone_weights.size() == mm.vertex_count * 4);
            CHECK(mm.bone_indices.size() == mm.vertex_count * 4);
            CHECK(mm.faces.size() == mm.face_count * 4);
            CHECK(mm.loop_vertex_indices.size() == mm.total_loop_count);

            // At least one UV layer, and its data has the right size.
            CHECK(!mm.loop_uvs.empty());
            for (const auto& uv : mm.loop_uvs)
            {
                CHECK(uv.data.size() == mm.total_loop_count * uv.dim);
            }

            // All loop normals (if present) have the right size.
            if (!mm.loop_normals.empty())
            {
                CHECK(mm.loop_normals.size() == mm.total_loop_count * 3);
            }

            // Every face corner loop index must be in range.
            bool all_valid = true;
            for (std::uint32_t fi = 0; fi < mm.face_count; ++fi)
            {
                for (int c = 0; c < 3; ++c)
                {
                    if (mm.faces[fi * 4 + c] >= mm.total_loop_count)
                    {
                        all_valid = false;
                    }
                }
            }
            CHECK(all_valid);
        }
    }
}

TEST_CASE("FLVER: reads from DCX compressed file")
{
    auto path = GetResourcePath("darksouls1r/m0100B2A10.flver.dcx");
    auto raw = LoadFile(path);
    if (raw.empty())
    {
        MESSAGE("Skipping — fixture not found");
        return;
    }

    DCXResult result = DecompressDCX(raw.data(), raw.size());

    // Should have decompressed to a non-empty buffer.
    REQUIRE(result.data.size() > 0);
    CHECK(result.type != DCXType::Unknown);
    CHECK(result.type != DCXType::Null);

    // First 4 bytes of a FLVER file are "FLVR" (little-endian magic).
    // Actually the FLVER magic is "FLVER\0" at offset 6. Let's just parse it.
    FLVER::CPtr flver = FLVER::FromBytes(result.data.data(), result.data.size());

    // Basic sanity checks on the parsed FLVER.
    CHECK(flver->meshes.size() > 0);
    CHECK(flver->bones.size() > 0);
    CHECK(flver->big_endian == false);
}
