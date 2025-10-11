
add_library(FirelinkERHook SHARED)

target_sources(FirelinkERHook
    PRIVATE
    ERHook.cpp
)
target_sources(FirelinkERHook
    PUBLIC
    FILE_SET HEADERS
    BASE_DIRS
        "${PROJECT_SOURCE_DIR}/src"
    FILES
        ERHook.h
        EROffsets.h
)

generate_firelink_export_header(FirelinkERHook FIRELINK_ER_HOOK_API)

target_link_libraries(FirelinkERHook
    PUBLIC FirelinkCore
)
