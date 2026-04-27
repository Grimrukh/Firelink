// pyrelink_core_dcx.cpp — pybind11 bindings for FirelinkCore DCX module.
//
// Exposes DCX compression/decompression with zero-copy where possible
// (input bytes are borrowed from Python buffers).

#include <pybind11/pybind11.h>

#include <FirelinkCore/DCX.h>
#include <FirelinkCore/Oodle.h>
#include <pyrelink_helpers.h>

namespace py = pybind11;
using namespace Firelink;

void bind_firelink_core_dcx(py::module& m)
{
    py::enum_<DCXType>(m, "DCXType",
        "DCX compression type identifier.")
        .value("Unknown",                  DCXType::Unknown)
        .value("Null",                     DCXType::Null)
        .value("Zlib",                     DCXType::Zlib)
        .value("DCP_EDGE",                 DCXType::DCP_EDGE)
        .value("DCP_DFLT",                 DCXType::DCP_DFLT)
        .value("DCX_EDGE",                 DCXType::DCX_EDGE)
        .value("DCX_DFLT_10000_24_9",      DCXType::DCX_DFLT_10000_24_9)
        .value("DCX_DFLT_10000_44_9",      DCXType::DCX_DFLT_10000_44_9)
        .value("DCX_DFLT_11000_44_8",      DCXType::DCX_DFLT_11000_44_8)
        .value("DCX_DFLT_11000_44_9",      DCXType::DCX_DFLT_11000_44_9)
        .value("DCX_DFLT_11000_44_9_15",   DCXType::DCX_DFLT_11000_44_9_15)
        .value("DCX_KRAK",                 DCXType::DCX_KRAK)
        .value("DCX_ZSTD",                 DCXType::DCX_ZSTD)
        .export_values();

    // Register DCXError as a Python exception inheriting from RuntimeError.
    py::register_exception<DCXError>(m, "DCXError", PyExc_RuntimeError);

    m.def("is_dcx", [](const py::buffer& buf)
    {
        auto [ptr, size] = borrow_buffer(buf);
        return IsDCX(ptr, size);
    },
    py::arg("data"),
    "Return True if data appears to be DCX/DCP compressed (or raw zlib).");

    m.def("detect_dcx", [](const py::buffer& buf)
    {
        auto [ptr, size] = borrow_buffer(buf);
        return DetectDCX(ptr, size);
    },
    py::arg("data"),
    "Detect the DCXType from the header without decompressing.");

    m.def("decompress_dcx", [](const py::buffer& buf)
    {
        auto [ptr, size] = borrow_buffer(buf);
        py::gil_scoped_release release;
        return DecompressDCX(ptr, size);
    },
    py::arg("data"),
    "Decompress DCX/DCP/raw-zlib data.\n\n"
    "Returns a DCXResult with .data (bytes) and .type (DCXType).");

    m.def("compress_dcx", [](const py::buffer& buf, DCXType type)
    {
        auto [ptr, size] = borrow_buffer(buf);
        std::vector<std::byte> result;
        {
            py::gil_scoped_release release;
            result = CompressDCX(ptr, size, type);
        }
        return vector_to_bytes(result);
    },
    py::arg("data"), py::arg("dcx_type"),
    "Compress data into the given DCX format.\n\n"
    "Returns the compressed bytes.");

    // Bind DCXResult so Python can access .data and .type
    py::class_<DCXResult>(m, "DCXResult",
        "Result of DCX decompression: decompressed data and detected type.")
        .def_property_readonly("data", [](const DCXResult& r)
        {
            return py::bytes(reinterpret_cast<const char*>(r.data.data()), r.data.size());
        }, "Decompressed payload as bytes.")
        .def_readonly("type", &DCXResult::type, "Detected DCX variant.");

    // ========================================================================
    // Oodle DLL configuration
    // ========================================================================

    m.def("set_oodle_dll_path", &Oodle::SetDLLSearchPath,
        py::arg("directory"),
        "Set the directory to search for oo2core_6_win64.dll before default paths.\n\n"
        "Must be called before the first DCX_KRAK compress/decompress call to take effect.\n"
        "Alternatively, use load_oodle_dll() to load immediately.");

    m.def("load_oodle_dll", &Oodle::LoadDLL,
        py::arg("directory"),
        "Explicitly load oo2core_6_win64.dll from the given directory.\n\n"
        "Returns True on success. Can be called at any time.");

    m.def("is_oodle_available", &Oodle::IsAvailable,
        "Return True if the Oodle DLL has been loaded and is ready for use.");

    m.def("oodle_dll_path", &Oodle::LoadedPath,
        "Return the path the Oodle DLL was loaded from, or empty string if not loaded.");
}
