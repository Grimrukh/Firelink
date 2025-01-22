# tests/CMakeLists.txt
add_executable(GrimHookTest logtest.cpp)

# Link against the core and DSR libraries.
target_link_libraries(GrimHookTest
    PRIVATE GrimHookCore
    PRIVATE GrimHookDSR
)

# Add the test to CTest.
add_test(NAME GrimHookTest COMMAND GrimHookTest)

# Copy in built, linked DLLs.
add_custom_command(TARGET GrimHookTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:GrimHookCore>
    $<TARGET_FILE:GrimHookDSR>
    $<TARGET_FILE_DIR:GrimHookTest>
)