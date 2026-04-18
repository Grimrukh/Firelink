add_executable(FirelinkFLVER_tests
    test_main.cpp
    test_VertexFormats.cpp
    test_FLVER_read.cpp
    test_FLVER_roundtrip.cpp
)

target_link_libraries(FirelinkFLVER_tests PRIVATE
    FirelinkTestHelpers
    FirelinkFLVER
    doctest::doctest
)

target_compile_definitions(FirelinkFLVER_tests PRIVATE
    TEST_RESOURCES_DIR="${FIRELINK_TEST_RESOURCES_DIR}"
)

add_test(NAME FirelinkFLVER_tests COMMAND FirelinkFLVER_tests)
