add_executable(test_toast test_toast.c)
target_link_libraries(test_toast PRIVATE toast test_harness)
add_test(NAME toast COMMAND test_toast)
