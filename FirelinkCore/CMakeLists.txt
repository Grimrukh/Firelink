cmake_minimum_required(VERSION 3.21)
project(FirelinkCore)

# C++20
set(CMAKE_CXX_STANDARD 20)

set(PROJECT_SOURCES
    include/Firelink/BaseHook.h
    include/Firelink/BinaryReadWrite.h
    include/Firelink/BinaryValidation.h
    include/Firelink/Collections.h
    include/Firelink/Export.h
    include/Firelink/Logging.h
    include/Firelink/Pointer.h
    include/Firelink/Process.h
    include/Firelink/MemoryUtils.h

    src/dllmain.cpp
    src/pch.h
    src/pch.cpp
    src/Firelink/BaseHook.cpp
    src/Firelink/BinaryReadWrite.cpp
    src/Firelink/Collections.cpp
    src/Firelink/Logging.cpp
    src/Firelink/Pointer.cpp
    src/Firelink/Process.cpp
    src/Firelink/MemoryUtils.cpp
)

add_library(FirelinkCore SHARED ${PROJECT_SOURCES})

if (WIN32)
    # Disable default 'lib' prefix of DLL.
    set_target_properties(FirelinkCore PROPERTIES PREFIX "")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(FirelinkCore PRIVATE -static)
endif()

# Enable DLL exports for FirelinkCore.
target_compile_definitions(FirelinkCore PRIVATE FIRELINKCORE_EXPORTS)

target_precompile_headers(FirelinkCore
    PRIVATE src/pch.h
)

target_include_directories(FirelinkCore
    PUBLIC include
    PRIVATE src/private
)
