# tests/CMakeLists.txt
cmake_minimum_required(VERSION 3.21)

# C++20
set(CMAKE_CXX_STANDARD 20)

add_executable(LogTest
    log_test.cpp
)

add_executable(EldenRingMSBTest
    er_msb_test.cpp
)

enable_testing()

message(STATUS "Adding LogTest test...")
add_test(NAME LogTest
    COMMAND LogTest
    WORKING_DIRECTORY $<TARGET_FILE_DIR:LogTest>
)

message(STATUS "Adding Elden Ring MSB test...")
add_test(NAME EldenRingMSBTest
    COMMAND EldenRingMSBTest
    WORKING_DIRECTORY $<TARGET_FILE_DIR:EldenRingMSBTest>
)

# Link statically against GCC runtime (all).
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(LogTest PRIVATE -static)
    target_link_options(EldenRingMSBTest PRIVATE -static)
endif()

target_link_libraries(LogTest
    PRIVATE FirelinkCore
    PRIVATE FirelinkDSR
    PRIVATE FirelinkER
)

target_link_libraries(EldenRingMSBTest
    PRIVATE FirelinkCore
    PRIVATE FirelinkDSR
    PRIVATE FirelinkER
)

# Copy DLLs to the output directories.
add_custom_command(TARGET LogTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:FirelinkCore>
        $<TARGET_FILE:FirelinkDSR>
        $<TARGET_FILE:FirelinkER>
        $<TARGET_FILE_DIR:LogTest>
)

add_custom_command(TARGET EldenRingMSBTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:FirelinkCore>
        $<TARGET_FILE:FirelinkER>
        $<TARGET_FILE_DIR:EldenRingMSBTest>
)
add_custom_command(TARGET EldenRingMSBTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:EldenRingMSBTest>/resources/elden_ring
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/elden_ring/m10_00_00_00.msb
        $<TARGET_FILE_DIR:EldenRingMSBTest>/resources/elden_ring/m10_00_00_00.msb
)