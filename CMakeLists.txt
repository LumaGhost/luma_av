cmake_minimum_required(VERSION 3.13)
project(luma_av)

set(CMAKE_VERBOSE_MAKEFILE OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(outcome REQUIRED)
find_package(ffmpeg REQUIRED)

add_subdirectory(src)

find_package(GTest REQUIRED)
enable_testing()
include(GoogleTest)
add_subdirectory(tests)

add_subdirectory(examples)
