# Shared header-only test harness (test.h).
add_library(test_harness INTERFACE)
target_include_directories(test_harness INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(src/geometry)
add_subdirectory(src/naming)
add_subdirectory(src/config)
add_subdirectory(src/capture)
add_subdirectory(src/hotkey)
