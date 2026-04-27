// pyrelink_core_tpf.cpp — pybind11 bindings for FirelinkCore TPF module.

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <FirelinkCore/TPF.h>
#include <pyrelink_helpers.h>

namespace py = pybind11;
using namespace Firelink;

void bind_firelink_core_tpf(py::module& m)
{
    py::enum_<TPFPlatform>(m, "TPFPlatform",
        "TPF target platform.")
        .value("PC", TPFPlatform::PC)
        .value("Xbox360", TPFPlatform::Xbox360)
        .value("PS3", TPFPlatform::PS3)
        .value("PS4", TPFPlatform::PS4)
        .value("XboxOne", TPFPlatform::XboxOne)
        .export_values();

    py::enum_<TextureType>(m, "TextureType",
        "TPF texture type.")
        .value("Texture", TextureType::Texture)
        .value("Cubemap", TextureType::Cubemap)
        .value("Volume", TextureType::Volume)
        .export_values();

    py::class_<TPFTexture>(m, "TPFTexture",
        "A single texture in a TPF archive.")
        .def(py::init<>())
        .def_readwrite("stem", &TPFTexture::stem)
        .def_readwrite("format", &TPFTexture::format)
        .def_readwrite("texture_type", &TPFTexture::texture_type)
        .def_readwrite("mipmap_count", &TPFTexture::mipmap_count)
        .def_readwrite("texture_flags", &TPFTexture::texture_flags)
        .def_property("data",
            [](const TPFTexture& t) {
                return py::bytes(reinterpret_cast<const char*>(t.data.data()), t.data.size());
            },
            [](TPFTexture& t, const py::buffer& buf) {
                auto info = buf.request();
                auto ptr = static_cast<const std::byte*>(info.ptr);
                auto sz = static_cast<std::size_t>(info.size * info.itemsize);
                t.data.assign(ptr, ptr + sz);
            },
            "Texture DDS data as bytes.")
        .def("__repr__", [](const TPFTexture& t) {
            return "<TPFTexture stem='" + t.stem + "' " + std::to_string(t.data.size()) + " bytes>";
        });

    py::register_exception<TPFError>(m, "TPFError", PyExc_RuntimeError);

    auto tpf = py::class_<TPF>(m, "TPF",
        "A FromSoftware TPF texture pack file.");

    bind_game_file(tpf);

    tpf
        .def_property("platform", &TPF::GetPlatform, &TPF::SetPlatform)
        .def_property("tpf_flags", &TPF::GetFlags, &TPF::SetFlags)
        .def_property("encoding_type", &TPF::GetEncodingType, &TPF::SetEncodingType)
        .def_property_readonly("textures",
            [](TPF& t) -> std::vector<TPFTexture>& {
                return t.Textures();
            },
            py::return_value_policy::reference_internal,
            "List of TPF textures (mutable).")
        .def_property_readonly("texture_count", &TPF::TextureCount);

    tpf
        .def("find_texture", [](const TPF& t, const std::string& stem) -> const TPFTexture* {
            return t.FindTexture(stem);
        }, py::return_value_policy::reference_internal, py::arg("stem"));

    tpf
        .def("__len__", &TPF::TextureCount)
        .def("__repr__", [](const TPF& t) {
            return "<TPF " + std::to_string(t.Textures().size()) + " textures>";
        });
}

