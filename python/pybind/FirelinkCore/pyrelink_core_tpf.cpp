// pyrelink_core_tpf.cpp — pybind11 bindings for FirelinkCore TPF module.

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <FirelinkCore/DDS.h>
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
        .value("XboxOne", TPFPlatform::XboxOne);

    py::enum_<TextureType>(m, "TextureType",
        "TPF texture type.")
        .value("Texture", TextureType::Texture)
        .value("Cubemap", TextureType::Cubemap)
        .value("Volume", TextureType::Volume);

    py::class_<TPFTexture::ConsoleInfo>(m, "TPFConsoleInfo",
        "Extra metadata carried by console TPF textures, which store no DDS header.")
        .def(py::init<>())
        .def_readwrite("width", &TPFTexture::ConsoleInfo::width)
        .def_readwrite("height", &TPFTexture::ConsoleInfo::height)
        .def_readwrite("texture_count", &TPFTexture::ConsoleInfo::texture_count)
        .def_readwrite("unk1", &TPFTexture::ConsoleInfo::unk1)
        .def_readwrite("unk2", &TPFTexture::ConsoleInfo::unk2)
        .def_property("dxgi_format",
            [](const TPFTexture::ConsoleInfo& ci) { return static_cast<DXGI_FORMAT>(ci.dxgi_format); },
            [](TPFTexture::ConsoleInfo& ci, const DXGI_FORMAT f) { ci.dxgi_format = static_cast<std::int32_t>(f); },
            "DXGI format: read from the file on PS4/XboxOne, mapped from `format` elsewhere.")
        .def("__repr__", [](const TPFTexture::ConsoleInfo& ci) {
            return "<TPFConsoleInfo " + std::to_string(ci.width) + "x" + std::to_string(ci.height) + ">";
        });

    py::class_<TPFTexture>(m, "TPFTexture",
        "A single texture in a TPF archive.")
        .def(py::init<>())
        .def_readwrite("stem", &TPFTexture::stem)
        .def_readwrite("format", &TPFTexture::format)
        .def_readwrite("texture_type", &TPFTexture::texture_type)
        .def_readwrite("mipmap_count", &TPFTexture::mipmap_count)
        .def_readwrite("texture_flags", &TPFTexture::texture_flags)
        .def_readwrite("platform", &TPFTexture::platform,
            "Platform of the owning TPF, copied in when the TPF is read.")
        .def_readwrite("console_info", &TPFTexture::console_info,
            "Console metadata (dimensions, format), or None for PC textures.")
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
        .def_property_readonly("has_dds_header", &TPFTexture::HasDDSHeader,
            "True if `data` already starts with the 'DDS ' magic.\n\n"
            "Console TPFs (PS3, Xbox 360, and some PS4/XboxOne) store the bare mip chain\n"
            "instead, and need a header rebuilt before anything can decode them.")
        .def("to_dds", [](const TPFTexture& t)
        {
            DDS result;
            {
                py::gil_scoped_release release;
                result = t.ToDDS();
            }
            return result;
        },
        "Return this texture as a DDS.\n\n"
        "If the stored data is headerless, a DDS header is rebuilt from this texture's\n"
        "metadata (dimensions, format, mipmap count, texture type).  Pixel data is not\n"
        "deswizzled; call DDS.deswizzle_ps4() on the result for PS4 dumps.")
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

