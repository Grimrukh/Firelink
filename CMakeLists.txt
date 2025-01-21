# Combined test project for GrimHookCore and GrimHookDSR.
cmake_minimum_required(VERSION 3.20)

# C++20
set(CMAKE_CXX_STANDARD 20)

project(GrimHookTest)

add_subdirectory(GrimHookCore)
add_subdirectory(GrimHookDSR)

add_executable(GrimHookTest
    main.cpp
)

include_directories(GrimHookCore/include)
include_directories(GrimHookDSR/include)

target_link_libraries(GrimHookTest
    PUBLIC GrimHookCore
    PUBLIC GrimHookDSR
)

# Copy DLLs into the build directory
add_custom_command(TARGET GrimHookTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:GrimHookCore> $<TARGET_FILE:GrimHookDSR>
    $<TARGET_FILE_DIR:GrimHookTest>
)