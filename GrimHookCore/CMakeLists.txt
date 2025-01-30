cmake_minimum_required(VERSION 3.21)
project(GrimHookCore)

# C++20
set(CMAKE_CXX_STANDARD 20)

set(PROJECT_SOURCES
    include/GrimHook/BaseHook.h
    include/GrimHook/BinaryReadWrite.h
    include/GrimHook/BinaryValidation.h
    include/GrimHook/Collections.h
    include/GrimHook/Export.h
    include/GrimHook/Logging.h
    include/GrimHook/Pointer.h
    include/GrimHook/Process.h
    include/GrimHook/MemoryUtils.h

    src/dllmain.cpp
    src/pch.h
    src/pch.cpp
    src/GrimHook/BaseHook.cpp
    src/GrimHook/BinaryReadWrite.cpp
    src/GrimHook/Collections.cpp
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

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(GrimHookCore PRIVATE -static)
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
