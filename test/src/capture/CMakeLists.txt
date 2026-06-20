add_executable(test_capture test_capture.c)
target_link_libraries(test_capture PRIVATE capture test_harness)
add_test(NAME capture COMMAND test_capture)
