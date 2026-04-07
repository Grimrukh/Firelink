cmake_minimum_required(VERSION 3.27)
project(FirelinkER VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(src)

# Interface target for all FirelinkER component targets.
add_library(FirelinkER INTERFACE)
target_link_libraries(FirelinkER INTERFACE
    FirelinkERHook
    FirelinkERMaps
)

install(TARGETS FirelinkER)
