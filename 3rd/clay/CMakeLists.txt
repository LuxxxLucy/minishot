add_library(clay STATIC clay_impl.c)
target_include_directories(clay PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# Vendored code: silence its warnings.
target_compile_options(clay PRIVATE -w)
