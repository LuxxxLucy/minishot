add_executable(test_toolbar test_toolbar.c)
target_link_libraries(test_toolbar PRIVATE toolbar test_harness)
add_test(NAME toolbar COMMAND test_toolbar)
