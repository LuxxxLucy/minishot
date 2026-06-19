add_executable(test_hotkey test_hotkey.c)
target_link_libraries(test_hotkey PRIVATE hotkey test_harness)
add_test(NAME hotkey COMMAND test_hotkey)
