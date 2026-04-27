#pragma once

#include <pybind11/pybind11.h>

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
    return py::bytes(reinterpret_cast<const char*>(v.data()), v.size());
}

/// @brief Convert a Python path-like (str, Path, os.PathLike) to std::filesystem::path.
/// Must be called with the GIL held.
inline std::filesystem::path to_fs_path(const py::object& obj)
{
    // py::str handles both str and os.PathLike (including pathlib.Path) via __fspath__
    return std::filesystem::path(py::str(obj).cast<std::string>());
}

/// @brief Template function for binding C++ classes `T` that inherit from `GameFile<T>`.
template <typename T>
void bind_game_file(py::class_<T>& cls)
{
    cls
        .def_static("from_path", [](const py::object& path) {
            auto fs_path = to_fs_path(path);
            py::gil_scoped_release release;
            return T::FromPath(fs_path);
        })
        .def_static("from_paths_parallel", [](const py::object& paths, int max_threads = 0) {
            // With GIL held: convert `paths` (sequence of path-likes) to a vector of paths.
            const py::sequence seq(paths);
            if (seq.empty()) return std::vector<std::unique_ptr<T>>{};
            std::vector<std::filesystem::path> fs_paths;
            fs_paths.reserve(seq.size());
            for (const auto& item : seq)
            {
                fs_paths.push_back(to_fs_path(item));
            }
            // GIL released: parse in parallel.
            py::gil_scoped_release release;
            return T::FromPathsParallel(fs_paths, max_threads);
        })
        .def_static("from_bytes", [](const py::buffer& buf) {
            auto [ptr, size] = borrow_buffer(buf);
            py::gil_scoped_release release;
            return T::FromBytes(ptr, size);
        })
        .def_static("from_bytes_parallel", [](const py::list& buffers, int max_threads = 0) {
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
        })
        .def("to_bytes", [](const T& obj) {
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
