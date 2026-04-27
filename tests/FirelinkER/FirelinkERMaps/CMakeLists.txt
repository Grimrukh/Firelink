# Link the test executable against the OBJECT library directly. This
# pulls in the same .obj files as the shared/static targets (avoiding
# duplicate compiles) and carries the PUBLIC include directories.
add_executable(FirelinkERMaps_test
    main.cpp

    EldenRingMSBTest.cpp
    EldenRingNavmeshTest.cpp
)

target_link_libraries(FirelinkERMaps_test PRIVATE
    FirelinkTestHelpers
    FirelinkERMaps
    doctest::doctest
)

target_compile_definitions(FirelinkERMaps_test PRIVATE
    TEST_RESOURCES_DIR="${FIRELINK_TEST_RESOURCES_DIR}"
)

add_test(NAME FirelinkERMaps_test COMMAND FirelinkERMaps_test)
