// flver/merged_mesh.hpp
//
// C++ port of soulstruct.flver.python.mesh_tools.MergedMesh.
//
// Merges all FLVER meshes into a single combined mesh suitable for Blender
// import. Unique vertices are identified by (position, bone_indices,
// bone_weights), and per-face-vertex "loop" data (normals, tangents, UVs,
// colors) is stored separately so that face corners can reference shared
// vertices while retaining per-corner shading data.

#pragma once

#include <FirelinkFLVER/Export.h>

#include <cstdint>
#include <string>
#include <vector>

namespace Firelink
{
    // Forward declarations
    class FLVER;

    // Holds all merged mesh output arrays. All arrays are flat and contiguous
    // for direct handoff to Python/numpy via the C API (zero-copy possible).
    struct FIRELINK_FLVER_API MergedMesh
    {
        // --- Per-vertex data (unique vertices) --------------------------------

        // Flat (vertex_count * 3) float array: x y z x y z ...
        std::vector<float> positions;
        // Flat (vertex_count * 4) float array: w0 w1 w2 w3 w0 w1 w2 w3 ...
        std::vector<float> bone_weights;
        // Flat (vertex_count * 4) int32 array: b0 b1 b2 b3 b0 b1 b2 b3 ...
        std::vector<std::int32_t> bone_indices;

        std::uint32_t vertex_count = 0;

        // --- Per-loop (per-face-vertex) data ----------------------------------

        // Index into vertex arrays for each loop. Length = total_loop_count.
        std::vector<std::uint32_t> loop_vertex_indices;

        // Normals: (total_loop_count * 3) floats, or empty if absent.
        std::vector<float> loop_normals;
        // Normal W: (total_loop_count) uint8, or empty.
        std::vector<std::uint8_t> loop_normals_w;

        // Tangents: outer index = tangent slot. Each is (total_loop_count * 4) floats.
        std::vector<std::vector<float>> loop_tangents;
        // Bitangent: (total_loop_count * 4) floats, or empty.
        std::vector<float> loop_bitangents;

        // Vertex colors: outer = color slot. Each is (total_loop_count * 4) floats.
        std::vector<std::vector<float>> loop_vertex_colors;

        // UVs: ordered list of (name, data). Each data is (total_loop_count * dim) floats.
        // dim is usually 2 but can be up to 4.
        struct UVLayer
        {
            std::string name; // e.g. "UVMap0", "UVMap1"
            std::uint32_t dim = 2; // columns per UV
            std::vector<float> data;
        };

        std::vector<UVLayer> loop_uvs;

        std::uint32_t total_loop_count = 0;

        // --- Faces ------------------------------------------------------------

        // Flat (face_count * 4) uint32 array: loop0 loop1 loop2 material_index
        std::vector<std::uint32_t> faces;
        std::uint32_t face_count = 0;

        // Whether vertices were merged (vs simply stacked).
        bool vertices_merged = false;

        // --- Construction -----------------------------------------------------

        // Build a MergedMesh from a parsed FLVER. This is the main entry point.
        // `mesh_material_indices`: per-mesh material index for the merged faces[:, 3].
        //   If empty, each mesh gets its own index (0, 1, 2, ...).
        // `material_uv_layer_names`: per-material list of UV layer names. If empty,
        //   defaults to "UVMap0", "UVMap1", etc.
        // `merge_vertices`: if true, deduplicate vertices by position+bone data.
        static MergedMesh from_flver(
            const FLVER& flver,
            const std::vector<std::uint32_t>& mesh_material_indices = {},
            const std::vector<std::vector<std::string>>& material_uv_layer_names = {},
            bool merge_vertices = true
        );
    };
} // namespace Firelink
