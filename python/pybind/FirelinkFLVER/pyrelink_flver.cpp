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
        .value("ArmoredCore6", FLVERVersion::ArmoredCore6)
        .export_values();

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
    // No strings; just rename `color` -> `color_rgba`.

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

        .def("get_cached_merged_mesh", &FLVER::GetCachedMergedMesh)
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
        .def_static("from_paths_parallel_with_merged_mesh", [](
            const py::object& paths, const int max_threads = 0)
        {
            // With GIL held: convert `paths` (sequence of path-likes) to a vector of paths.
            const py::sequence seq(paths);
            std::vector<std::filesystem::path> fs_paths;
            for (const auto& item : seq)
            {
                fs_paths.push_back(to_fs_path(item));
            }
            // Callback: cache merged mesh immediately.
            auto callback = [](FLVER& f)
            {
                f.GetCachedMergedMesh();
            };
            // GIL released: parse in parallel.
            py::gil_scoped_release release;
            return FLVER::FromPathsParallel(fs_paths, max_threads, callback);
        })
        .def_static("from_bytes_parallel_with_merged_mesh", [](
            const py::list& buffers, int max_threads = 0)
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
            auto callback = [](FLVER& f)
            {
                f.GetCachedMergedMesh();
            };
            // GIL released: parse in parallel.
            py::gil_scoped_release release;
            return FLVER::FromBytesParallel(std::move(data), max_threads, callback);
        });
}

PYBIND11_MODULE(_bindings, m)
{
    m.doc() = "Python bindings for FirelinkFLVER (FLVER, MergedMesh, TextureFinder).";

    bind_firelink_flver(m);
    bind_firelink_flver_texture_finder(m);
}
