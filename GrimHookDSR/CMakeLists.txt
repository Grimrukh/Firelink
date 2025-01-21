cmake_minimum_required(VERSION 3.17)
project(GrimHookDSR)

# C++20
set(CMAKE_CXX_STANDARD 20)

set(PROJECT_SOURCES
        src/DSRHook.cpp
)

add_library(GrimHookDSR SHARED ${PROJECT_SOURCES})

if (WIN32)
    # Disable default 'lib' prefix of DLL.
    set_target_properties(GrimHookDSR PROPERTIES PREFIX "")
endif()

# Enable DLL exports for GrimHookDSR.
target_compile_definitions(GrimHookDSR PRIVATE GRIMHOOKDSR_EXPORTS)

target_include_directories(GrimHookDSR
        PUBLIC include
        PRIVATE src/private
)

target_link_libraries(GrimHookDSR
        PUBLIC GrimHookCore
)