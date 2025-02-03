cmake_minimum_required(VERSION 3.21)
project(FirelinkDSR)

# C++20
set(CMAKE_CXX_STANDARD 20)

set(PROJECT_SOURCES
    include/FirelinkDSR/DSREnums.h
    include/FirelinkDSR/DSRHook.h
    include/FirelinkDSR/DSROffsets.h
    include/FirelinkDSR/Export.h

    src/DSRHook.cpp
)

add_library(FirelinkDSR SHARED ${PROJECT_SOURCES})

if (WIN32)
    # Disable default 'lib' prefix of DLL.
    set_target_properties(FirelinkDSR PROPERTIES PREFIX "")
endif()

# Enable DLL exports for FirelinkDSR.
target_compile_definitions(FirelinkDSR PRIVATE FIRELINK_DSR_EXPORTS)

target_include_directories(FirelinkDSR
    PUBLIC include
    PRIVATE src/private
)

target_link_libraries(FirelinkDSR
    PUBLIC FirelinkCore
)