# Link the test executable against the OBJECT library directly. This
# pulls in the same .obj files as the shared/static targets (avoiding
# duplicate compiles) and carries the PUBLIC include directories.
add_executable(FirelinkERHavok_test
    main.cpp
    EldenRingNavmeshTest.cpp
    EldenRingCollisionTest.cpp
)

target_link_libraries(FirelinkERHavok_test PRIVATE
    FirelinkTestHelpers
    FirelinkERHavok
    doctest::doctest
)

target_compile_definitions(FirelinkERHavok_test PRIVATE
    TEST_RESOURCES_DIR="${FIRELINK_TEST_RESOURCES_DIR}"
)

add_test(NAME FirelinkERHavok_test COMMAND FirelinkERHavok_test)
