add_library(hotkey STATIC hotkey.c)
target_include_directories(hotkey PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(hotkey PUBLIC "-framework Carbon")
