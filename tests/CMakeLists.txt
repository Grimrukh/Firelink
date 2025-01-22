# tests/CMakeLists.txt
cmake_minimum_required(VERSION 3.21)

# C++20
set(CMAKE_CXX_STANDARD 20)

add_executable(LogTest
    log_test.cpp
)

enable_testing()
message(STATUS "Adding LogTest test...")
add_test(NAME LogTest COMMAND LogTest)

# Link statically against GCC runtime (all).
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(LogTest PRIVATE -static)
endif()

target_link_libraries(LogTest
    PRIVATE GrimHookCore
    PRIVATE GrimHookDSR
)

# Copy DLLs to the output directory.
add_custom_command(TARGET LogTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:GrimHookCore>
        $<TARGET_FILE:GrimHookDSR>
        $<TARGET_FILE_DIR:LogTest>
)
