add_executable(FirelinkCore_tests
    main.cpp

    BinderTest.cpp
    BinaryReadWriteTest.cpp
    DCXTest.cpp
    DDSTest.cpp
    FirelinkCoreTestHelpers.h
    LoggingTest.cpp
    TPFTest.cpp
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
