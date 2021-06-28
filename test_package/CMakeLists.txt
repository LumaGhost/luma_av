cmake_minimum_required(VERSION 3.13)

find_package(luma_av REQUIRED)

enable_testing()

add_executable(luma_av_testpackage example.cpp)

target_compile_features(luma_av_testpackage PUBLIC cxx_std_20)

target_link_libraries(luma_av_testpackage
    PRIVATE
        luma_av::luma_av
)

add_test(NAME luma_av.testpackage
    COMMAND luma_av_testpackage
)