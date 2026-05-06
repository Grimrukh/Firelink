add_executable(FirelinkFLVER_tests
    main.cpp

#        FLVER0Test.cpp  # TODO: FLVER0 needs work
        FLVER2Test.cpp
        TextureFinderTest.cpp
        VertexFormatsTest.cpp
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
