// pyrelink_core_binder.cpp — pybind11 bindings for FirelinkCore Binder module.

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <FirelinkCore/Binder.h>
#include <pyrelink_helpers.h>

namespace py = pybind11;
using namespace Firelink;

void bind_firelink_core_binder(py::module& m)
{
    py::enum_<BinderVersion>(m, "BinderVersion",
        "Binder archive version.")
        .value("V3", BinderVersion::V3)
        .value("V4", BinderVersion::V4)
        .export_values();

    py::class_<BinderFlags>(m, "BinderFlags",
        "Bit flags for a Binder archive header.")
        .def(py::init<>())
        .def_readwrite("value", &BinderFlags::value)
        .def_property_readonly("is_big_endian", &BinderFlags::is_big_endian)
        .def_property_readonly("has_ids", &BinderFlags::has_ids)
        .def_property_readonly("has_names", &BinderFlags::has_names)
        .def_property_readonly("has_long_offsets", &BinderFlags::has_long_offsets)
        .def_property_readonly("has_compression", &BinderFlags::has_compression)
        .def_property_readonly("entry_header_size", &BinderFlags::entry_header_size);

    py::class_<BinderVersion4Info>(m, "BinderVersion4Info",
        "Extra info for BND4 binders.")
        .def(py::init<>())
        .def_readwrite("unknown1", &BinderVersion4Info::unknown1)
        .def_readwrite("unknown2", &BinderVersion4Info::unknown2)
        .def_readwrite("unicode", &BinderVersion4Info::unicode)
        .def_readwrite("hash_table_type", &BinderVersion4Info::hash_table_type);

    py::class_<BinderEntry>(m, "BinderEntry",
        "A single entry in a Binder archive.")
        .def(py::init<>())
        .def_readwrite("entry_id", &BinderEntry::entry_id)
        .def_readwrite("path", &BinderEntry::path)
        .def_readwrite("flags", &BinderEntry::flags)
        .def_property("data",
            [](const BinderEntry& e) {
                return py::bytes(reinterpret_cast<const char*>(e.data.data()), e.data.size());
            },
            [](BinderEntry& e, const py::buffer& buf) {
                auto info = buf.request();
                auto ptr = static_cast<const std::byte*>(info.ptr);
                auto sz = static_cast<std::size_t>(info.size * info.itemsize);
                e.data.assign(ptr, ptr + sz);
            },
            "Entry payload as bytes.")
        .def_property_readonly("name", &BinderEntry::name,
            "Basename of the entry path.")
        .def_property_readonly("stem", &BinderEntry::stem,
            "Minimal stem (before first '.') of the entry path basename.")
        .def("__repr__", [](const BinderEntry& e) {
            return "<BinderEntry id=" + std::to_string(e.entry_id) +
                   " path='" + e.path + "' " + std::to_string(e.data.size()) + " bytes>";
        });

    py::register_exception<BinderError>(m, "BinderError", PyExc_RuntimeError);

    auto binder = py::class_<Binder>(m, "Binder",
        "A FromSoftware BND3/BND4 multi-file archive.");

    bind_game_file(binder);

    binder
        .def_static("from_split_bytes", [](const py::buffer& bhd_buf, const py::buffer& bdt_buf) {
            auto [bhd_ptr, bhd_size] = borrow_buffer(bhd_buf);
            auto [bdt_ptr, bdt_size] = borrow_buffer(bdt_buf);
            py::gil_scoped_release release;
            return Binder::FromSplitBytes(bhd_ptr, bhd_size, bdt_ptr, bdt_size);
        },
        py::arg("bhd_data"), py::arg("bdt_data"),
        "Parse a split BHF/BDT binder from raw bytes.");

    binder
        .def_property("version", &Binder::GetVersion, &Binder::SetVersion)
        .def_property("signature", &Binder::GetSignature, &Binder::SetSignature)
        .def_property("flags", &Binder::GetFlags, &Binder::SetFlags)
        .def_property("big_endian", &Binder::GetBigEndian, &Binder::SetBigEndian)
        .def_property("bit_big_endian", &Binder::GetBitBigEndian, &Binder::SetBitBigEndian)
        .def_property_readonly("entries",
            [](Binder& b) -> std::vector<BinderEntry>& {
                return b.Entries();
            },
            py::return_value_policy::reference_internal,
            "List of binder entries (mutable).")
        .def_property_readonly("entry_count", &Binder::EntryCount);

    binder
        .def("find_entry_by_id", [](const Binder& b, std::int32_t id) -> const BinderEntry* {
            return b.FindEntryByID(id);
        }, py::return_value_policy::reference_internal, py::arg("entry_id"))
        .def("find_entry_by_name", [](const Binder& b, const std::string& name) -> const BinderEntry* {
            return b.FindEntryByName(name);
        }, py::return_value_policy::reference_internal, py::arg("name"));
        // TODO: Bind other find methods.

    binder
        .def("__len__", &Binder::EntryCount)
        .def("__repr__", [](const Binder& b) {
            return "<Binder " + std::string(b.GetVersion() == BinderVersion::V3 ? "V3" : "V4")
                + " entries=" + std::to_string(b.Entries().size()) + ">";
        });
}
