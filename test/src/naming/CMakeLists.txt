add_executable(test_naming test_naming.c)
target_link_libraries(test_naming PRIVATE naming test_harness)
add_test(NAME naming COMMAND test_naming)
