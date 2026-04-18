// Test the FLVER2 reader against all FLVER fixtures.
//
// Known expected values for c2010.flver (Elden Ring character FLVER):
//   version: 0x2001A (Sekiro_EldenRing)
//   unicode: true
//   bones: 356
//   dummies: 118
//   meshes: 16
//   first mesh material name: "arm_armor" (UTF-16 LE raw bytes)
//   first mesh face_sets count: 6
//   first mesh vertex_arrays[0] vertex count: 6646

#include <doctest/doctest.h>

#include <FirelinkTestHelpers.h>
#include <FirelinkFLVER/FLVER.h>
#include <FirelinkFLVER/MergedMesh.h>

#include <filesystem>
#include <string>
#include <vector>

using namespace Firelink;

namespace
{
    // All fixture filenames to test.
    const char* FIXTURE_NAMES[] = {
        "eldenring/c2010.flver",
        "darksouls1r/m5020B2A10.flver",
        "darksouls1r/m5030B2A10.flver",
        "darksouls1r/m8101B2A10.flver",
    };
} // namespace

TEST_CASE("FLVER2 reader: header and basic structure")
{
    for (const auto* name : FIXTURE_NAMES)
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

            FLVER flver = FLVER::FromBytes(buf.data(), buf.size());

            // Generic checks that every FLVER fixture must satisfy.
            CHECK(flver.big_endian == false);
            CHECK(flver.meshes.size() > 0);
            CHECK(flver.bones.size() > 0);

            // c2010.flver — known exact values.
            if (std::string(name) == "c2010.flver")
            {
                CHECK(static_cast<std::uint32_t>(flver.version) == 0x2001A);
                CHECK(flver.unicode == true);
                CHECK(flver.bones.size() == 356);
                CHECK(flver.dummies.size() == 118);
                CHECK(flver.meshes.size() == 16);

                const auto& m0 = flver.meshes[0];
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
    for (const auto* name : FIXTURE_NAMES)
    {
        SUBCASE(name)
        {
            auto path = GetResourcePath(name);
            auto buf = LoadFile(path);
            if (buf.empty()) return;

            FLVER flver = FLVER::FromBytes(buf.data(), buf.size());

            // At least the first bone should have a non-empty name (UTF-16 LE raw bytes).
            REQUIRE(!flver.bones.empty());
            CHECK(!flver.bones[0].name.empty());
        }
    }
}

TEST_CASE("FLVER2 reader: materials have textures")
{
    for (const auto* name : FIXTURE_NAMES)
    {
        SUBCASE(name)
        {
            auto path = GetResourcePath(name);
            auto buf = LoadFile(path);
            if (buf.empty()) return;

            FLVER flver = FLVER::FromBytes(buf.data(), buf.size());

            // Every mesh should have a material with at least one texture.
            for (const auto& mesh : flver.meshes)
            {
                CHECK(!mesh.material.textures.empty());
            }
        }
    }
}

TEST_CASE("MergedMesh: builds successfully for all fixtures")
{
    for (const auto* name : FIXTURE_NAMES)
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

            FLVER flver = FLVER::FromBytes(buf.data(), buf.size());
            MergedMesh mm = MergedMesh::from_flver(flver);

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
    auto path = GetResourcePath("m0100B2A10.flver.dcx");
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
    FLVER flver = FLVER::FromBytes(result.data.data(), result.data.size());

    // Basic sanity checks on the parsed FLVER.
    CHECK(flver.meshes.size() > 0);
    CHECK(flver.bones.size() > 0);
    CHECK(flver.big_endian == false);
}
