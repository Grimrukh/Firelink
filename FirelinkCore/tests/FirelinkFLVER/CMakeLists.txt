# Link the test executable against the OBJECT library directly. This
# pulls in the same .obj files as the shared/static targets (avoiding
# duplicate compiles) and carries the PUBLIC include directories.
add_executable(FirelinkFLVER_tests
    test_main.cpp
    test_VertexFormats.cpp
    test_FLVER_read.cpp
    test_FLVER_roundtrip.cpp
)
target_compile_definitions(FirelinkFLVER_tests PRIVATE
        TEST_RESOURCES_DIR="${FIRELINKCORE_TEST_RESOURCES_DIR}"
)
target_link_libraries(FirelinkFLVER_tests PRIVATE FirelinkFLVER doctest::doctest)
add_test(NAME FirelinkFLVER_tests COMMAND FirelinkFLVER_tests)
