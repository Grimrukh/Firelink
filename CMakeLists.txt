# NOTE: This CML project can be used to build all Firelink modules as well as tests.
# You can also `add_subdirectory` the individual modules in your own CMake project.

cmake_minimum_required(VERSION 3.27)
project(Firelink VERSION 0.1.0 LANGUAGES CXX)

# Unify Debug/Release output directories for all subprojects.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/bin/Debug")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/bin/Debug")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/bin/Debug")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin/Release")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin/Release")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin/Release")

if(MSVC)
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /DEBUG")
    set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} /DEBUG")
endif()

# Add subdirectories for all modules.
add_subdirectory(FirelinkCore)
add_subdirectory(FirelinkDSR)
add_subdirectory(FirelinkER)

# TODO: Put tests into appropriate subdirectories.
option(FIRELINK_WITH_TESTS "Build Firelink tests" ON)
if (FIRELINK_WITH_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
