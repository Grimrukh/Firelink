cmake_minimum_required(VERSION 3.20)
project(GrimHookTest)

# C++20
set(CMAKE_CXX_STANDARD 20)

# Subdirectories for core and DSR modules
add_subdirectory(GrimHookCore)
add_subdirectory(GrimHookDSR)

# Enable testing
enable_testing()

# Add test directory
add_subdirectory(tests)