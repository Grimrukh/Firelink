
add_firelink_library(FirelinkERMaps)

target_sources(FirelinkERMaps
    PRIVATE
    src/MapStudio/Entry.cpp
    src/MapStudio/EntryParam.cpp
    src/MapStudio/EntryReference.cpp
    src/MapStudio/Event.cpp
    src/MapStudio/Model.cpp
    src/MapStudio/MSB.cpp
    src/MapStudio/Part.cpp
    src/MapStudio/Region.cpp
    src/MapStudio/Route.cpp
    src/MapStudio/Shape.cpp
)
target_sources(FirelinkERMaps
    PUBLIC
    FILE_SET HEADERS
    BASE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    FILES
        include/FirelinkERMaps/MapStudio/Enums.h
        include/FirelinkERMaps/MapStudio/Entry.h
        include/FirelinkERMaps/MapStudio/EntryParam.h
        include/FirelinkERMaps/MapStudio/EntryReference.h
        include/FirelinkERMaps/MapStudio/Event.h
        include/FirelinkERMaps/MapStudio/EventParam.h
        include/FirelinkERMaps/MapStudio/Layer.h
        include/FirelinkERMaps/MapStudio/LayerParam.h
        include/FirelinkERMaps/MapStudio/Model.h
        include/FirelinkERMaps/MapStudio/ModelParam.h
        include/FirelinkERMaps/MapStudio/MSB.h
        include/FirelinkERMaps/MapStudio/MSBFormatError.h
        include/FirelinkERMaps/MapStudio/Part.h
        include/FirelinkERMaps/MapStudio/PartParam.h
        include/FirelinkERMaps/MapStudio/Region.h
        include/FirelinkERMaps/MapStudio/RegionParam.h
        include/FirelinkERMaps/MapStudio/Route.h
        include/FirelinkERMaps/MapStudio/RouteParam.h
        include/FirelinkERMaps/MapStudio/Shape.h
)

generate_firelink_export_header(FirelinkERMaps FIRELINK_ER_MAPS_API)

target_link_libraries(FirelinkERMaps
    PUBLIC FirelinkCore
)

install(TARGETS FirelinkERMaps
        EXPORT FirelinkTargets
        FILE_SET HEADERS
)
