cmake_minimum_required(VERSION 3.13)

find_package(GTest REQUIRED)

add_subdirectory(unit)
add_subdirectory(integration)