// pyrelink_core_tpf.cpp — pybind11 bindings for FirelinkCore TPF module.

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <FirelinkCore/TPF.h>

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

    py::class_<TPF>(m, "TPF",
        "A FromSoftware TPF texture pack file.")
        .def(py::init<>())
        .def_readwrite("platform", &TPF::platform)
        .def_readwrite("tpf_flags", &TPF::tpf_flags)
        .def_readwrite("encoding_type", &TPF::encoding_type)
        .def_readwrite("textures", &TPF::textures)
        .def_static("from_bytes", [](const py::buffer& buf) {
            auto [ptr, size] = borrow_buffer(buf);
            py::gil_scoped_release release;
            return TPF::FromBytes(ptr, size);
        },
        py::arg("data"),
        "Parse a TPF from raw bytes (must already be DCX-decompressed).")
        .def("to_bytes", [](const TPF& t) {
            std::vector<std::byte> result;
            {
                py::gil_scoped_release release;
                result = t.ToBytes();
            }
            return vector_to_bytes(result);
        },
        "Serialize the TPF back to bytes.")
        .def_property_readonly("texture_count", &TPF::TextureCount)
        .def("find_texture", [](const TPF& t, const std::string& stem) -> const TPFTexture* {
            return t.FindTexture(stem);
        }, py::return_value_policy::reference_internal, py::arg("stem"))
        .def("__len__", &TPF::TextureCount)
        .def("__repr__", [](const TPF& t) {
            return "<TPF " + std::to_string(t.textures.size()) + " textures>";
        });
}

