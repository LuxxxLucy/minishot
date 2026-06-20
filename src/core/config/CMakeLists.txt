add_library(config STATIC config.c)
target_include_directories(config PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
