cmake_minimum_required(VERSION 3.20)
project(minishot C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra)

find_package(SDL3 REQUIRED CONFIG)
find_package(SDL3_ttf REQUIRED CONFIG)

# Third-party dependencies.
add_subdirectory(3rd/clay)
add_subdirectory(3rd/stb)
