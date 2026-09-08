// Merges all FLVER meshes into a single combined mesh suitable for Blender
// import. Unique vertices are identified by (position, bone_indices,
// bone_weights), and per-face-vertex "loop" data (normals, tangents, UVs,
// colors) is stored separately so that face corners can reference shared
// vertices while retaining per-corner shading data.
//
// Also handles splitting of a MergedMesh into multiple FLVER submeshes.
// Each submesh is defined by a `SplitMeshDef`; additional submeshes may
// be created if the maximum bone count for a submesh is reached.

#pragma once

#include <FirelinkFLVER/Export.h>
#include <FirelinkFLVER/Material.h>
#include <FirelinkFLVER/Mesh.h>

#include <cstdint>
#include <string>
#include <vector>

namespace Firelink
{
    // Forward declarations (FLVER stores a cached MergedMesh).
    class FLVER;

    // One output FLVER submesh definition, supplied per distinct value of
    // `MergedMesh::faces[:, 3]`. The C++ analogue of Python's `SplitMeshDef`
    // NamedTuple (the Python `kwargs` dict is unpacked into explicit fields).
    struct FIRELINK_FLVER_API SplitMeshDef
    {
        Material material;
        VertexArrayLayout layout; // target (decompressed) layout for this submesh
        bool is_dynamic = false; // skinned (always `bone_indices`) vs rigid (`normal_w` bone in newer games)
        bool use_backface_culling = true;
        std::int32_t default_bone_index = 0;
        bool uses_bounding_boxes = true;
        int face_set_count = 1; // 1..3; >1 duplicates the base face set as LOD copies

        // Global UV layer name (a key in `MergedMesh::loop_uvs`) feeding each
        // local `uv_<i>` field of `layout`, indexed by local UV slot. If empty,
        // each local slot defaults to "UVMap<i>". Length should match the
        // layout's UV field count; missing entries fall back to the default.
        std::vector<std::string> uv_layer_names;
    };

    /*! @struct SplitMeshParams
     *  @brief Settings for `MergedMesh.SplitMesh()` method.
     *
     *  @details These parameters control how the merged mesh is split into submeshes,
     *  particularly how vertex bone indices are handled and how vertex deduplication
     *  is performed based on normal/tangent similarity.
     *
     *  @note Defaults are geared towards Dark Souls 1 (PTDE/DSR).
     *///
    struct FIRELINK_FLVER_API SplitMeshParams
    {
        // Whether vertex bone indices index into local mesh bone indices array
        // (true, older games) or into the global FLVER bones (false, newer games).
        bool useMeshBoneIndices = true;
        // Maximum number of bones per FLVER mesh. Additional meshes are created
        // automatically (in addition to the explicit SplitMeshDefs) as required if
        // this capacity is reached.
        int maxBonesPerMesh = 38;
        // In dynamic FLVER meshes, unused bone indices (corresponding to zero weight)
        // are set at zero, which cannot be distinguished from genuine bone index 0
        // without checking for the zero weight. The caller may instead mark unused
        // bone indices with -1, which makes the bone indices easier to parse when
        // splitting; they will still be changed to 0 in the real FLVER vertex data.
        bool unusedBoneIndicesAreMinusOne = false;
        // Threshold (minimum dot product) for merging FLVER vertices based on how
        // close their normals and tangents are. If 1.0 (default), these vectors must
        // match exactly, i.e. the dot product must be 1. If 0.5, the vectors may
        // differ up to 90 degrees, etc., leading to more aggressive merging and
        // smaller FLVERs (but shading over sharp edges may appear smoother). Note
        // that vertices must always have exactly identical positions to be merged.
        float normalTangentDotThreshold = 1.0f;
        // Maximum number of vertices per mesh, typically constrained by whether
        // face vertex indices are 16-bit (older games, max 65535) or 32-bit. Note
        // that when reading vanilla FLVERs with strip-type face indices, the true
        // maximum is 65534, because index 65535 (-1) marks a strip winding order
        // reversal -- but we never write strip-type FLVERs, so this marker is never
        // needed on FLVER write.
        std::uint32_t maxVerticesPerMesh = 0;
        // Whether this is an old `FLVER0` (e.g., Demon's Souls), which affects a
        // few details of Mesh cleanup such as skipping bounding box computation.
        bool isFlver0 = false;
    };

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

        // Default-constructs an empty MergedMesh, e.g. for callers (such as Python
        // bindings) that want to populate the arrays manually before calling
        // `SplitMesh()`, rather than building from a parsed FLVER.
        MergedMesh() = default;

        /*! @brief Build a MergedMesh from a parsed FLVER.
         *
         *  @details This is the main entry point.
         *  @param flver: FLVER whose submeshes will be merged.
         *  @param meshMaterialIndices  Per-mesh material index for the merged faces[:, 3].
         *    If empty, each mesh gets its own index (0, 1, 2, ...).
         *  @param materialUVLayerNames  Per-material list of UV layer names. If empty,
         *    defaults to "UVMap0", "UVMap1", etc.
         *  @param mergeVertices  If true, deduplicate vertices that have identical position
         *    and bone data (i.e., face-corner 'loops' differing only in color/normal/tangent/UV).
         *///
        explicit MergedMesh(
            const FLVER& flver,
            const std::vector<std::uint32_t>& meshMaterialIndices = {},
            const std::vector<std::vector<std::string>>& materialUVLayerNames = {},
            bool mergeVertices = true);

        /*! @brief Split this merged mesh into FLVER submeshes, one per entry of
         *  `splitMeshDefs` (and possibly several per entry, when bone-count
         *  sub-splitting is required).
         *
         *  @param splitMeshDefs  Definitions of split FLVER submeshes.
         *  @param params         Settings for mesh-splitting. See ``SplitMeshParams``.
         *///
        [[nodiscard]] std::vector<Mesh> SplitMesh(
            const std::vector<SplitMeshDef>& splitMeshDefs,
            const SplitMeshParams& params) const;
    };
} // namespace Firelink
