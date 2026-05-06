
add_library(FirelinkDSRHook)

target_sources(FirelinkDSRHook
    PRIVATE
        src/DSRHook.cpp
        src/DSRPlayer.cpp
)
target_sources(FirelinkDSRHook
    PUBLIC
    FILE_SET HEADERS
    BASE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    FILES
        include/FirelinkDSRHook/DSREnums.h
        include/FirelinkDSRHook/DSRHook.h
        include/FirelinkDSRHook/DSROffsets.h
        include/FirelinkDSRHook/DSRPlayer.h
)

generate_firelink_export_header(FirelinkDSRHook FIRELINK_DSR_HOOK_API)

target_link_libraries(FirelinkDSRHook
    PUBLIC FirelinkCore
)

install(TARGETS FirelinkDSRHook
        EXPORT FirelinkTargets
        FILE_SET HEADERS
)
