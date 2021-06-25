cmake_minimum_required(VERSION 3.13)
project(luma_av)

set(CMAKE_VERBOSE_MAKEFILE OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(src)

enable_testing()
add_subdirectory(tests)

add_subdirectory(examples)
