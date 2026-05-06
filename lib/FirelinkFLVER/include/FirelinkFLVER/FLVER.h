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
    // Encoding of on-disk strings in a FLVER file. Chosen by the `isUnicode` property
    // in the header (true -> UTF-16 LE / BE depending on endian, false -> Shift-JIS 2004).
    enum class FLVERStringEncoding : std::uint8_t
    {
        ShiftJIS2004 = 0,
        UTF16LE = 1,
        UTF16BE = 2,
    };


    class FIRELINK_FLVER_API FLVER : public GameFile<FLVER>
    {
    public:
        FLVER() = default;

        // --- GAME FILE OVERRIDES ---

        /// @brief Get endianness of this FLVER.
        [[nodiscard]] BinaryReadWrite::Endian GetEndian() const noexcept;

        /// @brief Deserialize FLVER (GameFile).
        void Deserialize(BinaryReadWrite::BufferReader& r);

        /// @brief Serialize FLVER (GameFile).
        void Serialize(BinaryReadWrite::BufferWriter& w) const;

        // --- PUBLIC METHODS ---

        /// @brief Get the bone at the given index.
        [[nodiscard]] const Bone& GetBone(const std::size_t index) const { return m_bones[index]; }
        [[nodiscard]] Bone& GetBone(const std::size_t index) { return m_bones[index]; }

        /// @brief Get the dummy at the given index.
        [[nodiscard]] const Dummy& GetDummy(const std::size_t index) const { return m_dummies[index]; }
        [[nodiscard]] Dummy& GetDummy(const std::size_t index) { return m_dummies[index]; }

        /// @brief Get the mesh at the given index.
        [[nodiscard]] const Mesh& GetMesh(const std::size_t index) const { return m_meshes[index]; }
        [[nodiscard]] Mesh& GetMesh(const std::size_t index) { return m_meshes[index]; }

        /// @brief Create and cache a Merged Mesh suitable for external model import.
        MergedMesh GetCachedMergedMesh(
            const std::vector<std::uint32_t>& meshMaterialIndices = {},
            const std::vector<std::vector<std::string>>& materialUVLayerNames = {},
            bool merge_vertices = true);

        /// @brief Clear the cached Merged Mesh for re-computation.
        void ClearCachedMergedMesh();

        // --- PROPERTIES ---

        // Header-level metadata.
        GAME_FILE_PROPERTY(FLVERVersion, m_version, Version, FLVERVersion::Null)
        GAME_FILE_PROPERTY(bool, m_isBigEndian, IsBigEndian, false)
        GAME_FILE_PROPERTY(bool, m_isUnicode, IsUnicode, true)
        GAME_FILE_PROPERTY_REF(AABB, m_boundingBox, BoundingBox, AABB::Invalid())
        GAME_FILE_PROPERTY(std::int32_t, m_trueFaceCount, TrueFaceCount, 0)
        GAME_FILE_PROPERTY(std::int32_t, m_totalFaceCount, TotalFaceCount, 0)

        // FLVER2-specific preserved fields. Ignored by serializer for FLVER0 versions.
        GAME_FILE_PROPERTY(bool, m_f2Unk4a, F2Unk4a, false)
        GAME_FILE_PROPERTY(std::int32_t, m_f2Unk4c, F2Unk4c, 1)
        GAME_FILE_PROPERTY(std::int32_t, m_f2Unk5c, F2Unk5c, 0)
        GAME_FILE_PROPERTY(std::int32_t, m_f2Unk5d, F2Unk5d, 65535)
        GAME_FILE_PROPERTY(std::int32_t, m_f2Unk68, F2Unk68, 0)

        // FLVER0-specific preserved fields. Ignored by serializer for FLVER2 versions.
        GAME_FILE_PROPERTY(std::uint8_t, m_f0Unk4a, F0Unk4a, 0)
        GAME_FILE_PROPERTY(std::uint8_t, m_f0Unk4b, F0Unk4b, 0)
        GAME_FILE_PROPERTY(std::uint32_t, m_f0Unk4c, F0Unk4c, 0)
        GAME_FILE_PROPERTY(std::uint8_t, m_f0Unk5c, F0Unk5c, 0)

        // Collections. Everything else is denormalized per-mesh and unified by equality on serialization.
        GAME_FILE_PROPERTY_REF(std::vector<Bone>, m_bones, Bones, );
        GAME_FILE_PROPERTY_REF(std::vector<Dummy>, m_dummies, Dummies, );
        GAME_FILE_PROPERTY_REF(std::vector<Mesh>, m_meshes, Meshes, );

    private:

        /// @brief Cached MergedMesh for this FLVER.
        /// @note Nothing intelligent about caching; user must clear it manually if they have modified
        /// FLVER properties that would affect the merged mesh.
        std::unique_ptr<MergedMesh> m_cachedMergedMesh;

        /// @brief Dispatched deserialization for FLVER0.
        void DeserializeFLVER0(BinaryReadWrite::BufferReader& r);

        /// @brief Dispatched deserialization for FLVER2.
        void DeserializeFLVER2(BinaryReadWrite::BufferReader& r);

        /// @brief Dispatched serialization for FLVER0.
        void SerializeFLVER0(BinaryReadWrite::BufferWriter& w) const;

        /// @brief Dispatched serialization for FLVER2.
        void SerializeFLVER2(BinaryReadWrite::BufferWriter& w) const;
    };
} // namespace Firelink
