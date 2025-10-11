
add_library(FirelinkERMaps SHARED)

target_sources(FirelinkERMaps
    PRIVATE
    MapStudio/Entry.cpp
    MapStudio/EntryParam.cpp
    MapStudio/EntryReference.cpp
    MapStudio/Event.cpp
    MapStudio/Model.cpp
    MapStudio/MSB.cpp
    MapStudio/Part.cpp
    MapStudio/Region.cpp
    MapStudio/Route.cpp
    MapStudio/Shape.cpp
)
target_sources(FirelinkERMaps
    PUBLIC
    FILE_SET HEADERS
    BASE_DIRS
        "${PROJECT_SOURCE_DIR}/src"
    FILES
    MapStudio/Enums.h
    MapStudio/Entry.h
    MapStudio/EntryParam.h
    MapStudio/EntryReference.h
    MapStudio/Event.h
    MapStudio/EventParam.h
    MapStudio/Layer.h
    MapStudio/LayerParam.h
    MapStudio/Model.h
    MapStudio/ModelParam.h
    MapStudio/MSB.h
    MapStudio/MSBFormatError.h
    MapStudio/Part.h
    MapStudio/PartParam.h
    MapStudio/Region.h
    MapStudio/RegionParam.h
    MapStudio/Route.h
    MapStudio/RouteParam.h
    MapStudio/Shape.h
)

generate_firelink_export_header(FirelinkERMaps FIRELINK_ER_MAPS_API)

target_link_libraries(FirelinkERMaps
    PUBLIC FirelinkCore
)
