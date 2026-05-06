// flver/layout_repair.hpp
//
// C++ port of soulstruct.flver.python.layout_repair.
//
// QLOC botched some vertex layouts in DS1R: the layout stored in the FLVER
// says one thing, but the actual on-disk vertex data has extra fields (often
// tangents/bitangents that the MTD doesn't reference). We detect the mismatch
// by comparing the layout's total compressed size with the vertex array
// header's vertex_size. When they disagree, we look up the material's MTD
// name in a table of known-corrected layouts and substitute the repaired one.
//
// This must be called BEFORE vertex arrays are decompressed.

#pragma once

#include <FirelinkFLVER/Material.h>
#include <FirelinkFLVER/Mesh.h>

#include <cstdint>
#include <vector>

namespace Firelink
{
    // Forward declaration of the vertex array header used during reading.
    // (Matches the anonymous-namespace struct in flver.cpp; we redeclare
    // the minimal interface here so layout_repair.cpp can use it.)
    struct VertexArrayHeaderView
    {
        std::uint32_t layout_index;
        std::uint32_t vertex_size;
    };

    // Attempt to repair known-broken DS1R vertex array layouts in-place.
    //
    // For each mesh, if the layout's compressed size doesn't match the vertex
    // array header's vertex_size, we try to guess the correct layout from the
    // mesh's MTD name. If a match is found, a new layout is appended to
    // `layouts` and `va_header.layout_index` is updated to point to it.
    //
    // 99% of fixes are just adding tangents where they were omitted, or removing
    // them where they were incorrectly included in the layout.
    //
    // `mesh_va_indices` is parallel to `meshes` — each entry lists the global
    // vertex-array indices the mesh references.
    //
    // Returns the number of layouts that were successfully repaired.
    int CheckDS1Layouts(
        std::vector<VertexArrayLayout>& layouts,
        std::vector<VertexArrayHeaderView>& va_headers,
        const std::vector<Material>& materials,
        const std::vector<std::vector<std::uint32_t>>& mesh_va_indices
    );
} // namespace Firelink
