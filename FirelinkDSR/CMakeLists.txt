cmake_minimum_required(VERSION 3.27)
project(FirelinkDSR)

add_subdirectory(src)

# Interface target for all FirelinkDSR component targets.
add_library(FirelinkDSR INTERFACE)
target_link_libraries(FirelinkDSR INTERFACE
    FirelinkDSRHook
)

install(TARGETS FirelinkDSR)
