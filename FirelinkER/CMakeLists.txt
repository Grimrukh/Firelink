cmake_minimum_required(VERSION 3.21)
project(FirelinkER)

# C++20
set(CMAKE_CXX_STANDARD 20)

set(PROJECT_SOURCES
    include/FirelinkER/ERHook.h
    include/FirelinkER/EROffsets.h
    include/FirelinkER/Export.h
    include/FirelinkER/Maps/MapStudio/Enums.h
    include/FirelinkER/Maps/MapStudio/Entry.h
    include/FirelinkER/Maps/MapStudio/EntryParam.h
    include/FirelinkER/Maps/MapStudio/EntryReference.h
    include/FirelinkER/Maps/MapStudio/Event.h
    include/FirelinkER/Maps/MapStudio/EventParam.h
    include/FirelinkER/Maps/MapStudio/Layer.h
    include/FirelinkER/Maps/MapStudio/LayerParam.h
    include/FirelinkER/Maps/MapStudio/Model.h
    include/FirelinkER/Maps/MapStudio/ModelParam.h
    include/FirelinkER/Maps/MapStudio/MSB.h
    include/FirelinkER/Maps/MapStudio/MSBFormatError.h
    include/FirelinkER/Maps/MapStudio/Part.h
    include/FirelinkER/Maps/MapStudio/PartParam.h
    include/FirelinkER/Maps/MapStudio/Region.h
    include/FirelinkER/Maps/MapStudio/RegionParam.h
    include/FirelinkER/Maps/MapStudio/Route.h
    include/FirelinkER/Maps/MapStudio/RouteParam.h
    include/FirelinkER/Maps/MapStudio/Shape.h

    src/ERHook.cpp
    src/Maps/MapStudio/Entry.cpp
    src/Maps/MapStudio/EntryParam.cpp
    src/Maps/MapStudio/EntryReference.cpp
    src/Maps/MapStudio/Event.cpp
    src/Maps/MapStudio/Model.cpp
    src/Maps/MapStudio/MSB.cpp
    src/Maps/MapStudio/Part.cpp
    src/Maps/MapStudio/Region.cpp
    src/Maps/MapStudio/Route.cpp
    src/Maps/MapStudio/Shape.cpp
)

add_library(FirelinkER SHARED ${PROJECT_SOURCES})

if (WIN32)
    # Disable default 'lib' prefix of DLL.
    set_target_properties(FirelinkER PROPERTIES PREFIX "")
endif()

# Enable DLL exports for FirelinkER.
target_compile_definitions(FirelinkER PRIVATE FIRELINK_ER_EXPORTS)

target_include_directories(FirelinkER
    PUBLIC include
)

target_link_libraries(FirelinkER
    PUBLIC FirelinkCore
)