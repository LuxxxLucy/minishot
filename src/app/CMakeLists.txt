add_executable(minishot
    main.c
    render.c
    overlay.c
    view.c
    settings.c)
target_include_directories(minishot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(minishot PRIVATE
    clay stb SDL3::SDL3 SDL3_ttf::SDL3_ttf
    capture appstate hotkey pinmodel geometry naming config toast
    "-framework Carbon" "-framework Cocoa")
