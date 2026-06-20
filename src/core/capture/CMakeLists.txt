add_library(capture STATIC capture.c)
target_include_directories(capture PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
