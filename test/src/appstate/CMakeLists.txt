add_executable(test_appstate test_appstate.c)
target_link_libraries(test_appstate PRIVATE appstate test_harness)
add_test(NAME appstate COMMAND test_appstate)
