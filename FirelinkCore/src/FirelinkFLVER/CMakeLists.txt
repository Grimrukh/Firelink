
add_library(FirelinkFLVER)

target_sources(FirelinkFLVER
    PRIVATE
    src/Bone.cpp
    src/Dummy.cpp
    src/Encodings.cpp
    src/FLVER.cpp
    src/LayoutRepair.cpp
    src/Material.cpp
    src/MergedMesh.cpp
    src/Mesh.cpp
    src/TextureFinder.cpp
    src/Version.cpp
    src/VertexFormats.cpp
)
target_sources(FirelinkFLVER
    PUBLIC
    FILE_SET HEADERS
    BASE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    FILES
        include/FirelinkFLVER/Bone.h
        include/FirelinkFLVER/Dummy.h
        include/FirelinkFLVER/Encodings.h
        include/FirelinkFLVER/Exceptions.h
        include/FirelinkFLVER/FLVER.h
        include/FirelinkFLVER/LayoutRepair.h
        include/FirelinkFLVER/Material.h
        include/FirelinkFLVER/Math.h
        include/FirelinkFLVER/Mesh.h
        include/FirelinkFLVER/MergedMesh.h
        include/FirelinkFLVER/TextureFinder.h
        include/FirelinkFLVER/Version.h
        include/FirelinkFLVER/VertexFormats.h
)

generate_firelink_export_header(FirelinkFLVER FIRELINK_FLVER_API)

target_link_libraries(FirelinkFLVER
    PUBLIC FirelinkCore
)

install(TARGETS FirelinkFLVER
        EXPORT FirelinkTargets
        FILE_SET HEADERS
)
