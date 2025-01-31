cmake_minimum_required(VERSION 3.21)

project(Firelink LANGUAGES CXX)

# Enable ctest.
enable_testing()

# Add subdirectories for modules and tests.
add_subdirectory(FirelinkCore)
add_subdirectory(FirelinkDSR)
add_subdirectory(FirelinkER)
add_subdirectory(tests)
