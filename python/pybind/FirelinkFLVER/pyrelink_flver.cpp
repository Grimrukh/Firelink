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
#include <FirelinkFLVER/Version.h>

#include <FirelinkCore/BinaryReadWrite.h>
#include <pyrelink_helpers.h>

#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace py = pybind11;
using namespace Firelink;

void bind_firelink_flver_texture_finder(py::module& m);

namespace
{
    // --- Helpers for settable MergedMesh array properties -----------------
    // These convert incoming Python array-likes (numpy arrays, lists, etc.)
    // into flat C++ vectors, validating/deriving row counts along the way.

    // Flattens a 2-D array-like of shape (rows, cols) into a row-major
    // std::vector<T>, coercing the dtype via `forcecast`. Writes the row
    // count to `rows_out`.
    template <typename T>
    std::vector<T> FlattenRows(const py::array& arr, py::ssize_t cols, py::ssize_t& rows_out)
    {
        const auto buf = py::array_t<T, py::array::c_style | py::array::forcecast>::ensure(arr);
        if (!buf) throw std::runtime_error("Could not convert array to the expected dtype.");
        if (buf.ndim() != 2 || buf.shape(1) != cols)
        {
            throw std::runtime_error(
                "Expected an array of shape (N, " + std::to_string(cols) + "), got ndim=" +
                std::to_string(buf.ndim()) + (buf.ndim() == 2
                    ? (", cols=" + std::to_string(buf.shape(1))) : "") + ".");
        }
        rows_out = buf.shape(0);
        const T* data = buf.data();
        return std::vector<T>(data, data + buf.size());
    }

    // Flattens a 1-D array-like into a std::vector<T>, coercing the dtype.
    // Writes the element count to `count_out`.
    template <typename T>
    std::vector<T> FlattenFlat(const py::array& arr, py::ssize_t& count_out)
    {
        const auto buf = py::array_t<T, py::array::c_style | py::array::forcecast>::ensure(arr);
        if (!buf) throw std::runtime_error("Could not convert array to the expected dtype.");
        if (buf.ndim() != 1)
            throw std::runtime_error("Expected a 1-D array.");
        count_out = buf.shape(0);
        const T* data = buf.data();
        return std::vector<T>(data, data + buf.size());
    }

    // Validates that `count` is consistent with `field`'s current value (if
    // already set), or otherwise adopts `count` as the new value. Used to
    // keep `vertex_count` / `total_loop_count` / `face_count` in sync with
    // whichever array is assigned.
    void SyncCount(std::uint32_t& field, const py::ssize_t count, const char* propertyName)
    {
        if (field != 0 && static_cast<py::ssize_t>(field) != count)
        {
            throw std::runtime_error(
                std::string("Length mismatch setting '") + propertyName + "': expected " +
                std::to_string(field) + " rows but got " + std::to_string(count) + ".");
        }
        field = static_cast<std::uint32_t>(count);
    }
} // namespace

void bind_firelink_flver(py::module& m)
{
    m.doc() = "C++ FLVER reader with pybind11 bindings";

    // --- Version ---
    py::enum_<FLVERVersion>(m, "FLVERVersion", "FLVER version.")
        .value("Null", FLVERVersion::Null)
        .value("DemonsSouls_0x0F", FLVERVersion::DemonsSouls_0x0F)
        .value("DemonsSouls_0x10", FLVERVersion::DemonsSouls_0x10)
        .value("DemonsSouls_0x14", FLVERVersion::DemonsSouls_0x14)
        .value("DemonsSouls", FLVERVersion::DemonsSouls)
        .value("DarkSouls2_Armor9320", FLVERVersion::DarkSouls2_Armor9320)
        .value("DarkSouls_PS3_o0700_o0701", FLVERVersion::DarkSouls_PS3_o0700_o0701)
        .value("DarkSouls_A", FLVERVersion::DarkSouls_A)
        .value("DarkSouls_B", FLVERVersion::DarkSouls_B)
        .value("DarkSouls2_NT", FLVERVersion::DarkSouls2_NT)
        .value("DarkSouls2", FLVERVersion::DarkSouls2)
        .value("Bloodborne_DS3_A", FLVERVersion::Bloodborne_DS3_A)
        .value("Bloodborne_DS3_B", FLVERVersion::Bloodborne_DS3_B)
        .value("Sekiro_TestChr", FLVERVersion::Sekiro_TestChr)
        .value("Sekiro_EldenRing", FLVERVersion::Sekiro_EldenRing)
        .value("ArmoredCore6", FLVERVersion::ArmoredCore6);

    // --- Bone ---------------------------------------------------------------
    // Exposed directly from C++ Bone; name is already decoded UTF-8.

    py::class_<Bone>(m, "Bone")
        .def_readonly("name", &Bone::name)
        .def_readonly("usage_flags", &Bone::usage_flags)
        .def_readonly("parent_bone_index", &Bone::parent_bone_index)
        .def_readonly("child_bone_index", &Bone::child_bone_index)
        .def_readonly("next_sibling_bone_index", &Bone::next_sibling_bone_index)
        .def_readonly("previous_sibling_bone_index", &Bone::previous_sibling_bone_index)
        .def_readwrite("translate", &Bone::translate)
        .def_readwrite("rotate", &Bone::rotate)
        .def_readwrite("scale", &Bone::scale)
        .def_readwrite("bounding_box", &Bone::bounding_box);

    // --- Dummy --------------------------------------------------------------

    py::class_<Dummy>(m, "Dummy")
        .def(py::init<>())
        .def_readwrite("translate", &Dummy::translate)
        .def_readwrite("forward", &Dummy::forward)
        .def_readwrite("upward", &Dummy::upward)
        .def_readwrite("color", &Dummy::color)
        .def_readwrite("reference_id", &Dummy::reference_id)
        .def_readwrite("parent_bone_index", &Dummy::parent_bone_index)
        .def_readwrite("attach_bone_index", &Dummy::attach_bone_index)
        .def_readwrite("follows_attach_bone", &Dummy::follows_attach_bone)
        .def_readwrite("use_upward_vector", &Dummy::use_upward_vector)
        .def_readwrite("unk_x30", &Dummy::unk_x30)
        .def_readwrite("unk_x34", &Dummy::unk_x34);

    // --- Texture ------------------------------------------------------------
    // Exposed directly; path and texture_type are decoded UTF-8 in-place.

    py::class_<Texture>(m, "Texture")
        .def_readwrite("path", &Texture::path)
        .def_readwrite("texture_type", &Texture::texture_type)
        .def_readwrite("scale", &Texture::scale)
        .def_readwrite("f2_unk_x10", &Texture::f2_unk_x10)
        .def_readwrite("f2_unk_x11", &Texture::f2_unk_x11)
        .def_readwrite("f2_unk_x14", &Texture::f2_unk_x14)
        .def_readwrite("f2_unk_x18", &Texture::f2_unk_x18)
        .def_readwrite("f2_unk_x1c", &Texture::f2_unk_x1c);

    // --- GXItem -------------------------------------------------------------

    py::class_<GXItem>(m, "GXItem")
        // TODO: editable category (but must have length 4)
        .def_property_readonly(
            "category", [](const GXItem& g)
            {
                return py::bytes(g.category.data(), 4);
            })
        .def_readwrite("index", &GXItem::index)
        // TODO: editable data
        .def_property_readonly(
            "data", [](const GXItem& g)
            {
                return py::bytes(reinterpret_cast<const char*>(g.data.data()), g.data.size());
            })
        .def_property_readonly("is_terminator", &GXItem::IsTerminator);

    // --- Material -----------------------------------------------------------
    // Exposed directly; name and mat_def_path are decoded UTF-8 in-place.

    py::class_<Material>(m, "Material")
        .def_readwrite("name", &Material::name)
        .def_readwrite("mat_def_path", &Material::mat_def_path)
        .def_readwrite("flags", &Material::flags)
        .def_readwrite("f2_unk_x18", &Material::f2_unk_x18)
        .def_property_readonly("textures",
            [](Material& mat) -> std::vector<Texture>& {
                return mat.textures;
            },
            py::return_value_policy::reference_internal,
            "List of textures (mutable).")
        .def_property_readonly("gx_items",
            [](Material& mat) -> std::vector<GXItem>& {
                return mat.gx_items;
            },
            py::return_value_policy::reference_internal,
            "List of GX items (mutable).");

    // --- VertexUsage / VertexDataFormat / VertexDataType / VertexArrayLayout ---
    // Needed to build `SplitMeshDef.layout` for `MergedMesh.split_mesh()`.

    py::enum_<VertexUsage>(m, "VertexUsage")
        .value("Position", VertexUsage::Position)
        .value("BoneWeights", VertexUsage::BoneWeights)
        .value("BoneIndices", VertexUsage::BoneIndices)
        .value("Normal", VertexUsage::Normal)
        .value("Tangent", VertexUsage::Tangent)
        .value("Bitangent", VertexUsage::Bitangent)
        .value("Color", VertexUsage::Color)
        .value("UV", VertexUsage::UV)
        .value("Ignore", VertexUsage::Ignore);

    py::enum_<VertexDataFormatEnum>(m, "VertexDataFormat")
        .value("TwoFloats", VertexDataFormatEnum::TwoFloats)
        .value("ThreeFloats", VertexDataFormatEnum::ThreeFloats)
        .value("FourFloats", VertexDataFormatEnum::FourFloats)
        .value("FourBytesA", VertexDataFormatEnum::FourBytesA)
        .value("FourBytesB", VertexDataFormatEnum::FourBytesB)
        .value("FourBytesC", VertexDataFormatEnum::FourBytesC)
        .value("FourBytesD_NormalW", VertexDataFormatEnum::FourBytesD_NormalW)
        .value("TwoShorts", VertexDataFormatEnum::TwoShorts)
        .value("FourShorts", VertexDataFormatEnum::FourShorts)
        .value("FourShortsBones", VertexDataFormatEnum::FourShortsBones)
        .value("FourShortsToFloats", VertexDataFormatEnum::FourShortsToFloats)
        .value("FourShortsToFloatsB", VertexDataFormatEnum::FourShortsToFloatsB)
        .value("FourBytesE", VertexDataFormatEnum::FourBytesE)
        .value("EdgeCompressed", VertexDataFormatEnum::EdgeCompressed)
        .value("Ignored", VertexDataFormatEnum::Ignored);

    py::class_<VertexDataType>(m, "VertexDataType")
        .def(py::init([](
            const VertexUsage usage,
            const VertexDataFormatEnum format,
            const std::uint32_t instance_index,
            const std::uint32_t unk_x00,
            const std::uint32_t data_offset)
            {
                VertexDataType vdt;
                vdt.usage = usage;
                vdt.format = format;
                vdt.instance_index = instance_index;
                vdt.unk_x00 = unk_x00;
                vdt.data_offset = data_offset;
                return vdt;
            }),
            py::arg("usage") = VertexUsage::Ignore,
            py::arg("format") = VertexDataFormatEnum::Ignored,
            py::arg("instance_index") = 0,
            py::arg("unk_x00") = 0,
            py::arg("data_offset") = 0)
        .def_readwrite("usage", &VertexDataType::usage)
        .def_readwrite("format", &VertexDataType::format)
        .def_readwrite("instance_index", &VertexDataType::instance_index)
        .def_readwrite("unk_x00", &VertexDataType::unk_x00)
        .def_readwrite("data_offset", &VertexDataType::data_offset)
        .def_property_readonly("compressed_size", &VertexDataType::CompressedSize);

    py::class_<VertexArrayLayout>(m, "VertexArrayLayout")
        .def(py::init([](std::vector<VertexDataType> types)
            {
                VertexArrayLayout layout;
                layout.types = std::move(types);
                return layout;
            }),
            py::arg("types") = std::vector<VertexDataType>{})
        .def_readwrite("types", &VertexArrayLayout::types)
        .def_readonly("compressed_vertex_size", &VertexArrayLayout::compressed_vertex_size)
        .def_readonly("decompressed_vertex_size", &VertexArrayLayout::decompressed_vertex_size)
        .def("get_compressed_vertex_size", &VertexArrayLayout::GetCompressedVertexSize,
            "Compute compressed vertex size for writing.")
        .def("get_hash", &VertexArrayLayout::GetHash);

    // --- FaceSet ------------------------------------------------------------

    py::class_<FaceSet>(m, "FaceSet")
        .def_readwrite("flags", &FaceSet::flags)
        .def_readwrite("is_triangle_strip", &FaceSet::is_triangle_strip)
        .def_readwrite("use_backface_culling", &FaceSet::use_backface_culling)
        .def_readwrite("unk_x06", &FaceSet::unk_x06)
        // TODO: editable vertex indices
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
        .def_readwrite("is_dynamic", &Mesh::is_dynamic)
        .def_readwrite("default_bone_index", &Mesh::default_bone_index)
        .def_readwrite("bone_indices", &Mesh::bone_indices)
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
        .def_readwrite("uses_bounding_boxes", &Mesh::uses_bounding_boxes)
        .def_readwrite("invalid_layout", &Mesh::invalid_layout)
        .def_readwrite("index", &Mesh::index)
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

    // --- SplitMeshDef / SplitMeshParams -------------------------------------
    // Inputs to `MergedMesh.split_mesh()`.

    py::class_<SplitMeshDef>(m, "SplitMeshDef")
        .def(py::init([](
            Material material,
            VertexArrayLayout layout,
            const bool is_dynamic,
            const bool use_backface_culling,
            const std::int32_t default_bone_index,
            const bool uses_bounding_boxes,
            const int face_set_count,
            std::vector<std::string> uv_layer_names)
            {
                SplitMeshDef def;
                def.material = std::move(material);
                def.layout = std::move(layout);
                def.is_dynamic = is_dynamic;
                def.use_backface_culling = use_backface_culling;
                def.default_bone_index = default_bone_index;
                def.uses_bounding_boxes = uses_bounding_boxes;
                def.face_set_count = face_set_count;
                def.uv_layer_names = std::move(uv_layer_names);
                return def;
            }),
            py::arg("material"),
            py::arg("layout"),
            py::arg("is_dynamic") = false,
            py::arg("use_backface_culling") = true,
            py::arg("default_bone_index") = 0,
            py::arg("uses_bounding_boxes") = true,
            py::arg("face_set_count") = 1,
            py::arg("uv_layer_names") = std::vector<std::string>{},
            "One output FLVER submesh definition, supplied per distinct value of "
            "`MergedMesh.faces[:, 3]`.")
        .def_readwrite("material", &SplitMeshDef::material)
        .def_readwrite("layout", &SplitMeshDef::layout)
        .def_readwrite("is_dynamic", &SplitMeshDef::is_dynamic)
        .def_readwrite("use_backface_culling", &SplitMeshDef::use_backface_culling)
        .def_readwrite("default_bone_index", &SplitMeshDef::default_bone_index)
        .def_readwrite("uses_bounding_boxes", &SplitMeshDef::uses_bounding_boxes)
        .def_readwrite("face_set_count", &SplitMeshDef::face_set_count)
        .def_readwrite("uv_layer_names", &SplitMeshDef::uv_layer_names);

    py::class_<SplitMeshParams>(m, "SplitMeshParams")
        .def(py::init([](
            const bool use_mesh_bone_indices,
            const int max_bones_per_mesh,
            const bool unused_bone_indices_are_minus_one,
            const float normal_tangent_dot_threshold,
            const std::uint32_t max_vertices_per_mesh,
            const bool is_flver0)
            {
                SplitMeshParams params;
                params.useMeshBoneIndices = use_mesh_bone_indices;
                params.maxBonesPerMesh = max_bones_per_mesh;
                params.unusedBoneIndicesAreMinusOne = unused_bone_indices_are_minus_one;
                params.normalTangentDotThreshold = normal_tangent_dot_threshold;
                params.maxVerticesPerMesh = max_vertices_per_mesh;
                params.isFlver0 = is_flver0;
                return params;
            }),
            py::arg("use_mesh_bone_indices") = true,
            py::arg("max_bones_per_mesh") = 38,
            py::arg("unused_bone_indices_are_minus_one") = false,
            py::arg("normal_tangent_dot_threshold") = 1.0f,
            py::arg("max_vertices_per_mesh") = 0,
            py::arg("is_flver0") = false,
            "Settings for `MergedMesh.split_mesh()`. Defaults are geared towards Dark Souls 1 (PTDE/DSR).")
        .def_readwrite("use_mesh_bone_indices", &SplitMeshParams::useMeshBoneIndices)
        .def_readwrite("max_bones_per_mesh", &SplitMeshParams::maxBonesPerMesh)
        .def_readwrite("unused_bone_indices_are_minus_one", &SplitMeshParams::unusedBoneIndicesAreMinusOne)
        .def_readwrite("normal_tangent_dot_threshold", &SplitMeshParams::normalTangentDotThreshold)
        .def_readwrite("max_vertices_per_mesh", &SplitMeshParams::maxVerticesPerMesh)
        .def_readwrite("is_flver0", &SplitMeshParams::isFlver0);

    // --- MergedMesh ---------------------------------------------------------
    // Exposes flat arrays as zero-copy numpy views into the C++ vectors for
    // reading. Setters accept array-likes (numpy arrays, lists, etc.), copy
    // their data into the underlying vectors, and keep `vertex_count` /
    // `total_loop_count` / `face_count` in sync so that a `MergedMesh` can be
    // constructed and populated entirely from Python (e.g. to call
    // `split_mesh()` without first building one from a `FLVER`).

    auto merged_mesh = py::class_<MergedMesh>(m, "MergedMesh");

    py::class_<MergedMesh::UVLayer>(merged_mesh, "UVLayer")
        .def(py::init([](std::string name, const std::uint32_t dim, std::vector<float> data)
            {
                MergedMesh::UVLayer layer;
                layer.name = std::move(name);
                layer.dim = dim;
                layer.data = std::move(data);
                return layer;
            }),
            py::arg("name") = std::string(),
            py::arg("dim") = 2,
            py::arg("data") = std::vector<float>{},
            "One named UV layer. `data` is a flat (loop_count * dim) float array.")
        .def_readwrite("name", &MergedMesh::UVLayer::name, "e.g. \"UVMap0\", \"UVMap1\".")
        .def_readwrite("dim", &MergedMesh::UVLayer::dim, "Columns per UV (usually 2, up to 4).")
        .def_readwrite("data", &MergedMesh::UVLayer::data, "Flat (loop_count * dim) float array.");

    merged_mesh
        .def(py::init<>(), "Construct an empty MergedMesh to populate manually from Python.")
        .def_readwrite("vertex_count", &MergedMesh::vertex_count)
        .def_readwrite("total_loop_count", &MergedMesh::total_loop_count)
        .def_readwrite("face_count", &MergedMesh::face_count)
        .def_readwrite("vertices_merged", &MergedMesh::vertices_merged)

        .def_property(
            "positions",
            [](const py::object& self)
            {
                const auto& mm = py::cast<MergedMesh&>(self);
                return py::array_t<float>(
                    {static_cast<py::ssize_t>(mm.vertex_count), py::ssize_t(3)},
                    {3 * sizeof(float), sizeof(float)},
                    mm.positions.data(), self
                );
            },
            [](MergedMesh& mm, const py::array& arr)
            {
                py::ssize_t rows = 0;
                mm.positions = FlattenRows<float>(arr, 3, rows);
                SyncCount(mm.vertex_count, rows, "positions");
            },
            "Shape (vertex_count, 3).")
        .def_property(
            "bone_weights",
            [](const py::object& self)
            {
                const auto& mm = py::cast<MergedMesh&>(self);
                return py::array_t<float>(
                    {static_cast<py::ssize_t>(mm.vertex_count), py::ssize_t(4)},
                    {4 * sizeof(float), sizeof(float)},
                    mm.bone_weights.data(), self
                );
            },
            [](MergedMesh& mm, const py::array& arr)
            {
                py::ssize_t rows = 0;
                mm.bone_weights = FlattenRows<float>(arr, 4, rows);
                SyncCount(mm.vertex_count, rows, "bone_weights");
            },
            "Shape (vertex_count, 4).")
        .def_property(
            "bone_indices",
            [](const py::object& self)
            {
                const auto& mm = py::cast<MergedMesh&>(self);
                return py::array_t<std::int32_t>(
                    {static_cast<py::ssize_t>(mm.vertex_count), py::ssize_t(4)},
                    {4 * sizeof(std::int32_t), sizeof(std::int32_t)},
                    mm.bone_indices.data(), self
                );
            },
            [](MergedMesh& mm, const py::array& arr)
            {
                py::ssize_t rows = 0;
                mm.bone_indices = FlattenRows<std::int32_t>(arr, 4, rows);
                SyncCount(mm.vertex_count, rows, "bone_indices");
            },
            "Shape (vertex_count, 4).")
        .def_property(
            "loop_vertex_indices",
            [](const py::object& self)
            {
                const auto& mm = py::cast<MergedMesh&>(self);
                return py::array_t<std::uint32_t>(
                    {static_cast<py::ssize_t>(mm.total_loop_count)},
                    {sizeof(std::uint32_t)},
                    mm.loop_vertex_indices.data(), self
                );
            },
            [](MergedMesh& mm, const py::array& arr)
            {
                py::ssize_t count = 0;
                mm.loop_vertex_indices = FlattenFlat<std::uint32_t>(arr, count);
                SyncCount(mm.total_loop_count, count, "loop_vertex_indices");
            },
            "Shape (total_loop_count,).")
        .def_property(
            "loop_normals",
            [](const py::object& self) -> py::object
            {
                const auto& mm = py::cast<MergedMesh&>(self);
                if (mm.loop_normals.empty()) return py::none();
                return py::array_t<float>(
                    {static_cast<py::ssize_t>(mm.total_loop_count), py::ssize_t(3)},
                    {3 * sizeof(float), sizeof(float)},
                    mm.loop_normals.data(), self
                );
            },
            [](MergedMesh& mm, const py::object& value)
            {
                if (value.is_none()) { mm.loop_normals.clear(); return; }
                py::ssize_t rows = 0;
                mm.loop_normals = FlattenRows<float>(py::cast<py::array>(value), 3, rows);
                SyncCount(mm.total_loop_count, rows, "loop_normals");
            },
            "Shape (total_loop_count, 3) or None.")
        .def_property(
            "loop_normals_w",
            [](const py::object& self) -> py::object
            {
                const auto& mm = py::cast<MergedMesh&>(self);
                if (mm.loop_normals_w.empty()) return py::none();
                return py::array_t<std::uint8_t>(
                    {static_cast<py::ssize_t>(mm.total_loop_count), py::ssize_t(1)},
                    {sizeof(std::uint8_t), sizeof(std::uint8_t)},
                    mm.loop_normals_w.data(), self
                );
            },
            [](MergedMesh& mm, const py::object& value)
            {
                if (value.is_none()) { mm.loop_normals_w.clear(); return; }
                py::ssize_t count = 0;
                const auto arr = py::cast<py::array>(value);
                if (arr.ndim() == 2)
                {
                    py::ssize_t rows = 0;
                    mm.loop_normals_w = FlattenRows<std::uint8_t>(arr, 1, rows);
                    count = rows;
                }
                else
                {
                    mm.loop_normals_w = FlattenFlat<std::uint8_t>(arr, count);
                }
                SyncCount(mm.total_loop_count, count, "loop_normals_w");
            },
            "Shape (total_loop_count, 1) or None.")
        .def_property(
            "loop_tangents",
            [](const py::object& self)
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
            },
            [](MergedMesh& mm, const std::vector<py::array>& slots)
            {
                std::vector<std::vector<float>> tangents;
                tangents.reserve(slots.size());
                for (const auto& slot : slots)
                {
                    py::ssize_t rows = 0;
                    tangents.push_back(FlattenRows<float>(slot, 4, rows));
                    SyncCount(mm.total_loop_count, rows, "loop_tangents");
                }
                mm.loop_tangents = std::move(tangents);
            },
            "List of tangent slot arrays, each shape (total_loop_count, 4).")
        .def_property(
            "loop_bitangents",
            [](const py::object& self) -> py::object
            {
                const auto& mm = py::cast<MergedMesh&>(self);
                if (mm.loop_bitangents.empty()) return py::none();
                return py::array_t<float>(
                    {static_cast<py::ssize_t>(mm.total_loop_count), py::ssize_t(4)},
                    {4 * sizeof(float), sizeof(float)},
                    mm.loop_bitangents.data(), self
                );
            },
            [](MergedMesh& mm, const py::object& value)
            {
                if (value.is_none()) { mm.loop_bitangents.clear(); return; }
                py::ssize_t rows = 0;
                mm.loop_bitangents = FlattenRows<float>(py::cast<py::array>(value), 4, rows);
                SyncCount(mm.total_loop_count, rows, "loop_bitangents");
            },
            "Shape (total_loop_count, 4) or None.")
        .def_property(
            "loop_vertex_colors",
            [](const py::object& self)
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
            },
            [](MergedMesh& mm, const std::vector<py::array>& slots)
            {
                std::vector<std::vector<float>> colors;
                colors.reserve(slots.size());
                for (const auto& slot : slots)
                {
                    py::ssize_t rows = 0;
                    colors.push_back(FlattenRows<float>(slot, 4, rows));
                    SyncCount(mm.total_loop_count, rows, "loop_vertex_colors");
                }
                mm.loop_vertex_colors = std::move(colors);
            },
            "List of vertex color slot arrays, each shape (total_loop_count, 4).")
        .def_property(
            "loop_uvs",
            [](const py::object& self)
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
            },
            [](MergedMesh& mm, const py::object& value)
            {
                // Accepts either a list of `MergedMesh.UVLayer` (preserves order
                // and per-layer `dim` exactly), or a dict of name -> (N, dim)
                // array-like (dim inferred per-entry) for convenience.
                std::vector<MergedMesh::UVLayer> layers;
                if (py::isinstance<py::dict>(value))
                {
                    for (auto item : py::cast<py::dict>(value))
                    {
                        const auto arr = py::cast<py::array>(item.second);
                        if (arr.ndim() != 2)
                            throw std::runtime_error("UV layer array must be 2-D (N, dim).");
                        MergedMesh::UVLayer layer;
                        layer.name = py::cast<std::string>(item.first);
                        layer.dim = static_cast<std::uint32_t>(arr.shape(1));
                        py::ssize_t rows = 0;
                        layer.data = FlattenRows<float>(arr, arr.shape(1), rows);
                        SyncCount(mm.total_loop_count, rows, "loop_uvs");
                        layers.push_back(std::move(layer));
                    }
                }
                else
                {
                    layers = py::cast<std::vector<MergedMesh::UVLayer>>(value);
                    for (const auto& layer : layers)
                    {
                        if (layer.dim == 0) continue;
                        SyncCount(
                            mm.total_loop_count,
                            static_cast<py::ssize_t>(layer.data.size() / layer.dim),
                            "loop_uvs");
                    }
                }
                mm.loop_uvs = std::move(layers);
            },
            "UV layers keyed by name (dict getter). Settable from a dict of "
            "name -> (N, dim) arrays, or a list of `MergedMesh.UVLayer`.")
        .def_property(
            "faces",
            [](const py::object& self)
            {
                const auto& mm = py::cast<MergedMesh&>(self);
                return py::array_t<std::uint32_t>(
                    {static_cast<py::ssize_t>(mm.face_count), py::ssize_t(4)},
                    {4 * sizeof(std::uint32_t), sizeof(std::uint32_t)},
                    mm.faces.data(), self
                );
            },
            [](MergedMesh& mm, const py::array& arr)
            {
                py::ssize_t rows = 0;
                mm.faces = FlattenRows<std::uint32_t>(arr, 4, rows);
                SyncCount(mm.face_count, rows, "faces");
            },
            "Shape (face_count, 4).")
        .def(
            "split_mesh", &MergedMesh::SplitMesh,
            py::arg("split_mesh_defs"),
            py::arg("params"),
            "Split this merged mesh into FLVER submeshes, one per entry of "
            "`split_mesh_defs` (and possibly several per entry, when bone-count "
            "sub-splitting is required).");

    // --- FLVER ---

    auto flver = py::class_<FLVER>(m, "FLVER", "FromSoftware's FLVER 3D model class.");

    bind_game_file(flver);

    flver
        // Header metadata
        .def_property("version", &FLVER::GetVersion, &FLVER::SetVersion)
        .def_property("big_endian", &FLVER::GetIsBigEndian, &FLVER::SetIsBigEndian)
        .def_property("unicode", &FLVER::GetIsUnicode, &FLVER::SetIsUnicode)
        .def_property_readonly("bounding_box", static_cast<AABB& (FLVER::*)()>(&FLVER::BoundingBox))
        .def_property_readonly("true_face_count", &FLVER::GetTrueFaceCount)
        .def_property_readonly("total_face_count", &FLVER::GetTotalFaceCount)

        // FLVER0 unknowns
        .def_property("f0_unk_x4a", &FLVER::GetF0Unk4a, &FLVER::SetF0Unk4a)
        .def_property("f0_unk_x4b", &FLVER::GetF0Unk4b, &FLVER::SetF0Unk4b)
        .def_property("f0_unk_x4c", &FLVER::GetF0Unk4c, &FLVER::SetF0Unk4c)
        .def_property("f0_unk_x5c", &FLVER::GetF0Unk5c, &FLVER::SetF0Unk5c)

        // FLVER2 unknowns
        .def_property("f2_unk_x4a", &FLVER::GetF2Unk4a, &FLVER::SetF2Unk4a)
        .def_property("f2_unk_x4c", &FLVER::GetF2Unk4c, &FLVER::SetF2Unk4c)
        .def_property("f2_unk_x5c", &FLVER::GetF2Unk5c, &FLVER::SetF2Unk5c)
        .def_property("f2_unk_x5d", &FLVER::GetF2Unk5d, &FLVER::SetF2Unk5d)
        .def_property("f2_unk_x68", &FLVER::GetF2Unk68, &FLVER::SetF2Unk68)

        .def_property_readonly("bones",
            [](FLVER& f) -> std::vector<Bone>& {
                return f.Bones();
            },
            py::return_value_policy::reference_internal,
            "List of bones (mutable).")
        .def_property_readonly("dummies",
            [](FLVER& f) -> std::vector<Dummy>& {
                return f.Dummies();
            },
            py::return_value_policy::reference_internal,
            "List of dummies (mutable).")
        .def_property_readonly("meshes",
            [](FLVER& f) -> std::vector<Mesh>& {
                return f.Meshes();
            },
            py::return_value_policy::reference_internal,
            "List of meshes (mutable).")

        .def("has_cached_merged_mesh", &FLVER::HasCachedMergedMesh)
        .def("get_cached_merged_mesh", &FLVER::GetCachedMergedMesh)
        .def("update_cached_merged_mesh", &FLVER::UpdateCachedMergedMesh)
        .def("clear_cached_merged_mesh", &FLVER::ClearCachedMergedMesh)

        // --- MergedMesh builder ---------------------------------------------
        .def(
            "build_merged_mesh", [](
            const FLVER& f,
            const std::vector<std::uint32_t>& mesh_material_indices,
            const std::vector<std::vector<std::string>>& material_uv_layer_names,
            const bool merge_vertices)
            {
                return std::make_unique<MergedMesh>(
                    MergedMesh(f, mesh_material_indices, material_uv_layer_names, merge_vertices)
                );
            },
            py::arg("mesh_material_indices") = std::vector<std::uint32_t>{},
            py::arg("material_uv_layer_names") = std::vector<std::vector<std::string>>{},
            py::arg("merge_vertices") = true,
            "Build a MergedMesh from this FLVER. Returns a new MergedMesh object."
        )

        // Convenience counts
        .def_property_readonly("bone_count", [](const FLVER& f) { return f.Bones().size(); })
        .def_property_readonly("dummy_count", [](const FLVER& f) { return f.Dummies().size(); })
        .def_property_readonly("mesh_count", [](const FLVER& f) { return f.Meshes().size(); });

    flver
        .def_static(
            "from_paths_parallel_with_merged_mesh",
            [](const std::vector<std::filesystem::path>& paths,
                 const std::vector<std::uint32_t>& mesh_material_indices = {},
                 const std::vector<std::vector<std::string>>& material_uv_layer_names = {},
                 const bool merge_vertices = true,
                 const int max_threads = 0)
            {
                // Callback: cache merged mesh immediately.
                auto callback = [&](FLVER& f)
                {
                    f.UpdateCachedMergedMesh(
                        mesh_material_indices, material_uv_layer_names, merge_vertices);
                };
                // GIL released: parse in parallel.
                py::gil_scoped_release release;
                return FLVER::FromPathsParallel(paths, max_threads, callback);
            },
            py::arg("paths"),
            py::arg("mesh_material_indices") = std::vector<std::uint32_t>{},
            py::arg("material_uv_layer_names") = std::vector<std::vector<std::string>>{},
            py::arg("merge_vertices") = true,
            py::arg("max_threads") = 0)
        .def_static(
            "from_bytes_parallel_with_merged_mesh",
            [](const py::list& buffers,
                 const std::vector<std::uint32_t>& mesh_material_indices = {},
                 const std::vector<std::vector<std::string>>& material_uv_layer_names = {},
                 const bool merge_vertices = true,
                 const int max_threads = 0)
            {
                // With GIL held: copy each buffer into a C++ vector.
                std::vector<std::vector<std::byte>> data;
                data.reserve(buffers.size());
                for (const auto& item : buffers)
                {
                    auto [ptr, size] = borrow_buffer(py::cast<py::buffer>(item));
                    data.emplace_back(ptr, ptr + size);
                }
                // Callback: cache merged mesh immediately.
                auto callback = [&](FLVER& f)
                {
                    f.UpdateCachedMergedMesh(
                        mesh_material_indices, material_uv_layer_names, merge_vertices);
                };
                // GIL released: parse in parallel.
                py::gil_scoped_release release;
                return FLVER::FromBytesParallel(std::move(data), max_threads, callback);
            },
            py::arg("buffers"),
            py::arg("mesh_material_indices") = std::vector<std::uint32_t>{},
            py::arg("material_uv_layer_names") = std::vector<std::vector<std::string>>{},
            py::arg("merge_vertices") = true,
            py::arg("max_threads") = 0)
        .def_static(
            "update_cached_merged_meshes_parallel",
            [](const py::list& flvers,
               const std::vector<std::vector<std::uint32_t>>& mesh_material_indices,
               const std::vector<std::vector<std::vector<std::string>>>& material_uv_layer_names,
               const std::vector<bool>& merge_vertices,
               const int max_threads)
            {
                // With GIL held: collect raw FLVER pointers from the Python list.
                std::vector<FLVER*> flver_ptrs;
                flver_ptrs.reserve(flvers.size());
                for (const auto& item : flvers)
                    flver_ptrs.push_back(&py::cast<FLVER&>(item));
                // GIL released: run parallel MergedMesh construction.
                py::gil_scoped_release release;
                return FLVER::UpdateCachedMergedMeshesParallel(
                    flver_ptrs, mesh_material_indices, material_uv_layer_names,
                    merge_vertices, max_threads);
            },
            py::arg("flvers"),
            py::arg("mesh_material_indices") = std::vector<std::vector<std::uint32_t>>{},
            py::arg("material_uv_layer_names") = std::vector<std::vector<std::vector<std::string>>>{},
            py::arg("merge_vertices") = std::vector<bool>{},
            py::arg("max_threads") = 0);
}

PYBIND11_MODULE(_bindings, m)
{
    m.doc() = "Python bindings for FirelinkFLVER (FLVER, MergedMesh, TextureFinder).";

    bind_firelink_flver(m);
    bind_firelink_flver_texture_finder(m);
}
