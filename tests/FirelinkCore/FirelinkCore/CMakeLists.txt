add_executable(FirelinkCore_tests
    test_main.cpp
    test_Binder.cpp
    test_BinaryReadWrite.cpp
    test_DCX.cpp
    test_DDS.cpp
    test_ImageImportManager.cpp
    test_Logging.cpp
    test_TPF.cpp
)

target_link_libraries(FirelinkCore_tests PRIVATE
    FirelinkTestHelpers
    FirelinkCore
    doctest::doctest
)

target_compile_definitions(FirelinkCore_tests PRIVATE
    TEST_RESOURCES_DIR="${FIRELINK_TEST_RESOURCES_DIR}"
)

add_test(NAME FirelinkCore_tests COMMAND FirelinkCore_tests)
