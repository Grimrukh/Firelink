
# --------------------------------------------------------------
# Generate and include export header for a target.
# --------------------------------------------------------------
function(generate_firelink_export_header target_name api_macro_name)
    include(GenerateExportHeader)
    generate_export_header(${target_name}
        EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/include/${target_name}/Export.h
        EXPORT_MACRO_NAME ${api_macro_name}
    )
    target_compile_definitions(${target_name} PRIVATE "${target_name}_EXPORTS")
    target_sources(${target_name}
        PUBLIC
        FILE_SET HEADERS
        BASE_DIRS
        "${CMAKE_CURRENT_BINARY_DIR}/include"
        FILES
        "${CMAKE_CURRENT_BINARY_DIR}/include/${target_name}/Export.h"
    )
endfunction()

macro(configure_install_pybind_module target_name)
    set_target_properties(${target_name} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )

    # Copy PYI file to build directory
    add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/${target_name}.pyi"
            "${CMAKE_BINARY_DIR}/bin/${target_name}.pyi"
    )

    # Install the module and PYI file
    install(TARGETS ${target_name}
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION bin
    )
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/${target_name}.pyi"
            DESTINATION bin
    )
endmacro()
