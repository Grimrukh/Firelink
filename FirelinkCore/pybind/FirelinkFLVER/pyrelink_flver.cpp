// flver_pybind.cpp — pybind11 bindings for FirelinkFLVER.
//
// Exposes the C++ FLVER reader and MergedMesh to Python with zero-copy
// numpy arrays where possible.
//
// String handling: C++ FLVER stores raw on-disk bytes in std::string fields.
// After parsing, we decode all strings IN-PLACE to UTF-8 (via Python codecs
// for Shift-JIS and the CPython API for UTF-16). This means `bone.name`,
// `material.name`, `texture.path`, etc. are proper decoded strings directly
// on the C++ structs, without any wrapper indirection.

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>

#include <FirelinkFLVER/FLVER.h>
#include <FirelinkFLVER/MergedMesh.h>

#include <FirelinkCore/BinaryReadWrite.h>
#include <FirelinkCore/DCX.h>

#include <cstring>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

// Amalgamated source
#include "pyrelink_texture_finder.cpp"

namespace py = pybind11;
using namespace Firelink;

// ============================================================================
// String decoding helper
// ============================================================================
// Converts a raw on-disk byte string to a UTF-8 std::string using Python
// codecs (Shift-JIS) or the CPython API (UTF-16).

static std::string decode_flver_string(const std::string& raw, StringEncoding enc)
{
    if (raw.empty()) return {};

    if (enc == StringEncoding::UTF16LE || enc == StringEncoding::UTF16BE)
    {
        // Use CPython's UTF-16 decoder.  The C++ reader already handles
        // endian-swapping, so the bytes are native-endian (LE on PC).
        int byte_order = (enc == StringEncoding::UTF16LE) ? -1 : 1;
        const auto decoded = py::reinterpret_steal<py::str>(
            PyUnicode_DecodeUTF16(
                raw.data(), static_cast<Py_ssize_t>(raw.size()),
                nullptr, &byte_order)
        );
        if (!decoded)
        {
            PyErr_Clear();
            return raw; // fallback: return raw bytes as-is
        }
        auto result = decoded.cast<std::string>();
        while (!result.empty() && result.back() == '\0') result.pop_back();
        return result;
    }

    // Shift-JIS: delegate to Python's codec.
    const py::bytes raw_bytes(raw.data(), raw.size());
    try
    {
        const py::str decoded = raw_bytes.attr("decode")("shift-jis");
        auto result = decoded.cast<std::string>();
        while (!result.empty() && result.back() == '\0') result.pop_back();
        return result;
    }
    catch (...)
    {
        const py::str decoded = raw_bytes.attr("decode")("utf-8", "replace");
        auto result = decoded.cast<std::string>();
        while (!result.empty() && result.back() == '\0') result.pop_back();
        return result;
    }
}

// ============================================================================
// Helper: get encoding from FLVER header flags
// ============================================================================

static StringEncoding get_encoding(const FLVER& f)
{
    if (!f.unicode) return StringEncoding::ShiftJIS2004;
    return f.big_endian ? StringEncoding::UTF16BE : StringEncoding::UTF16LE;
}

// ============================================================================
// Helper: convert a Python int, IntEnum, or Enum to a C++ int.
// ============================================================================
// IntEnum subclasses int, so PyLong_AsLong works directly.
// For plain Enum, we fall back to the `.value` attribute.

static int py_object_to_int(const py::object& val)
{
    // Fast path: if it's already a Python int (or IntEnum, which IS an int).
    if (PyLong_Check(val.ptr()))
    {
        return static_cast<int>(PyLong_AsLong(val.ptr()));
    }
    // Fallback: try .value attribute (plain Enum).
    if (py::hasattr(val, "value"))
    {
        return py::cast<int>(val.attr("value"));
    }
    // Last resort: try __index__ protocol.
    PyObject* idx = PyNumber_Index(val.ptr());
    if (idx)
    {
        int result = static_cast<int>(PyLong_AsLong(idx));
        Py_DECREF(idx);
        return result;
    }
    PyErr_Clear();
    throw std::runtime_error("Cannot convert Python object to int.");
}

// ============================================================================
// PyFLVER: owns a FLVER and decodes all strings in-place after parsing.
// ============================================================================

struct PyFLVER
{
    FLVER::Ptr flver;

    // Decode all in-place string fields from raw on-disk encoding to UTF-8.
    // Must be called with the GIL held (uses Python codecs).
    static void decode_strings(FLVER& f)
    {
        const StringEncoding enc = get_encoding(f);

        for (auto& bone : f.bones)
        {
            bone.name = decode_flver_string(bone.name, enc);
        }
        // Materials are denormalised per-mesh; each mesh has its own copy, so
        // no pointer-tracking is needed — each material address is unique.
        std::unordered_set<Material*> decoded_materials;
        for (auto& mesh : f.meshes)
        {
            auto* mat = &mesh.material;
            if (decoded_materials.contains(mat)) continue;
            decoded_materials.insert(mat);

            mat->name = decode_flver_string(mat->name, enc);
            mat->mat_def_path = decode_flver_string(mat->mat_def_path, enc);
            for (auto& tex : mat->textures)
            {
                tex.path = decode_flver_string(tex.path, enc);
                if (tex.texture_type.has_value())
                {
                    tex.texture_type = decode_flver_string(*tex.texture_type, enc);
                }
            }
        }
    }

    // Construct from raw (decompressed) bytes.
    explicit PyFLVER(const py::bytes& data)
    {
        const std::string buf = data;
        flver = FLVER::FromBytes(reinterpret_cast<const std::byte*>(buf.data()), buf.size());
        decode_strings(*flver);
    }

    // Construct from an already-parsed FLVER (used by from_path / load_flvers).
    // Decodes strings immediately.
    explicit PyFLVER(std::unique_ptr<FLVER> parsed)
        : flver(std::move(parsed))
    {
        decode_strings(*flver);
    }

    // Private tag for constructing without auto-decoding (parallel loader
    // decodes strings separately after all FLVERs are parsed).
    struct NoDecode {};
    PyFLVER(std::unique_ptr<FLVER> parsed, NoDecode)
        : flver(std::move(parsed)) {}
};

// ============================================================================
// Module definition
// ============================================================================

void bind_firelink_flver(py::module& m)
{
    m.doc() = "C++ FLVER reader with pybind11 bindings";

    // --- Bone ---------------------------------------------------------------
    // Exposed directly from C++ Bone; name is already decoded UTF-8.

    py::class_<Bone>(m, "Bone")
        .def_readonly("name", &Bone::name)
        .def_readonly("usage_flags", &Bone::usage_flags)
        .def_readonly("parent_bone_index", &Bone::parent_bone_index)
        .def_readonly("child_bone_index", &Bone::child_bone_index)
        .def_readonly("next_sibling_bone_index", &Bone::next_sibling_bone_index)
        .def_readonly("previous_sibling_bone_index", &Bone::previous_sibling_bone_index)
        .def_property_readonly(
            "translate", [](const Bone& b)
            {
                return py::make_tuple(b.translate.x, b.translate.y, b.translate.z);
            })
        .def_property_readonly(
            "rotate", [](const Bone& b)
            {
                return py::make_tuple(b.rotate.x, b.rotate.y, b.rotate.z);
            })
        .def_property_readonly(
            "scale", [](const Bone& b)
            {
                return py::make_tuple(b.scale.x, b.scale.y, b.scale.z);
            })
        .def_property_readonly(
            "bounding_box_min", [](const Bone& b)
            {
                return py::make_tuple(b.bounding_box.min.x, b.bounding_box.min.y, b.bounding_box.min.z);
            })
        .def_property_readonly(
            "bounding_box_max", [](const Bone& b)
            {
                return py::make_tuple(b.bounding_box.max.x, b.bounding_box.max.y, b.bounding_box.max.z);
            });

    // --- Dummy --------------------------------------------------------------
    // No strings; just rename `color` -> `color_rgba`.

    py::class_<Dummy>(m, "Dummy")
        .def_property_readonly(
            "translate", [](const Dummy& d)
            {
                return py::make_tuple(d.translate.x, d.translate.y, d.translate.z);
            })
        .def_property_readonly(
            "forward", [](const Dummy& d)
            {
                return py::make_tuple(d.forward.x, d.forward.y, d.forward.z);
            })
        .def_property_readonly(
            "upward", [](const Dummy& d)
            {
                return py::make_tuple(d.upward.x, d.upward.y, d.upward.z);
            })
        .def_property_readonly(
            "color_rgba", [](const Dummy& d)
            {
                return py::make_tuple(d.color.r, d.color.g, d.color.b, d.color.a);
            })
        .def_readonly("reference_id", &Dummy::reference_id)
        .def_readonly("parent_bone_index", &Dummy::parent_bone_index)
        .def_readonly("attach_bone_index", &Dummy::attach_bone_index)
        .def_readonly("follows_attach_bone", &Dummy::follows_attach_bone)
        .def_readonly("use_upward_vector", &Dummy::use_upward_vector)
        .def_readonly("unk_x30", &Dummy::unk_x30)
        .def_readonly("unk_x34", &Dummy::unk_x34);

    // --- Texture ------------------------------------------------------------
    // Exposed directly; path and texture_type are decoded UTF-8 in-place.

    py::class_<Texture>(m, "Texture")
        .def_readonly("path", &Texture::path)
        .def_property_readonly(
            "texture_type", [](const Texture& t) -> py::object
            {
                if (!t.texture_type.has_value()) return py::none();
                return py::str(t.texture_type.value());
            })
        .def_property_readonly(
            "scale", [](const Texture& t)
            {
                return py::make_tuple(t.scale.x, t.scale.y);
            })
        .def_readonly("f2_unk_x10", &Texture::f2_unk_x10)
        .def_readonly("f2_unk_x11", &Texture::f2_unk_x11)
        .def_readonly("f2_unk_x14", &Texture::f2_unk_x14)
        .def_readonly("f2_unk_x18", &Texture::f2_unk_x18)
        .def_readonly("f2_unk_x1c", &Texture::f2_unk_x1c);

    // --- GXItem -------------------------------------------------------------

    py::class_<GXItem>(m, "GXItem")
        .def_property_readonly(
            "category", [](const GXItem& g)
            {
                return py::bytes(g.category.data(), 4);
            })
        .def_readonly("index", &GXItem::index)
        .def_property_readonly(
            "data", [](const GXItem& g)
            {
                return py::bytes(reinterpret_cast<const char*>(g.data.data()), g.data.size());
            })
        .def_property_readonly("is_terminator", &GXItem::IsTerminator);

    // --- Material -----------------------------------------------------------
    // Exposed directly; name and mat_def_path are decoded UTF-8 in-place.

    py::class_<Material>(m, "Material")
        .def_readonly("name", &Material::name)
        .def_readonly("mat_def_path", &Material::mat_def_path)
        .def_readonly("flags", &Material::flags)
        .def_readonly("f2_unk_x18", &Material::f2_unk_x18)
        .def_property_readonly(
            "textures", [](const py::object& self)
            {
                auto& m = py::cast<Material&>(self);
                py::list result;
                for (auto& tex : m.textures)
                {
                    result.append(py::cast(tex, py::return_value_policy::reference_internal, self));
                }
                return result;
            })
        .def_property_readonly(
            "gx_items", [](const py::object& self)
            {
                auto& m = py::cast<Material&>(self);
                py::list result;
                for (auto& gx : m.gx_items)
                {
                    result.append(py::cast(gx, py::return_value_policy::reference_internal, self));
                }
                return result;
            });

    // --- FaceSet ------------------------------------------------------------

    py::class_<FaceSet>(m, "FaceSet")
        .def_readonly("flags", &FaceSet::flags)
        .def_readonly("is_triangle_strip", &FaceSet::is_triangle_strip)
        .def_readonly("use_backface_culling", &FaceSet::use_backface_culling)
        .def_readonly("unk_x06", &FaceSet::unk_x06)
        .def_property_readonly(
            "vertex_indices", [](const py::object& self)
            {
                const auto& fs = py::cast<FaceSet&>(self);
                return py::array_t<std::uint32_t>(
                    {static_cast<py::ssize_t>(fs.vertex_indices.size())},
                    {sizeof(std::uint32_t)},
                    fs.vertex_indices.data(),
                    self // parent: numpy array keeps FaceSet alive
                );
            });

    // --- Mesh ---------------------------------------------------------------
    // Exposed directly from C++ Mesh.

    py::class_<Mesh>(m, "Mesh")
        .def_readonly("is_dynamic", &Mesh::is_dynamic)
        .def_readonly("default_bone_index", &Mesh::default_bone_index)
        .def_readonly("bone_indices", &Mesh::bone_indices)
        .def_property_readonly(
            "material", [](const py::object& self)
            {
                auto& mesh = py::cast<Mesh&>(self);
                return py::cast(mesh.material, py::return_value_policy::reference_internal, self);
            })
        .def_property_readonly(
            "face_sets", [](const py::object& self)
            {
                auto& mesh = py::cast<Mesh&>(self);
                py::list result;
                for (auto& fs : mesh.face_sets)
                {
                    result.append(py::cast(fs, py::return_value_policy::reference_internal, self));
                }
                return result;
            })
        .def_readonly("uses_bounding_boxes", &Mesh::uses_bounding_boxes)
        .def_readonly("invalid_layout", &Mesh::invalid_layout)
        .def_readonly("index", &Mesh::index)
        .def_property_readonly(
            "vertex_color_count", [](const Mesh& mesh)
            {
                std::size_t count = 0;
                for (const auto& va : mesh.vertex_arrays)
                {
                    for (const auto& vdt : va.layout.types)
                    {
                        if (vdt.usage == VertexUsage::Color) ++count;
                    }
                }
                return count;
            })
        .def_property_readonly(
            "vertex_array_count", [](const Mesh& mesh)
            {
                return mesh.vertex_arrays.size();
            })
        .def_property_readonly(
            "vertex_count", [](const Mesh& mesh)
            {
                return mesh.vertex_arrays.empty() ? 0u : mesh.vertex_arrays[0].vertex_count;
            })
        .def_property_readonly(
            "use_backface_culling", [](const Mesh& mesh)
            {
                if (mesh.face_sets.empty()) throw std::runtime_error(
                    "Mesh has no face sets to check `use_backface_culling`.");
                const bool cull = mesh.face_sets[0].use_backface_culling;
                for (const auto& fs : mesh.face_sets)
                {
                    if (fs.use_backface_culling != cull)
                    {
                        throw std::runtime_error(
                            "Mesh has multiple face sets with different `use_backface_culling` values.");
                    }
                }
                return cull;
            });

    // --- MergedMesh ---------------------------------------------------------
    // Exposes flat arrays as zero-copy numpy views into the C++ vectors.
    // All numpy arrays use `self` as the base object so the MergedMesh (and
    // its data) stays alive as long as any array view exists.

    py::class_<MergedMesh>(m, "MergedMesh")
        .def_readonly("vertex_count", &MergedMesh::vertex_count)
        .def_readonly("total_loop_count", &MergedMesh::total_loop_count)
        .def_readonly("face_count", &MergedMesh::face_count)
        .def_readonly("vertices_merged", &MergedMesh::vertices_merged)

        .def_property_readonly(
            "positions", [](const py::object& self)
            {
                const auto& mm = py::cast<MergedMesh&>(self);
                return py::array_t<float>(
                    {static_cast<py::ssize_t>(mm.vertex_count), py::ssize_t(3)},
                    {3 * sizeof(float), sizeof(float)},
                    mm.positions.data(), self
                );
            })
        .def_property_readonly(
            "bone_weights", [](const py::object& self)
            {
                const auto& mm = py::cast<MergedMesh&>(self);
                return py::array_t<float>(
                    {static_cast<py::ssize_t>(mm.vertex_count), py::ssize_t(4)},
                    {4 * sizeof(float), sizeof(float)},
                    mm.bone_weights.data(), self
                );
            })
        .def_property_readonly(
            "bone_indices", [](const py::object& self)
            {
                const auto& mm = py::cast<MergedMesh&>(self);
                return py::array_t<std::int32_t>(
                    {static_cast<py::ssize_t>(mm.vertex_count), py::ssize_t(4)},
                    {4 * sizeof(std::int32_t), sizeof(std::int32_t)},
                    mm.bone_indices.data(), self
                );
            })
        .def_property_readonly(
            "loop_vertex_indices", [](const py::object& self)
            {
                const auto& mm = py::cast<MergedMesh&>(self);
                return py::array_t<std::uint32_t>(
                    {static_cast<py::ssize_t>(mm.total_loop_count)},
                    {sizeof(std::uint32_t)},
                    mm.loop_vertex_indices.data(), self
                );
            })
        .def_property_readonly(
            "loop_normals", [](const py::object& self) -> py::object
            {
                const auto& mm = py::cast<MergedMesh&>(self);
                if (mm.loop_normals.empty()) return py::none();
                return py::array_t<float>(
                    {static_cast<py::ssize_t>(mm.total_loop_count), py::ssize_t(3)},
                    {3 * sizeof(float), sizeof(float)},
                    mm.loop_normals.data(), self
                );
            })
        .def_property_readonly(
            "loop_normals_w", [](const py::object& self) -> py::object
            {
                const auto& mm = py::cast<MergedMesh&>(self);
                if (mm.loop_normals_w.empty()) return py::none();
                return py::array_t<std::uint8_t>(
                    {static_cast<py::ssize_t>(mm.total_loop_count), py::ssize_t(1)},
                    {sizeof(std::uint8_t), sizeof(std::uint8_t)},
                    mm.loop_normals_w.data(), self
                );
            })
        .def_property_readonly(
            "loop_tangents", [](const py::object& self)
            {
                const auto& mm = py::cast<MergedMesh&>(self);
                py::list result;
                for (const auto& t : mm.loop_tangents)
                {
                    result.append(
                        py::array_t<float>(
                            {static_cast<py::ssize_t>(mm.total_loop_count), py::ssize_t(4)},
                            {4 * sizeof(float), sizeof(float)},
                            t.data(), self
                        ));
                }
                return result;
            })
        .def_property_readonly(
            "loop_bitangents", [](const py::object& self) -> py::object
            {
                const auto& mm = py::cast<MergedMesh&>(self);
                if (mm.loop_bitangents.empty()) return py::none();
                return py::array_t<float>(
                    {static_cast<py::ssize_t>(mm.total_loop_count), py::ssize_t(4)},
                    {4 * sizeof(float), sizeof(float)},
                    mm.loop_bitangents.data(), self
                );
            })
        .def_property_readonly(
            "loop_vertex_colors", [](const py::object& self)
            {
                const auto& mm = py::cast<MergedMesh&>(self);
                py::list result;
                for (const auto& c : mm.loop_vertex_colors)
                {
                    result.append(
                        py::array_t<float>(
                            {static_cast<py::ssize_t>(mm.total_loop_count), py::ssize_t(4)},
                            {4 * sizeof(float), sizeof(float)},
                            c.data(), self
                        ));
                }
                return result;
            })
        .def_property_readonly(
            "loop_uvs", [](const py::object& self)
            {
                const auto& mm = py::cast<MergedMesh&>(self);
                py::dict result;
                for (const auto& layer : mm.loop_uvs)
                {
                    result[py::str(layer.name)] = py::array_t<float>(
                        {
                            static_cast<py::ssize_t>(mm.total_loop_count),
                            static_cast<py::ssize_t>(layer.dim)
                        },
                        {
                            static_cast<py::ssize_t>(layer.dim * sizeof(float)),
                            static_cast<py::ssize_t>(sizeof(float))
                        },
                        layer.data.data(), self
                    );
                }
                return result;
            })
        .def_property_readonly(
            "faces", [](const py::object& self)
            {
                const auto& mm = py::cast<MergedMesh&>(self);
                return py::array_t<std::uint32_t>(
                    {static_cast<py::ssize_t>(mm.face_count), py::ssize_t(4)},
                    {4 * sizeof(std::uint32_t), sizeof(std::uint32_t)},
                    mm.faces.data(), self
                );
            });

    // --- PyFLVER (main entry point) -----------------------------------------

    py::class_<PyFLVER>(m, "FLVER")
        .def(
            py::init<py::bytes>(), py::arg("data"),
            "Parse a FLVER from raw (decompressed) bytes.")

        // Header metadata
        .def_property_readonly(
            "version", [](const PyFLVER& f)
            {
                return static_cast<std::uint32_t>(f.flver->version);
            })
        .def_property_readonly("big_endian", [](const PyFLVER& f) { return f.flver->big_endian; })
        .def_property_readonly("unicode", [](const PyFLVER& f) { return f.flver->unicode; })
        .def_property_readonly(
            "bounding_box_min", [](const PyFLVER& f)
            {
                auto& v = f.flver->bounding_box_min;
                return py::make_tuple(v.x, v.y, v.z);
            })
        .def_property_readonly(
            "bounding_box_max", [](const PyFLVER& f)
            {
                auto& v = f.flver->bounding_box_max;
                return py::make_tuple(v.x, v.y, v.z);
            })
        .def_property_readonly("true_face_count", [](const PyFLVER& f) { return f.flver->true_face_count; })
        .def_property_readonly("total_face_count", [](const PyFLVER& f) { return f.flver->total_face_count; })

        // FLVER0 unknowns
        .def_property_readonly("f0_unk_x4a", [](const PyFLVER& f) { return f.flver->f0_unk_x4a; })
        .def_property_readonly("f0_unk_x4b", [](const PyFLVER& f) { return f.flver->f0_unk_x4b; })
        .def_property_readonly("f0_unk_x4c", [](const PyFLVER& f) { return f.flver->f0_unk_x4c; })
        .def_property_readonly("f0_unk_x5c", [](const PyFLVER& f) { return f.flver->f0_unk_x5c; })

        // FLVER2 unknowns
        .def_property_readonly("f2_unk_x4a", [](const PyFLVER& f) { return f.flver->f2_unk_x4a; })
        .def_property_readonly("f2_unk_x4c", [](const PyFLVER& f) { return f.flver->f2_unk_x4c; })
        .def_property_readonly("f2_unk_x5c", [](const PyFLVER& f) { return f.flver->f2_unk_x5c; })
        .def_property_readonly("f2_unk_x5d", [](const PyFLVER& f) { return f.flver->f2_unk_x5d; })
        .def_property_readonly("f2_unk_x68", [](const PyFLVER& f) { return f.flver->f2_unk_x68; })

        // --- Bones ----------------------------------------------------------
        // Each element is a reference into PyFLVER-owned data.
        .def_property_readonly(
            "bones", [](const py::object& self)
            {
                const auto& f = py::cast<PyFLVER&>(self);
                py::list result;
                for (auto& bone : f.flver->bones)
                {
                    result.append(py::cast(bone, py::return_value_policy::reference_internal, self));
                }
                return result;
            })

        // --- Dummies --------------------------------------------------------
        .def_property_readonly(
            "dummies", [](const py::object& self)
            {
                const auto& f = py::cast<PyFLVER&>(self);
                py::list result;
                for (auto& dummy : f.flver->dummies)
                {
                    result.append(py::cast(dummy, py::return_value_policy::reference_internal, self));
                }
                return result;
            })

        // --- Meshes ---------------------------------------------------------
        .def_property_readonly(
            "meshes", [](const py::object& self)
            {
                const auto& f = py::cast<PyFLVER&>(self);
                py::list result;
                for (auto& mesh : f.flver->meshes)
                {
                    result.append(py::cast(mesh, py::return_value_policy::reference_internal, self));
                }
                return result;
            })

        // --- MergedMesh builder ---------------------------------------------
        .def(
            "build_merged_mesh", [](
            const PyFLVER& f,
            const std::vector<std::uint32_t>& mesh_material_indices,
            const std::vector<std::vector<std::string>>& material_uv_layer_names,
            const bool merge_vertices)
            {
                return std::make_unique<MergedMesh>(
                    MergedMesh(*f.flver, mesh_material_indices, material_uv_layer_names, merge_vertices)
                );
            },
            py::arg("mesh_material_indices") = std::vector<std::uint32_t>{},
            py::arg("material_uv_layer_names") = std::vector<std::vector<std::string>>{},
            py::arg("merge_vertices") = true,
            "Build a MergedMesh from this FLVER. Returns a new MergedMesh object."
        )

        // Convenience counts
        .def_property_readonly("bone_count", [](const PyFLVER& f) { return f.flver->bones.size(); })
        .def_property_readonly("dummy_count", [](const PyFLVER& f) { return f.flver->dummies.size(); })
        .def_property_readonly("mesh_count", [](const PyFLVER& f) { return f.flver->meshes.size(); })

        // --- Source path and derived properties -----------------------------
        .def_property(
            "path",
            [](const PyFLVER& f) -> py::object
            {
                if (f.flver->GetPath().empty()) return py::none();
                return py::module_::import("pathlib").attr("Path")(f.flver->GetPath().string());
            },
            [](PyFLVER& f, const py::object& val)
            {
                if (val.is_none()) { f.flver->GetPath().clear(); return; }
                // Accept anything os.fspath-compatible (Path, str, etc.)
                py::object os = py::module_::import("os");
                f.flver->SetPath(os.attr("fspath")(val).cast<std::string>());
            },
            "Source filesystem path as pathlib.Path (not serialised)."
        )
        .def_property_readonly(
            "path_name", [](const PyFLVER& f) -> py::object
            {
                if (f.flver->GetPath().empty()) return py::none();
                return py::str(f.flver->GetPath().filename().string());
            },
            "Filename portion of path, or None.")
        .def_property_readonly(
            "path_stem", [](const PyFLVER& f) -> py::object
            {
                if (f.flver->GetPath().empty()) return py::none();
                return py::str(f.flver->GetPath().stem().string());
            },
            "Stem of path (removes last suffix only), or None.")
        .def_property_readonly(
            "path_minimal_stem", [](const PyFLVER& f) -> py::object
            {
                if (f.flver->GetPath().empty()) return py::none();
                auto stem = f.flver->GetPath().stem().string();
                auto dot = stem.find('.');
                if (dot != std::string::npos) stem = stem.substr(0, dot);
                return py::str(stem);
            },
            "Stem with ALL suffixes removed (e.g. 'c1000.chrbnd.dcx' -> 'c1000'), or None.")

        // DCX Type property, as an integer
        .def_property(
            "dcx_type",
            [](const PyFLVER& f) -> py::object
            {
                return py::int_(static_cast<int>(f.flver->GetDCXType()));
            },
            [](PyFLVER& f, const py::object& val)
            {
                if (val.is_none()) { f.flver->SetDCXType(DCXType::Null); return; }
                f.flver->SetDCXType(static_cast<DCXType>(py_object_to_int(val)));
            },
            "DCX compression type as an integer (accepts int, IntEnum, or Enum).")

        // --- from_path class method -----------------------------------------
        .def_static(
            "from_path", [](const py::object& path_obj)
            {
                // Resolve path to string under the GIL.
                py::object os = py::module_::import("os");
                auto path_str = os.attr("fspath")(path_obj).cast<std::string>();

                // Release GIL for file I/O, DCX decompression, and FLVER parsing.
                std::unique_ptr<FLVER> flver;
                {
                    py::gil_scoped_release release;
                    flver = FLVER::FromPath(path_str);
                }
                // GIL re-acquired: decode strings.
                return std::make_unique<PyFLVER>(std::move(flver));
            },
            py::arg("path"),
            "Read a FLVER from a file path, automatically decompressing DCX if needed.")
        ;

    // --- batch loaders (module-level parallel loader) -------------------------

    m.def(
        "batch_from_path", [](
            const py::list& paths,
            int max_threads,
            bool cache_merged_mesh = false)
        {
            const auto count = py::len(paths);
            if (count == 0) return py::list();

            // Phase 1 (GIL held): resolve all Python path objects to paths.
            std::vector<std::filesystem::path> fs_paths;
            fs_paths.reserve(count);
            {
                py::object os = py::module_::import("os");
                for (std::size_t i = 0; i < count; ++i)
                {
                    fs_paths.emplace_back(os.attr("fspath")(paths[i]).cast<std::string>());
                }
            }

            // Phase 2 (GIL released): parse all FLVERs in parallel.
            std::vector<FLVER::Ptr> flvers;
            {
                py::gil_scoped_release release;

                if (cache_merged_mesh)
                {
                    auto callback = [](FLVER& f)
                    {
                        f.GetMergedMesh();
                    };
                    flvers = FLVER::FromPathsParallel(fs_paths, max_threads, callback);
                }
                else
                {
                    flvers = FLVER::FromPathsParallel(fs_paths, max_threads);
                }
            }
            // GIL is re-acquired here.

            // Phase 3 (GIL held): decode strings and build Python list.
            py::list out;
            for (std::size_t i = 0; i < count; ++i)
            {
                auto py_flver = std::make_unique<PyFLVER>(std::move(flvers[i]),
                    PyFLVER::NoDecode{});
                PyFLVER::decode_strings(*py_flver->flver);
                out.append(py::cast(std::move(py_flver)));
            }
            return out;
        },
        py::arg("paths"),
        py::arg("max_threads") = 0,
        py::arg("cache_merged_mesh") = false,
        "Load multiple FLVERs from file paths in parallel.\n\n"
        "Reads, DCX-decompresses, and parses all FLVERs in parallel\n"
        "using C++ threads (releasing the GIL during I/O and parsing).\n\n"
        "Args:\n"
        "    paths: List of file paths (str or Path).\n"
        "    max_threads: Maximum number of threads. 0 (default) uses hardware_concurrency.\n\n"
        "    cache_merged_mesh: Automatically cache merged mesh.\n\n"
        "Returns:\n"
        "    List of FLVER objects in the same order as the input paths."
    );

    m.def(
        "batch_from_bytes", [](
            const py::list& byte_list,
            int max_threads,
            bool cache_merged_mesh = false)
        {
            const auto count = py::len(byte_list);
            if (count == 0) return py::list();

            // Phase 1 (GIL held): copy Python bytes into C++ vectors.
            // We must do this while the GIL is held because py::bytes are
            // Python objects.
            std::vector<std::vector<std::byte>> buffers(count);
            for (std::size_t i = 0; i < count; ++i)
            {
                auto buf = py::cast<py::bytes>(byte_list[i]);
                char* ptr;
                Py_ssize_t len;
                if (PyBytes_AsStringAndSize(buf.ptr(), &ptr, &len) != 0)
                    throw py::error_already_set();
                buffers[i].resize(static_cast<std::size_t>(len));
                std::memcpy(buffers[i].data(), ptr, static_cast<std::size_t>(len));
            }

            // Phase 2 (GIL released): parse all FLVERs in parallel.
            std::vector<FLVER::Ptr> flvers;
            {
                py::gil_scoped_release release;

                if (cache_merged_mesh)
                {
                    auto callback = [](FLVER& f)
                    {
                        f.GetMergedMesh();
                    };
                    flvers = FLVER::FromBytesParallel(std::move(buffers), max_threads, callback);
                }
                else
                {
                    flvers = FLVER::FromBytesParallel(std::move(buffers), max_threads);
                }
            }
            // GIL is re-acquired here.

            // Phase 3 (GIL held): decode strings and build Python list.
            py::list out;
            for (std::size_t i = 0; i < count; ++i)
            {
                auto py_flver = std::make_unique<PyFLVER>(std::move(flvers[i]), PyFLVER::NoDecode{});
                PyFLVER::decode_strings(*py_flver->flver);
                out.append(py::cast(std::move(py_flver)));
            }
            return out;
        },
        py::arg("data_list"),
        py::arg("max_threads") = 0,
        py::arg("cache_merged_mesh") = false,
        "Parse multiple FLVERs from raw (decompressed) byte buffers in parallel.\n\n"
        "Each element of `data_list` should be a `bytes` object containing\n"
        "already-decompressed FLVER data.  Parsing happens in parallel using\n"
        "C++ threads (the GIL is released during parsing).\n\n"
        "Args:\n"
        "    data_list: List of raw FLVER bytes objects.\n"
        "    max_threads: Maximum number of threads. 0 (default) uses hardware_concurrency.\n\n"
        "    cache_merged_mesh: Automatically cache merged mesh.\n\n"
        "Returns:\n"
        "    List of FLVER objects in the same order as the input list."
    );
}


// ============================================================================
// Module definition
// ============================================================================

PYBIND11_MODULE(_bindings, m)
{
    m.doc() = "Python bindings for FirelinkFLVER (FLVER, MergedMesh, TextureFinder).";

    bind_firelink_flver(m);
    bind_firelink_flver_texture_finder(m);
}
