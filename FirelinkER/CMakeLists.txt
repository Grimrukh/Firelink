add_subdirectory(src)

# Interface target for all FirelinkER component targets.
add_library(FirelinkER INTERFACE)
target_link_libraries(FirelinkER INTERFACE
    FirelinkERHook
    FirelinkERMaps
)
install(TARGETS FirelinkER EXPORT FirelinkERTargets)

if(FIRELINK_BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(FIRELINK_BUILD_PYBIND)
    add_subdirectory(pybind)
endif()

install(EXPORT FirelinkERTargets
        FILE FirelinkERTargets.cmake
        NAMESPACE FirelinkER::
        DESTINATION lib/cmake/FirelinkER
)
