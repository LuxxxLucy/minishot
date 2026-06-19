add_executable(test_pinmodel test_pinmodel.c)
target_link_libraries(test_pinmodel PRIVATE pinmodel geometry test_harness)
add_test(NAME pinmodel COMMAND test_pinmodel)
