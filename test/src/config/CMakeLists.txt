add_executable(test_config test_config.c)
target_link_libraries(test_config PRIVATE config test_harness)
add_test(NAME config COMMAND test_config)
