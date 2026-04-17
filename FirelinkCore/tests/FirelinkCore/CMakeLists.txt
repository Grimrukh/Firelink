# Link the test executable against the OBJECT library directly. This
# pulls in the same .obj files as the shared/static targets (avoiding
# duplicate compiles) and carries the PUBLIC include directories.
add_executable(FirelinkCore_tests
    test_main.cpp
    test_Binder.cpp
    test_BinaryReadWrite.cpp
    test_DCX.cpp
    test_DDS.cpp
    test_Logging.cpp
    test_TPF.cpp
)
target_compile_definitions(FirelinkCore_tests PRIVATE
    TEST_RESOURCES_DIR="${FIRELINKCORE_TEST_RESOURCES_DIR}"
)
target_link_libraries(FirelinkCore_tests PRIVATE FirelinkCore doctest::doctest)
add_test(NAME FirelinkCore_tests COMMAND FirelinkCore_tests)
