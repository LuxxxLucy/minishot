# One static library for all core logic: pure, platform-light, unit-tested.
add_library(core STATIC
    geometry/geometry.c
    naming/naming.c
    util/util.c
    config/config.c
    capture/capture.c
    hotkey/hotkey.c)
target_include_directories(core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/geometry
    ${CMAKE_CURRENT_SOURCE_DIR}/naming
    ${CMAKE_CURRENT_SOURCE_DIR}/util
    ${CMAKE_CURRENT_SOURCE_DIR}/config
    ${CMAKE_CURRENT_SOURCE_DIR}/capture
    ${CMAKE_CURRENT_SOURCE_DIR}/hotkey)
target_link_libraries(core PRIVATE "-framework Carbon")
