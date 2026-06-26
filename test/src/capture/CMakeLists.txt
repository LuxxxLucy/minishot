add_executable(test_capture test_capture.c)
target_link_libraries(test_capture PRIVATE core test_harness)
add_test(NAME capture COMMAND test_capture)
