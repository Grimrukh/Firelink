// pyrelink_core_binder.cpp — pybind11 bindings for FirelinkCore Binder module.

#include <pybind11/pybind11.h>
#include <pybind11/functional.h>
#include <pybind11/stl.h>

#include <FirelinkCore/Binder.h>
#include <pyrelink_helpers.h>

#include <regex>

namespace py = pybind11;
using namespace Firelink;

/// Convert a Python `str` or `re.Pattern` to a `std::string` ready for `std::regex`.
/// Raises `TypeError` if the argument is neither.
static std::string py_to_regex(const py::object& pat)
{
    if (py::isinstance<py::str>(pat))
        return pat.cast<std::string>();

    // re.Pattern objects expose their source via the `.pattern` attribute.
    if (py::hasattr(pat, "pattern"))
        return pat.attr("pattern").cast<std::string>();

    throw py::type_error("Expected a str or re.Pattern, got " +
                         py::cast<std::string>(py::str(py::type::handle_of(pat))));
}

void bind_firelink_core_binder(py::module& m)
{
    py::enum_<BinderVersion>(m, "BinderVersion",
        "Binder archive version.")
        .value("V3", BinderVersion::V3)
        .value("V4", BinderVersion::V4);

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

    py::class_<BinderEntry, std::shared_ptr<BinderEntry>>(m, "BinderEntry",
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
        .def("get_uncompressed_data",
            [](const BinderEntry& e) {
                const auto bytes = e.GetUncompressedData();
                return py::bytes(reinterpret_cast<const char*>(bytes.data()), bytes.size());
            },
            "Return the entry payload as bytes, decompressing with zlib if the compression flag is set.")
        .def("__repr__", [](const BinderEntry& e) {
            return "<BinderEntry id=" + std::to_string(e.entry_id) +
                   " path='" + e.path + "' " + std::to_string(e.data.size()) + " bytes>";
        });

    py::register_exception<BinderError>(m, "BinderError", PyExc_RuntimeError);
    py::register_exception<BinderEntryNotFoundError>(m, "BinderEntryNotFoundError", PyExc_KeyError);
    py::register_exception<MultipleBinderEntriesFoundError>(m, "MultipleBinderEntriesFoundError", PyExc_LookupError);

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
            [](const Binder& b) { return b.Entries(); },
            "List of binder entries (each a shared BinderEntry).")
        .def_property_readonly("entry_count", &Binder::EntryCount);

    binder
        .def("find_entry_by_id", &Binder::FindEntryByID, py::arg("entry_id"),
            "Find entry by ID. Raises BinderEntryNotFoundError if not found.")
        .def("find_entry_by_name", &Binder::FindEntryByName, py::arg("name"),
            "Find entry by basename. Raises BinderEntryNotFoundError if not found.")
        .def("find_entry_by_name_regex",
            [](const Binder& b, const py::object& pat, bool full_match) {
                return b.FindEntryByNameRegex(py_to_regex(pat), full_match);
            },
            py::arg("pattern"), py::arg("full_match") = false,
            "Find the single entry whose name matches *pattern* (str or re.Pattern).\n"
            "Raises BinderEntryNotFoundError if none match, MultipleBinderEntriesFoundError if several do.")
        .def("find_entries_by_name_regex",
            [](const Binder& b, const py::object& pat, bool full_match) {
                return b.FindEntriesByNameRegex(py_to_regex(pat), full_match);
            },
            py::arg("pattern"), py::arg("full_match") = false,
            "Return all entries whose names match *pattern* (str or re.Pattern).")
        .def("find_entry_by_filter",
            [](const Binder& b, const py::function& fn) {
                return b.FindEntryByFilter([&fn](const BinderEntry& e) -> bool {
                    return fn(e).cast<bool>();
                });
            },
            py::arg("filter"),
            "Find the single entry for which *filter(entry)* returns True.\n"
            "Raises BinderEntryNotFoundError if none match, MultipleBinderEntriesFoundError if several do.")
        .def("find_entries_by_filter",
            [](const Binder& b, const py::function& fn) {
                return b.FindEntriesByFilter([&fn](const BinderEntry& e) -> bool {
                    return fn(e).cast<bool>();
                });
            },
            py::arg("filter"),
            "Return all entries for which *filter(entry)* returns True.");

    binder
        .def("__len__", &Binder::EntryCount)
        .def("__repr__", [](const Binder& b) {
            return "<Binder " + std::string(b.GetVersion() == BinderVersion::V3 ? "V3" : "V4")
                + " entries=" + std::to_string(b.Entries().size()) + ">";
        });
}
