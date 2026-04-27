pybind11_add_module(pyrelink_core
    pyrelink_core.cpp
    pyrelink_core_binder.cpp
    pyrelink_core_dcx.cpp
    pyrelink_core_dds.cpp
    pyrelink_core_tpf.cpp
)
target_include_directories(pyrelink_core PUBLIC
    "${PROJECT_SOURCE_DIR}/python/pybind/include"
)
target_link_libraries(pyrelink_core PRIVATE FirelinkCore)
configure_install_pybind_module(pyrelink_core "core")
