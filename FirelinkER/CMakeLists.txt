add_subdirectory(src)

# Interface target for all FirelinkER component targets.
add_library(FirelinkER INTERFACE)
target_link_libraries(FirelinkER INTERFACE
    FirelinkERHook
    FirelinkERMaps
)
install(TARGETS FirelinkER
        EXPORT FirelinkTargets
)

if(FIRELINK_BUILD_PYBIND)
    add_subdirectory(pybind)
endif()
