add_subdirectory(src)

if(FIRELINK_BUILD_PYBIND)
    add_subdirectory(pybind)
endif()
