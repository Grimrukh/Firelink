// pyrelink_core_dds.cpp — pybind11 bindings for FirelinkCore DDS module.
//
// Exposes DDS texture conversion to Python with zero-copy where possible
// (input bytes are borrowed from Python buffers).

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <FirelinkCore/DDS.h>

namespace py = pybind11;
using namespace Firelink;

void bind_firelink_core_dds(py::module& m)
{

    // ========================================================================
    // DDS
    // ========================================================================

    // Expose DXGI_FORMAT as a Python enum so users can pass format constants.
    // We only expose the formats commonly used in FromSoftware textures.
    py::enum_<DXGI_FORMAT>(m, "DXGIFormat",
        "DXGI pixel format identifiers (subset relevant to FromSoftware textures).")
        .value("UNKNOWN",              DXGI_FORMAT_UNKNOWN)
        // Uncompressed
        .value("R8G8B8A8_UNORM",       DXGI_FORMAT_R8G8B8A8_UNORM)
        .value("R8G8B8A8_UNORM_SRGB",  DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)
        .value("B8G8R8A8_UNORM",       DXGI_FORMAT_B8G8R8A8_UNORM)
        .value("B8G8R8A8_UNORM_SRGB",  DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)
        // BC compressed
        .value("BC1_UNORM",            DXGI_FORMAT_BC1_UNORM)
        .value("BC1_UNORM_SRGB",       DXGI_FORMAT_BC1_UNORM_SRGB)
        .value("BC2_UNORM",            DXGI_FORMAT_BC2_UNORM)
        .value("BC2_UNORM_SRGB",       DXGI_FORMAT_BC2_UNORM_SRGB)
        .value("BC3_UNORM",            DXGI_FORMAT_BC3_UNORM)
        .value("BC3_UNORM_SRGB",       DXGI_FORMAT_BC3_UNORM_SRGB)
        .value("BC4_UNORM",            DXGI_FORMAT_BC4_UNORM)
        .value("BC4_SNORM",            DXGI_FORMAT_BC4_SNORM)
        .value("BC5_UNORM",            DXGI_FORMAT_BC5_UNORM)
        .value("BC5_SNORM",            DXGI_FORMAT_BC5_SNORM)
        .value("BC6H_UF16",            DXGI_FORMAT_BC6H_UF16)
        .value("BC6H_SF16",            DXGI_FORMAT_BC6H_SF16)
        .value("BC7_UNORM",            DXGI_FORMAT_BC7_UNORM)
        .value("BC7_UNORM_SRGB",       DXGI_FORMAT_BC7_UNORM_SRGB)
        // Single/dual channel
        .value("R8_UNORM",             DXGI_FORMAT_R8_UNORM)
        .value("R8G8_UNORM",           DXGI_FORMAT_R8G8_UNORM)
        .value("R16_FLOAT",            DXGI_FORMAT_R16_FLOAT)
        .value("R16G16_FLOAT",         DXGI_FORMAT_R16G16_FLOAT)
        .value("R16G16B16A16_FLOAT",   DXGI_FORMAT_R16G16B16A16_FLOAT)
        .value("R32_FLOAT",            DXGI_FORMAT_R32_FLOAT)
        .value("R32G32B32A32_FLOAT",   DXGI_FORMAT_R32G32B32A32_FLOAT)
        .export_values();

    // --- DDS → Image ---

    m.def("convert_dds_to_tga", [](const py::buffer& buf)
    {
        auto [ptr, size] = borrow_buffer(buf);
        std::vector<std::byte> result;
        {
            py::gil_scoped_release release;
            result = ConvertDDSToTGA(ptr, size);
        }
        return vector_to_bytes(result);
    },
    py::arg("data"),
    "Convert DDS texture bytes to TGA format.\n\n"
    "Block-compressed formats (BC1-BC7) are automatically decompressed.");

    m.def("convert_dds_to_png", [](const py::buffer& buf)
    {
        auto [ptr, size] = borrow_buffer(buf);
        std::vector<std::byte> result;
        {
            py::gil_scoped_release release;
            result = ConvertDDSToPNG(ptr, size);
        }
        return vector_to_bytes(result);
    },
    py::arg("data"),
    "Convert DDS texture bytes to PNG format.\n\n"
    "Block-compressed formats (BC1-BC7) are automatically decompressed.");

    // --- Image → DDS ---

    m.def("convert_tga_to_dds", [](const py::buffer& buf, DXGI_FORMAT format)
    {
        auto [ptr, size] = borrow_buffer(buf);
        std::vector<std::byte> result;
        {
            py::gil_scoped_release release;
            result = ConvertTGAToDDS(ptr, size, format);
        }
        return vector_to_bytes(result);
    },
    py::arg("data"), py::arg("format"),
    "Convert TGA image bytes to DDS with the specified DXGI pixel format.\n\n"
    "BC6H/BC7 compression uses GPU acceleration when available.");

    m.def("convert_png_to_dds", [](const py::buffer& buf, DXGI_FORMAT format)
    {
        auto [ptr, size] = borrow_buffer(buf);
        std::vector<std::byte> result;
        {
            py::gil_scoped_release release;
            result = ConvertPNGToDDS(ptr, size, format);
        }
        return vector_to_bytes(result);
    },
    py::arg("data"), py::arg("format"),
    "Convert PNG image bytes to DDS with the specified DXGI pixel format.\n\n"
    "BC6H/BC7 compression uses GPU acceleration when available.");
}