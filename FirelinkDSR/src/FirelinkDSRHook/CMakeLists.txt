
add_library(FirelinkDSRHook SHARED)

target_sources(FirelinkDSRHook
    PRIVATE
        DSRHook.cpp
        DSRPlayer.cpp
)
target_sources(FirelinkDSRHook
    PUBLIC
    FILE_SET HEADERS
        BASE_DIRS
            "${PROJECT_SOURCE_DIR}/src"
        FILES
            DSREnums.h
            DSRHook.h
            DSROffsets.h
            DSRPlayer.h
)

generate_firelink_export_header(FirelinkDSRHook FIRELINK_DSR_HOOK_API)

target_link_libraries(FirelinkDSRHook
    PUBLIC FirelinkCore
)
