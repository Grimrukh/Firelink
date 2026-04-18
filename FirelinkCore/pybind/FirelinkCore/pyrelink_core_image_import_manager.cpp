// pyrelink_core_image_import_manager.cpp — pybind11 bindings for ImageImportManager.

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <FirelinkCore/ImageImportManager.h>

namespace py = pybind11;
using namespace Firelink;


void bind_firelink_core_image_import_manager(py::module& m)
{
    py::enum_<GameType>(m, "GameType",
        "FromSoftware game identifier.")
        .value("DemonsSouls", GameType::DemonsSouls)
        .value("DarkSoulsPTDE", GameType::DarkSoulsPTDE)
        .value("DarkSoulsDSR", GameType::DarkSoulsDSR)
        .value("Bloodborne", GameType::Bloodborne)
        .value("DarkSouls3", GameType::DarkSouls3)
        .value("Sekiro", GameType::Sekiro)
        .value("EldenRing", GameType::EldenRing)
        .export_values();

    py::enum_<ImageFormat>(m, "ImageFormat",
        "Output image format for texture conversion.")
        .value("DDS", ImageFormat::DDS)
        .value("PNG", ImageFormat::PNG)
        .value("TGA", ImageFormat::TGA)
        .export_values();

    py::class_<ImageImportManager>(m, "ImageImportManager",
        "Lazy texture discovery and caching for FromSoftware game files.")
        .def(py::init<GameType, std::string>(),
            py::arg("game"), py::arg("data_root"),
            "Create a manager for the given game and data root directory.")
        .def("register_flver_sources",
            [](ImageImportManager& mgr, const std::string& path,
               const Binder* binder, bool prefer_hi_res) {
                py::gil_scoped_release release;
                mgr.RegisterFLVERSources(path, binder, prefer_hi_res);
            },
            py::arg("flver_source_path"),
            py::arg("flver_binder") = nullptr,
            py::arg("prefer_hi_res") = true,
            "Register texture source locations for a FLVER file.")
        .def("get_texture",
            [](ImageImportManager& mgr, const std::string& stem, const std::string& model_name)
                -> const TPFTexture* {
                py::gil_scoped_release release;
                return mgr.GetTexture(stem, model_name);
            },
            py::return_value_policy::reference_internal,
            py::arg("texture_stem"), py::arg("model_name") = "",
            "Look up a texture by stem. Returns None if not found.")
        .def("get_texture_as",
            [](ImageImportManager& mgr, const std::string& stem,
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
        .def("set_aet_root", &ImageImportManager::SetAETRoot,
            py::arg("aet_root"),
            "Manually set the AET root directory for asset texture lookups.")
        .def_property_readonly("cached_texture_count", &ImageImportManager::CachedTextureCount)
        .def("__repr__", [](const ImageImportManager& mgr) {
            return "<ImageImportManager cached=" + std::to_string(mgr.CachedTextureCount()) + ">";
        });
}

