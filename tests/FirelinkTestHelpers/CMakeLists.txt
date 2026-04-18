add_library(FirelinkTestHelpers INTERFACE)

target_include_directories(FirelinkTestHelpers INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(FirelinkTestHelpers INTERFACE FirelinkCore doctest::doctest)
