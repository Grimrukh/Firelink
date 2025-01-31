cmake_minimum_required(VERSION 3.21)
project(GrimHookER)

# C++20
set(CMAKE_CXX_STANDARD 20)

set(PROJECT_SOURCES
    include/GrimHookER/ERHook.h
    include/GrimHookER/EROffsets.h
    include/GrimHookER/Export.h
    include/GrimHookER/Maps/MapStudio/Enums.h
    include/GrimHookER/Maps/MapStudio/Entry.h
    include/GrimHookER/Maps/MapStudio/EntryParam.h
    include/GrimHookER/Maps/MapStudio/EntryReference.h
    include/GrimHookER/Maps/MapStudio/Event.h
    include/GrimHookER/Maps/MapStudio/EventParam.h
    include/GrimHookER/Maps/MapStudio/Layer.h
    include/GrimHookER/Maps/MapStudio/LayerParam.h
    include/GrimHookER/Maps/MapStudio/Model.h
    include/GrimHookER/Maps/MapStudio/ModelParam.h
    include/GrimHookER/Maps/MapStudio/MSB.h
    include/GrimHookER/Maps/MapStudio/MSBFormatError.h
    include/GrimHookER/Maps/MapStudio/Part.h
    include/GrimHookER/Maps/MapStudio/PartParam.h
    include/GrimHookER/Maps/MapStudio/Region.h
    include/GrimHookER/Maps/MapStudio/RegionParam.h
    include/GrimHookER/Maps/MapStudio/Route.h
    include/GrimHookER/Maps/MapStudio/RouteParam.h
    include/GrimHookER/Maps/MapStudio/Shape.h

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

add_library(GrimHookER SHARED ${PROJECT_SOURCES})

if (WIN32)
    # Disable default 'lib' prefix of DLL.
    set_target_properties(GrimHookER PROPERTIES PREFIX "")
endif()

# Enable DLL exports for GrimHookER.
target_compile_definitions(GrimHookER PRIVATE GRIMHOOKER_EXPORTS)

target_include_directories(GrimHookER
    PUBLIC include
)

target_link_libraries(GrimHookER
    PUBLIC GrimHookCore
)