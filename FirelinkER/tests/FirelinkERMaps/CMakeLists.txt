# Link the test executable against the OBJECT library directly. This
# pulls in the same .obj files as the shared/static targets (avoiding
# duplicate compiles) and carries the PUBLIC include directories.
add_executable(FirelinkERMaps_test
        test_main.cpp
        test_er_msb.cpp
)
target_compile_definitions(FirelinkERMaps_test PRIVATE
        TEST_RESOURCES_DIR="${FIRELINKER_TEST_RESOURCES_DIR}"
)
target_link_libraries(FirelinkERMaps_test PRIVATE FirelinkERMaps doctest::doctest)
add_test(NAME FirelinkERMaps_test COMMAND FirelinkERMaps_test)
