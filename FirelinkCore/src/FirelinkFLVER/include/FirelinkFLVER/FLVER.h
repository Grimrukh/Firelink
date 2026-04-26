// flver/flver.hpp
//
// Top-level FLVER container class (C++ port of src/soulstruct/flver/core.py).
//
// The C++ API mirrors the Python API where practical. The Python port uses
// denormalised ownership at the class level — materials, face sets, vertex
// arrays, and layouts are all stored *inside* their consuming Mesh
// rather than as parallel root-level lists — so we follow suit here. Writing
// is responsible for re-deduplicating shared pieces (materials, layouts,
// GXItem lists) back into global tables, mirroring the Python writer.
//
// Differences from the Python surface that are worth knowing:
//
//   * Vertex data is exposed as a single flat interleaved byte buffer
//     whose stride is given by the layout's decompressed size. Accessors
//     compute per-field views on top of that buffer. This is the same layout
//     numpy structured arrays use; it just avoids the numpy dependency. A
//     per-vertex Vertex class is explicitly avoided (too slow for ER).
//   * Bone tree navigation is via indices into FLVER::bones. The Python
//     port also stores back-references to the bones themselves for
//     convenience, which is awkward under value semantics in C++.
//   * Strings are stored as std::string holding raw on-disk bytes. The
//     FLVER's `encoding` field indicates how to interpret them. Decoding to
//     UTF-8 is a separate concern handled at the Python boundary.

#pragma once

#include <FirelinkFLVER/Bone.h>
#include <FirelinkFLVER/Dummy.h>
#include <FirelinkFLVER/Export.h>
#include <FirelinkFLVER/MergedMesh.h>
#include <FirelinkFLVER/Mesh.h>
#include <FirelinkFLVER/Version.h>

#include <FirelinkCore/Collections.h>
#include <FirelinkCore/Endian.h>
#include <FirelinkCore/GameFile.h>

#include <cstdint>
#include <vector>

namespace Firelink
{
    // Encoding of on-disk strings in a FLVER file. Chosen by the `unicode` flag
    // in the header (true -> UTF-16 LE / BE depending on endian, false -> Shift-JIS 2004).
    enum class StringEncoding : std::uint8_t
    {
        ShiftJIS2004 = 0,
        UTF16LE = 1,
        UTF16BE = 2,
    };

    class FIRELINK_FLVER_API FLVER : public GameFile<FLVER>
    {
    public:
        FLVER() = default;

        // --- header-level metadata --------------------------------------------

        FLVERVersion version = FLVERVersion::Null;
        bool big_endian = false;
        bool unicode = true;
        Vector3 bounding_box_min = Vector3::SingleMax();
        Vector3 bounding_box_max = Vector3::SingleMin();
        std::int32_t true_face_count = 0;
        std::int32_t total_face_count = 0;

        // Preserved-but-unknown header fields (round-trip fidelity).
        bool f2_unk_x4a = false;
        std::int32_t f2_unk_x4c = 1;
        std::int32_t f2_unk_x5c = 0;
        std::int32_t f2_unk_x5d = 65535;
        std::int32_t f2_unk_x68 = 0;

        // FLVER0-specific preserved fields.
        std::uint8_t f0_unk_x4a = 0;
        std::uint8_t f0_unk_x4b = 0;
        std::uint32_t f0_unk_x4c = 0;
        std::uint8_t f0_unk_x5c = 0;

        // --- collections (only these three; everything else is per-mesh) -----

        std::vector<Bone> bones;
        std::vector<Dummy> dummies;
        std::vector<Mesh> meshes;

        /// @brief Get endianness of this FLVER.
        [[nodiscard]] BinaryReadWrite::Endian GetEndian() const noexcept;

        /// @brief Deserialize FLVER (GameFile).
        void Deserialize(BinaryReadWrite::BufferReader& r);

        /// @brief Serialize FLVER (GameFile).
        void Serialize(BinaryReadWrite::BufferWriter& w) const;

        /// @brief Create and cache a Merged Mesh suitable for external model import.
        MergedMesh GetMergedMesh(
            const std::vector<std::uint32_t>& meshMaterialIndices = {},
            const std::vector<std::vector<std::string>>& materialUVLayerNames = {},
            bool merge_vertices = true);

        /// @brief Clear the cached Merged Mesh for re-computation.
        void ClearCachedMergedMesh();

    private:

        /// @brief Cached MergedMesh for this FLVER.
        std::unique_ptr<MergedMesh> m_cachedMergedMesh;

        /// @brief Dispatched serialization for FLVER0.
        void SerializeFLVER0(BinaryReadWrite::BufferWriter& w) const;

        /// @brief Dispatched serialization for FLVER2.
        void SerializeFLVER2(BinaryReadWrite::BufferWriter& w) const;
    };
} // namespace Firelink
