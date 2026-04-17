# NOTE: This CML project can be used to build all Firelink modules as well as tests.
# You can also `add_subdirectory` the individual modules in your own CMake project.

cmake_minimum_required(VERSION 3.27)
project(Firelink VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(FIRELINK_BUILD_TESTS      "Build doctest-based unit tests for FirelinkCore libraries" ON)
option(FIRELINK_BUILD_PYBIND    "Build pybind11 Python extension modules for Firelink"      ON)

# Add Firelink CMake scripts.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake")
include(FirelinkModules)

# Unify Debug/Release output directories for all subprojects.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}/bin/Debug")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}/bin/Debug")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}/bin/Debug")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}/bin/Release")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}/bin/Release")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}/bin/Release")

if(MSVC)
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /DEBUG")
    set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} /DEBUG")
endif()

# Define NOMINMAX globally to prevent Windows headers from defining min/max macros.
add_compile_definitions(NOMINMAX)

if(FIRELINK_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
            doctest
            GIT_REPOSITORY https://github.com/doctest/doctest.git
            GIT_TAG        v2.5.1
            GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(doctest)

    enable_testing()
endif()

if(FIRELINK_BUILD_PYBIND)
    include(FetchContent)
    FetchContent_Declare(
            pybind11
            GIT_REPOSITORY https://github.com/pybind/pybind11.git
            GIT_TAG        v3.0.3
            GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(pybind11)
endif()

# Add subdirectories for all modules.
add_subdirectory(FirelinkCore)
add_subdirectory(FirelinkDSR)
add_subdirectory(FirelinkER)
