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
add_test(NAME LogTest COMMAND LogTest)

message(STATUS "Adding Elden Ring MSB test...")
add_test(NAME EldenRingMSBTest COMMAND EldenRingMSBTest)

# Link statically against GCC runtime (all).
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(LogTest PRIVATE -static)
    target_link_options(EldenRingMSBTest PRIVATE -static)
endif()

target_link_libraries(LogTest
    PRIVATE GrimHookCore
    PRIVATE GrimHookDSR
    PRIVATE GrimHookER
)

target_link_libraries(EldenRingMSBTest
    PRIVATE GrimHookCore
    PRIVATE GrimHookDSR
    PRIVATE GrimHookER
)

# Copy DLLs to the output directories.
add_custom_command(TARGET LogTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:GrimHookCore>
        $<TARGET_FILE:GrimHookDSR>
        $<TARGET_FILE:GrimHookER>
        $<TARGET_FILE_DIR:LogTest>
)

add_custom_command(TARGET EldenRingMSBTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:GrimHookCore>
    $<TARGET_FILE:GrimHookDSR>
    $<TARGET_FILE:GrimHookER>
    $<TARGET_FILE_DIR:EldenRingMSBTest>
)
add_custom_command(TARGET EldenRingMSBTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:EldenRingMSBTest>/resources/elden_ring
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/elden_ring/m10_00_00_00.msb
    $<TARGET_FILE_DIR:EldenRingMSBTest>/resources/elden_ring/m10_00_00_00.msb
)

