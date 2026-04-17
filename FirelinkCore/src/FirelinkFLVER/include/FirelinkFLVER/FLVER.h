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
#include <FirelinkFLVER/Mesh.h>
#include <FirelinkFLVER/Version.h>

#include <FirelinkCore/Collections.h>
#include <FirelinkCore/DCX.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
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

    class FIRELINK_FLVER_API FLVER
    {
    public:
        FLVER() = default;

        // Read a FLVER from an already-decompressed byte buffer. Dispatches on
        // the detected version (FLVER0 vs FLVER2) inside. Phase 2 implements
        // FLVER2; Phase 4 adds FLVER0.
        static FLVER FromBytes(const std::byte* data, std::size_t size);

        // Read a FLVER from a file path. The file is read into memory and
        // parsed via `FromBytes`. Note that this does NOT handle DCX
        // decompression — callers that need DCX support should decompress
        // first (the Python bindings do this automatically).
        static FLVER FromPath(const std::filesystem::path& path);

        // Serialise to a byte buffer in the format matching `version`. Implemented
        // in Phase 3 (FLVER2) and Phase 4 (FLVER0).
        [[nodiscard]] std::vector<std::byte> ToBytes() const;

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

        // --- non-serialized properties set by caller -------------------------

        std::filesystem::path path;
        DCXType dcx_type = DCXType::Null;

    private:
        [[nodiscard]] std::vector<std::byte> ToBytesFLVER0() const;
        [[nodiscard]] std::vector<std::byte> ToBytesFLVER2() const;
    };
} // namespace Firelink
