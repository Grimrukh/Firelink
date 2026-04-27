// pyrelink_core.cpp — pybind11 bindings for FirelinkCore modules.

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <FirelinkCore/GameType.h>
#include <FirelinkCore/Collections.h>

namespace py = pybind11;
using namespace Firelink;

void bind_firelink_core_binder(py::module& m);
void bind_firelink_core_dcx(py::module& m);
void bind_firelink_core_dds(py::module& m);
void bind_firelink_core_tpf(py::module& m);

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

    py::class_<Vector2>(m, "Vector2", "Basic XY vector.")
        .def(py::init<float, float>(), py::arg("x") = 0.f, py::arg("y") = 0.f)
        .def(py::init([](const py::sequence& s) {
            if (s.size() != 2) throw py::value_error("Vector2 requires exactly 2 elements");
            return Vector2{s[0].cast<float>(), s[1].cast<float>()};
        }), py::arg("seq"))
        .def_readwrite("x", &Vector2::x)
        .def_readwrite("y", &Vector2::y)
        .def("__repr__", [](const Vector2& v) {
            return "Vector2(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ")";
        })
        .def("__len__", [](const Vector2&) { return 2; })
        .def("__getitem__", [](const Vector2& v, int i) -> float {
            if (i == 0) return v.x;
            if (i == 1) return v.y;
            throw py::index_error("Vector2 index out of range");
        })
        .def("__setitem__", [](Vector2& v, int i, float val) {
            if (i == 0) v.x = val;
            else if (i == 1) v.y = val;
            else throw py::index_error("Vector2 index out of range");
        })
        .def("__iter__", [](const Vector2& v) {
            return py::iter(py::make_tuple(v.x, v.y));
        });

    py::class_<Vector3>(m, "Vector3", "Basic XYZ vector.")
        .def(py::init<float, float, float>(), py::arg("x") = 0.f, py::arg("y") = 0.f, py::arg("z") = 0.f)
        .def(py::init([](const py::sequence& s) {
            if (s.size() != 3) throw py::value_error("Vector3 requires exactly 3 elements");
            return Vector3{s[0].cast<float>(), s[1].cast<float>(), s[2].cast<float>()};
        }), py::arg("seq"))
        .def_readwrite("x", &Vector3::x)
        .def_readwrite("y", &Vector3::y)
        .def_readwrite("z", &Vector3::z)
        .def("__repr__", [](const Vector3& v) {
            return "Vector3(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ", " + std::to_string(v.z) + ")";
        })
        .def("__len__", [](const Vector3&) { return 3; })
        .def("__getitem__", [](const Vector3& v, int i) -> float {
            if (i == 0) return v.x;
            if (i == 1) return v.y;
            if (i == 2) return v.z;
            throw py::index_error("Vector3 index out of range");
        })
        .def("__setitem__", [](Vector3& v, int i, float val) {
            if (i == 0) v.x = val;
            else if (i == 1) v.y = val;
            else if (i == 2) v.z = val;
            else throw py::index_error("Vector3 index out of range");
        })
        .def("__iter__", [](const Vector3& v) {
            return py::iter(py::make_tuple(v.x, v.y, v.z));
        })
        .def_static("zero", &Vector3::Zero)
        .def_static("one", &Vector3::One);

    py::class_<Vector4>(m, "Vector4", "Basic XYZW vector.")
        .def(py::init<float, float, float, float>(), py::arg("x") = 0.f, py::arg("y") = 0.f, py::arg("z") = 0.f, py::arg("w") = 0.f)
        .def(py::init([](const py::sequence& s) {
            if (s.size() != 4) throw py::value_error("Vector4 requires exactly 4 elements");
            return Vector4{s[0].cast<float>(), s[1].cast<float>(), s[2].cast<float>(), s[3].cast<float>()};
        }), py::arg("seq"))
        .def_readwrite("x", &Vector4::x)
        .def_readwrite("y", &Vector4::y)
        .def_readwrite("z", &Vector4::z)
        .def_readwrite("w", &Vector4::w)
        .def("__repr__", [](const Vector4& v) {
            return "Vector4(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ", " + std::to_string(v.z) + ", " + std::to_string(v.w) + ")";
        })
        .def("__len__", [](const Vector4&) { return 4; })
        .def("__getitem__", [](const Vector4& v, int i) -> float {
            if (i == 0) return v.x;
            if (i == 1) return v.y;
            if (i == 2) return v.z;
            if (i == 3) return v.w;
            throw py::index_error("Vector4 index out of range");
        })
        .def("__setitem__", [](Vector4& v, int i, float val) {
            if (i == 0) v.x = val;
            else if (i == 1) v.y = val;
            else if (i == 2) v.z = val;
            else if (i == 3) v.w = val;
            else throw py::index_error("Vector4 index out of range");
        })
        .def("__iter__", [](const Vector4& v) {
            return py::iter(py::make_tuple(v.x, v.y, v.z, v.w));
        });

    py::class_<EulerRad>(m, "EulerRad", "Euler XYZ angles in radians.")
        .def(py::init<float, float, float>(), py::arg("x") = 0.f, py::arg("y") = 0.f, py::arg("z") = 0.f)
        .def(py::init([](const py::sequence& s) {
            if (s.size() != 3) throw py::value_error("EulerRad requires exactly 3 elements");
            return EulerRad{s[0].cast<float>(), s[1].cast<float>(), s[2].cast<float>()};
        }), py::arg("seq"))
        .def_readwrite("x", &EulerRad::x)
        .def_readwrite("y", &EulerRad::y)
        .def_readwrite("z", &EulerRad::z)
        .def("__repr__", [](const EulerRad& v) {
            return "EulerRad(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ", " + std::to_string(v.z) + ")";
        })
        .def("__len__", [](const EulerRad&) { return 3; })
        .def("__getitem__", [](const EulerRad& v, int i) -> float {
            if (i == 0) return v.x;
            if (i == 1) return v.y;
            if (i == 2) return v.z;
            throw py::index_error("EulerRad index out of range");
        })
        .def("__setitem__", [](EulerRad& v, int i, float val) {
            if (i == 0) v.x = val;
            else if (i == 1) v.y = val;
            else if (i == 2) v.z = val;
            else throw py::index_error("EulerRad index out of range");
        })
        .def("__iter__", [](const EulerRad& v) {
            return py::iter(py::make_tuple(v.x, v.y, v.z));
        })
        .def_static("zero", &EulerRad::Zero);

    py::class_<ColorRGBA>(m, "Color4b", "Color RGBA as bytes [0-255].")
        .def(py::init<>())
        .def(py::init<std::uint8_t, std::uint8_t, std::uint8_t, std::uint8_t>(),
            py::arg("r") = 0,
            py::arg("g") = 0,
            py::arg("b") = 0,
            py::arg("a") = 255)
        .def(py::init([](const py::sequence& s) {
            if (s.size() != 4) throw py::value_error("ColorRGBA requires exactly 4 elements");
            return ColorRGBA{
                s[0].cast<std::uint8_t>(),
                s[1].cast<std::uint8_t>(),
                s[2].cast<std::uint8_t>(),
                s[3].cast<std::uint8_t>(),
            };
            }), py::arg("seq"))
        .def_readwrite("r", &ColorRGBA::r)
        .def_readwrite("g", &ColorRGBA::g)
        .def_readwrite("b", &ColorRGBA::b)
        .def_readwrite("a", &ColorRGBA::a)
        .def("__getitem__", [](const ColorRGBA& v, int i) -> float {
            if (i == 0) return v.r;
            if (i == 1) return v.g;
            if (i == 2) return v.b;
            if (i == 3) return v.a;
            throw py::index_error("ColorRGBA index out of range");
        })
        .def("__setitem__", [](ColorRGBA& v, int i, float val) {
            if (i == 0) v.r = val;
            else if (i == 1) v.g = val;
            else if (i == 2) v.b = val;
            else if (i == 3) v.a = val;
            else throw py::index_error("ColorRGBA index out of range");
        })
        .def("__iter__", [](const ColorRGBA& c) {
                return py::iter(py::make_tuple(c.r, c.g, c.b, c.a));
            });

    py::class_<AABB>(m, "AABB", "AABB XYZ bounding box")
        .def(py::init<>())
        .def_readwrite("min", &AABB::min)
        .def_readwrite("max", &AABB::max);
}

PYBIND11_MODULE(_bindings, m)
{
    m.doc() = "Python bindings for FirelinkCore.";

    bind_firelink_core(m);
    bind_firelink_core_binder(m);
    bind_firelink_core_dcx(m);
    bind_firelink_core_dds(m);
    bind_firelink_core_tpf(m);
}
