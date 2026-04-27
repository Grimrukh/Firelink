// pyrelink_core_texture_finder.cpp — pybind11 bindings for TextureFinder.

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <FirelinkFLVER/TextureFinder.h>
#include <pyrelink_helpers.h>

namespace py = pybind11;
using namespace Firelink;

void bind_firelink_flver_texture_finder(py::module& m)
{
    py::enum_<ImageFormat>(m, "ImageFormat",
        "Output image format for texture conversion.")
        .value("DDS", ImageFormat::DDS)
        .value("PNG", ImageFormat::PNG)
        .value("TGA", ImageFormat::TGA)
        .export_values();

    py::class_<TextureFinder>(m, "TextureFinder",
        "Lazy texture discovery and caching for FromSoftware game files.")
        .def(py::init<GameType, std::string>(),
            py::arg("game"), py::arg("data_root"),
            "Create a manager for the given game and data root directory.")
        .def("register_flver_sources",
            [](TextureFinder& mgr, const std::string& path,
               const Binder* binder, bool prefer_hi_res) {
                py::gil_scoped_release release;
                mgr.RegisterFLVERSources(path, binder, prefer_hi_res);
            },
            py::arg("flver_source_path"),
            py::arg("flver_binder") = nullptr,
            py::arg("prefer_hi_res") = true,
            "Register texture source locations for a FLVER file.")
        .def("get_texture",
            [](TextureFinder& mgr, const std::string& stem, const std::string& model_name)
                -> const TPFTexture* {
                py::gil_scoped_release release;
                return mgr.GetTexture(stem, model_name);
            },
            py::return_value_policy::reference_internal,
            py::arg("texture_stem"), py::arg("model_name") = "",
            "Look up a texture by stem. Returns None if not found.")
        .def("get_texture_as",
            [](TextureFinder& mgr, const std::string& stem,
               ImageFormat format, const std::string& model_name) {
                std::vector<std::byte> result;
                {
                    py::gil_scoped_release release;
                    result = mgr.GetTextureAs(stem, format, model_name);
                }
                return vector_to_bytes(result);
            },
            py::arg("texture_stem"), py::arg("format"),
            py::arg("model_name") = "",
            "Get texture data converted to the requested format. Returns empty bytes if not found.")
        .def("set_aet_root", &TextureFinder::SetAETRoot,
            py::arg("aet_root"),
            "Manually set the AET root directory for asset texture lookups.")
        .def_property_readonly("cached_texture_count", &TextureFinder::CachedTextureCount)
        .def("__repr__", [](const TextureFinder& mgr) {
            return "<TextureFinder cached=" + std::to_string(mgr.CachedTextureCount()) + ">";
        });
}
