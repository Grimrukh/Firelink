add_firelink_library(FirelinkCore)

target_sources(FirelinkCore
    PRIVATE
    src/dllmain.cpp
    src/pch.h
    src/pch.cpp
    src/BaseHook.cpp
    src/Binder.cpp
    src/BinaryReadWrite.cpp
    src/Collections.cpp
    src/DCX.cpp
    src/DDS.cpp
    src/Logging.cpp
    src/Oodle.cpp
    src/Paths.cpp
    src/Pointer.cpp
    src/Process.cpp
    src/TPF.cpp
    src/MemoryUtils.cpp
)
target_sources(FirelinkCore
    PUBLIC
    FILE_SET HEADERS
    BASE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    FILES
        include/FirelinkCore/BaseHook.h
        include/FirelinkCore/Binder.h
        include/FirelinkCore/BinaryReadWrite.h
        include/FirelinkCore/BinaryValidation.h
        include/FirelinkCore/Collections.h
        include/FirelinkCore/DCX.h
        include/FirelinkCore/DDS.h
        include/FirelinkCore/GameFile.h
        include/FirelinkCore/GameType.h
        include/FirelinkCore/Logging.h
        include/FirelinkCore/Logging.h
        include/FirelinkCore/Oodle.h
        include/FirelinkCore/Paths.h
        include/FirelinkCore/Pointer.h
        include/FirelinkCore/Process.h
        include/FirelinkCore/TPF.h
        include/FirelinkCore/MemoryUtils.h
)

target_precompile_headers(FirelinkCore
    PRIVATE src/pch.h
)

generate_firelink_export_header(FirelinkCore FIRELINK_CORE_API)

# DCX compression support requires zlib and zstd (fetched automatically).
include(FetchContent)

# --- zlib ---
FetchContent_Declare(
    zlib
    GIT_REPOSITORY https://github.com/madler/zlib.git
    GIT_TAG        v1.3.2
)
set(ZLIB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(zlib)

# zlib's CMakeLists.txt creates a `zlibstatic` target.
# Alias it so downstream code can use the canonical name if needed.
if(NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB ALIAS zlibstatic)
endif()

# --- zstd ---
FetchContent_Declare(
    zstd
    GIT_REPOSITORY https://github.com/facebook/zstd.git
    GIT_TAG        v1.5.7
    SOURCE_SUBDIR  build/cmake
)
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(zstd)

# Ensure the fetched headers are visible to FirelinkCore sources.
target_include_directories(FirelinkCore PRIVATE
    "${zlib_SOURCE_DIR}"
    "${zlib_BINARY_DIR}"
    "${zstd_SOURCE_DIR}/lib"
)

# --- DirectXTex ---
# In-memory DDS conversion (LoadFromDDSMemory, Decompress, Convert,
# SaveToTGAMemory, SaveToWICMemory).  We only need the static library.
FetchContent_Declare(
    DirectXTex
    GIT_REPOSITORY https://github.com/microsoft/DirectXTex.git
    GIT_TAG        oct2025
)
set(BUILD_TOOLS    OFF CACHE BOOL "" FORCE)
set(BUILD_SAMPLE   OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING  OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(DirectXTex)

# Link against the fetched dependencies.
# ole32   — COM / WIC (PNG output via SaveToWICMemory)
# d3d11   — GPU-accelerated BC6H/BC7 compression via DirectCompute
# dxgi    — DXGI adapter enumeration for D3D11 device creation
target_link_libraries(FirelinkCore PRIVATE zlibstatic libzstd_static DirectXTex ole32.lib d3d11.lib dxgi.lib)

install(TARGETS FirelinkCore
        EXPORT FirelinkTargets
        FILE_SET HEADERS
)
