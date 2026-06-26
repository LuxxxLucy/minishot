add_executable(test_util test_util.c)
target_link_libraries(test_util PRIVATE core test_harness)
add_test(NAME util COMMAND test_util)
