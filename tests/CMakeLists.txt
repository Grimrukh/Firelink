# tests/CMakeLists.txt
cmake_minimum_required(VERSION 3.21)

# C++20
set(CMAKE_CXX_STANDARD 20)

add_executable(log_test
    log_test.cpp
)

message(STATUS "Adding log_test test...")
add_test(NAME log_test COMMAND log_test)

# Link statically against GCC runtime (all).
target_link_options(log_test PRIVATE -static)

target_link_libraries(log_test
    PRIVATE GrimHookCore
    PRIVATE GrimHookDSR
)

# Copy DLLs to the output directory.
add_custom_command(TARGET log_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:GrimHookCore>
        $<TARGET_FILE:GrimHookDSR>
        $<TARGET_FILE_DIR:log_test>
)
