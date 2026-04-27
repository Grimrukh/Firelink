add_executable(EldenRingNavmeshDev
    main.cpp
)
target_link_libraries(EldenRingNavmeshDev PRIVATE FirelinkCore)

# Copy HKX file to build directory.
add_custom_command(TARGET EldenRingNavmeshDev POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/n10_01_00_00_001000.hkx"
        $<TARGET_FILE_DIR:EldenRingNavmeshDev>/n10_01_00_00_001000.hkx
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/n10_01_00_00_001700.hkx"
        $<TARGET_FILE_DIR:EldenRingNavmeshDev>/n10_01_00_00_001700.hkx
)