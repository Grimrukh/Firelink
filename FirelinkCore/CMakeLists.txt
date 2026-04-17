add_subdirectory(src)

if(FIRELINK_BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(FIRELINK_BUILD_PYBIND)
    add_subdirectory(pybind)
endif()

install(EXPORT FirelinkCoreTargets
        FILE FirelinkCoreTargets.cmake
        NAMESPACE FirelinkCore::
        DESTINATION lib/cmake/FirelinkCore
)
