
add_library(FirelinkCore SHARED)

target_sources(FirelinkCore
    PRIVATE
    dllmain.cpp
    pch.h
    pch.cpp
    BaseHook.cpp
    BinaryReadWrite.cpp
    Collections.cpp
    Logging.cpp
    Pointer.cpp
    Process.cpp
    MemoryUtils.cpp
)
target_sources(FirelinkCore
    PUBLIC
    FILE_SET HEADERS
    BASE_DIRS
        "${PROJECT_SOURCE_DIR}/src"
    FILES
        BaseHook.h
        BinaryReadWrite.h
        BinaryValidation.h
        Collections.h
        Logging.h
        Pointer.h
        Process.h
        MemoryUtils.h
)

target_precompile_headers(FirelinkCore
    PRIVATE pch.h
)

generate_firelink_export_header(FirelinkCore FIRELINK_CORE_API)
