
add_library(FirelinkERHook SHARED)

target_sources(FirelinkERHook
    PRIVATE
    src/ERHook.cpp
)
target_sources(FirelinkERHook
    PUBLIC
    FILE_SET HEADERS
    BASE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    FILES
        include/FirelinkERHook/ERHook.h
        include/FirelinkERHook/EROffsets.h
)

generate_firelink_export_header(FirelinkERHook FIRELINK_ER_HOOK_API)

target_link_libraries(FirelinkERHook
    PUBLIC FirelinkCore
)

install(TARGETS FirelinkERHook
        EXPORT FirelinkERTargets
        FILE_SET HEADERS
)
