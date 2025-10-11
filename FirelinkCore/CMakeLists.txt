cmake_minimum_required(VERSION 3.27)
project(FirelinkCore VERSION 0.1.0 LANGUAGES CXX)

# Add Firelink CMake scripts.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake")
include(FirelinkModules)

add_subdirectory(src)

install(TARGETS FirelinkCore)
