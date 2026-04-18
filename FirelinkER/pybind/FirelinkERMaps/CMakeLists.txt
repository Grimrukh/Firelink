
pybind11_add_module(pyrelink_er_maps
        pyrelink_er_maps.cpp)
target_link_libraries(pyrelink_er_maps PRIVATE FirelinkERMaps)
configure_install_pybind_module(pyrelink_er_maps "eldenring/maps")
