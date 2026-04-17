add_subdirectory(FirelinkCore)
add_subdirectory(FirelinkFLVER)

install(EXPORT FirelinkCoreTargets
        FILE FirelinkCoreTargets.cmake
        NAMESPACE FirelinkCore::
        DESTINATION lib/cmake/FirelinkCore
)