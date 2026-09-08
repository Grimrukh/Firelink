#pragma once

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>

#include <filesystem>
#include <string>
#include <vector>

namespace py = pybind11;

/// Borrow a pointer + size from a Python bytes-like object (bytes, bytearray,
/// memoryview, numpy array, etc.) without copying.
inline std::pair<const std::byte*, std::size_t> borrow_buffer(const py::buffer& buf)
{
    const py::buffer_info info = buf.request();
    return {
        static_cast<const std::byte*>(info.ptr),
        static_cast<std::size_t>(info.size * info.itemsize)
    };
}

/// Convert a std::vector<std::byte> to a Python `bytes` object.
inline py::bytes vector_to_bytes(const std::vector<std::byte>& v)
{
    return {reinterpret_cast<const char*>(v.data()), v.size()};
}

/// @brief Template function for binding C++ classes `T` that inherit from `GameFile<T>`.
template <typename T>
void bind_game_file(py::class_<T>& cls)
{
    cls
        .def(py::init<>())  // default constructible
        .def_static(
            "from_path",
            [](const std::filesystem::path& path)
            {
                py::gil_scoped_release release;
                return T::FromPath(path);
            })
        .def_static(
            "from_paths_parallel",
            [](const std::vector<std::filesystem::path>& paths, int max_threads = 0)
            {
                py::gil_scoped_release release;
                return T::FromPathsParallel(paths, max_threads);
            },
            py::arg("paths"),
            py::arg("max_threads") = 0)
        .def_static(
            "from_bytes",
            [](const py::buffer& buffer)
            {
                auto [ptr, size] = borrow_buffer(buffer);
                py::gil_scoped_release release;
                return T::FromBytes(ptr, size);
            },
            py::arg("buffer"))
        .def_static(
            "from_bytes_parallel",
            [](const py::list& buffers, int max_threads = 0)
            {
                // With GIL held: copy each buffer into a C++ vector.
                std::vector<std::vector<std::byte>> data;
                data.reserve(buffers.size());
                for (const auto& item : buffers)
                {
                    auto [ptr, size] = borrow_buffer(py::cast<py::buffer>(item));
                    data.emplace_back(ptr, ptr + size);
                }
                // GIL released: parse in parallel.
                py::gil_scoped_release release;
                return T::FromBytesParallel(std::move(data), max_threads);
            },
            py::arg("buffers"),
            py::arg("max_threads") = 0)
        .def(
            "to_bytes",
            [](const T& obj)
            {
                std::vector<std::byte> result;
                {
                    py::gil_scoped_release release;
                    result = obj.ToBytes();
                }
                return vector_to_bytes(result);
            })
        .def(
            "__bytes__",  // allows `data = bytes(game_file)` in Python
            [](const T& obj)
            {
                std::vector<std::byte> result;
                {
                    py::gil_scoped_release release;
                    result = obj.ToBytes();
                }
                return vector_to_bytes(result);
            })
        .def("write_to_path", &T::WriteToPath)
        .def_property("dcx_type", &T::GetDCXType, &T::SetDCXType)
        .def_property("path", &T::GetPath, &T::SetPath)
        .def_property_readonly("path_name", &T::GetPathName)
        .def_property_readonly("path_stem", &T::GetPathMinimalStem);
}
