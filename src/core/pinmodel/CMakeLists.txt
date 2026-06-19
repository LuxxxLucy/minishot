add_library(pinmodel STATIC pinmodel.c)
target_include_directories(pinmodel PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pinmodel PUBLIC geometry)
