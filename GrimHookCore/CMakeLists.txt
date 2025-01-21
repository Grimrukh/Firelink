cmake_minimum_required(VERSION 3.17)
project(GrimHookCore)

# C++20
set(CMAKE_CXX_STANDARD 20)

set(PROJECT_SOURCES
        src/dllmain.cpp
        src/pch.cpp
        src/GrimHook/BaseHook.cpp
        src/GrimHook/Logging.cpp
        src/GrimHook/Pointer.cpp
        src/GrimHook/Process.cpp
        src/GrimHook/MemoryUtils.cpp
)

add_library(GrimHookCore SHARED ${PROJECT_SOURCES})

if (WIN32)
    # Disable default 'lib' prefix of DLL.
    set_target_properties(GrimHookCore PROPERTIES PREFIX "")
endif()

# Enable DLL exports for GrimHookCore.
target_compile_definitions(GrimHookCore PRIVATE GRIMHOOKCORE_EXPORTS)

target_precompile_headers(GrimHookCore
        PRIVATE src/pch.h
)

target_include_directories(GrimHookCore
        PUBLIC include
        PRIVATE src/private
)
