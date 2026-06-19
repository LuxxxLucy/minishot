add_library(stb STATIC stb_impl.c)
target_include_directories(stb PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(stb PRIVATE -w)
