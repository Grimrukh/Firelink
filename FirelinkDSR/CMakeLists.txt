add_subdirectory(src)

# Interface target for all FirelinkDSR component targets.
add_library(FirelinkDSR INTERFACE)
target_link_libraries(FirelinkDSR INTERFACE
    FirelinkDSRHook
)
install(TARGETS FirelinkDSR
        EXPORT FirelinkTargets
)

# TODO: FirelinkDSR has no tests or Python bindings yet.

#if(FIRELINK_BUILD_TESTS)
#    add_subdirectory(tests)
#endif()

#if(FIRELINK_BUILD_PYBIND)
#    add_subdirectory(pybind)
#endif()
