// pyrelink_core.cpp — pybind11 bindings for FirelinkCore modules.

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "FirelinkCore/GameType.h"

namespace py = pybind11;

// ============================================================================
// Helpers
// ============================================================================

/// Borrow a pointer + size from a Python bytes-like object (bytes, bytearray,
/// memoryview, numpy array, etc.) without copying.
static std::pair<const std::byte*, std::size_t> borrow_buffer(const py::buffer& buf)
{
    const py::buffer_info info = buf.request();
    return {
        static_cast<const std::byte*>(info.ptr),
        static_cast<std::size_t>(info.size * info.itemsize)
    };
}

/// Convert a std::vector<std::byte> to a Python `bytes` object.
static py::bytes vector_to_bytes(const std::vector<std::byte>& v)
{
    return py::bytes(reinterpret_cast<const char*>(v.data()), v.size());
}

// Amalgamated source
#include "pyrelink_core_binder.cpp"
#include "pyrelink_core_dcx.cpp"
#include "pyrelink_core_dds.cpp"
#include "pyrelink_core_tpf.cpp"

void bind_firelink_core(py::module& m)
{
    py::enum_<GameType>(m, "GameType",
        "Game type.")
        .value("DemonsSouls", GameType::DemonsSouls)
        .value("DarkSoulsPTDE", GameType::DarkSoulsPTDE)
        .value("DarkSoulsDSR", GameType::DarkSoulsDSR)
        .value("Bloodborne", GameType::Bloodborne)
        .value("DarkSouls3", GameType::DarkSouls3)
        .value("Sekiro", GameType::Sekiro)
        .value("EldenRing", GameType::EldenRing)
        .export_values();

    // TODO: Bind GameFile base class methods.
}

// ============================================================================
// Module definition
// ============================================================================

PYBIND11_MODULE(_bindings, m)
{
    m.doc() = "Python bindings for FirelinkCore (Binder, DCX, DDS, ImageImportManager, TPF).";

    bind_firelink_core_binder(m);
    bind_firelink_core_dcx(m);
    bind_firelink_core_dds(m);
    bind_firelink_core_tpf(m);
}
