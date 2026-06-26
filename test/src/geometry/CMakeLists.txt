add_executable(test_geometry test_geometry.c)
target_link_libraries(test_geometry PRIVATE core test_harness)
add_test(NAME geometry COMMAND test_geometry)
