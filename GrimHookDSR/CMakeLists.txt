cmake_minimum_required(VERSION 3.30)
project(GrimHookDSR)

set(CMAKE_CXX_STANDARD 20)

add_library(GrimHookDSR SHARED
        src/DSRHook.cpp
)
if (WIN32)
    set_target_properties(GrimHookDSR PROPERTIES PREFIX "")
endif()

target_include_directories(GrimHookDSR
        PUBLIC include
        PRIVATE src/private
)

target_link_libraries(GrimHookDSR
        PUBLIC GrimHookCore
)