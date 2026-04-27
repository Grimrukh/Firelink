pybind11_add_module(pyrelink_flver
    pyrelink_flver.cpp
    pyrelink_texture_finder.cpp
)
target_include_directories(pyrelink_flver PUBLIC
    "${PROJECT_SOURCE_DIR}/python/pybind/include"
)
target_link_libraries(pyrelink_flver PRIVATE FirelinkFLVER)
configure_install_pybind_module(pyrelink_flver "flver")
