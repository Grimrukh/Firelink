cmake_minimum_required(VERSION 3.21)

project(GrimHook LANGUAGES CXX)

# Enable ctest.
enable_testing()

# Add subdirectories for modules and tests.
add_subdirectory(GrimHookCore)
add_subdirectory(GrimHookDSR)
add_subdirectory(GrimHookER)
add_subdirectory(tests)
