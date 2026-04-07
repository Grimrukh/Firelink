cmake_minimum_required(VERSION 3.27)
project(FirelinkDSR)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(src)

# Interface target for all FirelinkDSR component targets.
add_library(FirelinkDSR INTERFACE)
target_link_libraries(FirelinkDSR INTERFACE
    FirelinkDSRHook
)

install(TARGETS FirelinkDSR)
