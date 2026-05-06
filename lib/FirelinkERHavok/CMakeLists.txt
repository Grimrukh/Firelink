
add_firelink_library(FirelinkERHavok)

target_sources(FirelinkERHavok
    PRIVATE
    src/EldenRingHKX.cpp
    src/Navmesh.cpp
    src/Types/base.cpp
    src/Types/core.cpp
    src/Types/custom.cpp
    src/Types/hcl.cpp
    src/Types/hka.cpp
    src/Types/hkai.cpp
    src/Types/hkb.cpp
    src/Types/hkcd.cpp
    src/Types/hknp.cpp
    src/Types/hkp.cpp
    src/Types/hkx.cpp
)
target_sources(FirelinkERHavok
    PUBLIC
    FILE_SET HEADERS
    BASE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    FILES
        include/FirelinkERHavok/EldenRingHKX.h
        include/FirelinkERHavok/Navmesh.h
        include/FirelinkERHavok/Types/base.h
        include/FirelinkERHavok/Types/core.h
        include/FirelinkERHavok/Types/custom.h
        include/FirelinkERHavok/Types/hcl.h
        include/FirelinkERHavok/Types/hka.h
        include/FirelinkERHavok/Types/hkai.h
        include/FirelinkERHavok/Types/hkb.h
        include/FirelinkERHavok/Types/hkcd.h
        include/FirelinkERHavok/Types/hknp.h
        include/FirelinkERHavok/Types/hkp.h
        include/FirelinkERHavok/Types/hkx.h
)

generate_firelink_export_header(FirelinkERHavok FIRELINK_ER_HOOK_API)

target_link_libraries(FirelinkERHavok
    PUBLIC FirelinkCore
)

install(TARGETS FirelinkERHavok
        EXPORT FirelinkTargets
        FILE_SET HEADERS
)
