
# --------------------------------------------------------------
# Generate and include export header for a target.
# --------------------------------------------------------------
function(generate_firelink_export_header target_name api_macro_name)
    include(GenerateExportHeader)
    generate_export_header(${target_name}
        EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/Export.h
        EXPORT_MACRO_NAME ${api_macro_name}
    )
    target_compile_definitions(${target_name} PRIVATE "${target_name}_EXPORTS")
    target_sources(${target_name}
        PUBLIC
        FILE_SET HEADERS
        BASE_DIRS
        "${PROJECT_BINARY_DIR}/src"
        FILES
        "${CMAKE_CURRENT_BINARY_DIR}/Export.h"
    )
endfunction()
