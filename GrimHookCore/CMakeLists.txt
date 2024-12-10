cmake_minimum_required(VERSION 3.30)
project(GrimHookCore)

set(CMAKE_CXX_STANDARD 20)

add_library(GrimHookCore SHARED
        src/dllmain.cpp
        src/GrimHook/Logging.cpp
        src/GrimHook/MemoryReadWrite.cpp
        src/pch.cpp
        src/GrimHook/Pointer.cpp
        src/GrimHook/Process.cpp
        src/GrimHook/MemoryUtils.cpp
)
if (WIN32)
    set_target_properties(GrimHookCore PROPERTIES PREFIX "")
endif()

target_precompile_headers(GrimHookCore
        PRIVATE src/pch.h
)

target_include_directories(GrimHookCore
        PUBLIC include
        PRIVATE src/private
)