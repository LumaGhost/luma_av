cmake_minimum_required (VERSION 3.13)

if(NOT TARGET luma_av::luma_av)
   find_package(luma_av CONFIG REQUIRED)
endif()

find_package(GTest REQUIRED)

add_executable(luma_av_unit 
               luma/av/detail/unique_or_null_tests.cpp
               luma_av/frame_tests.cpp
               luma_av/codec_tests.cpp
               luma_av/result_tests.cpp
)
target_compile_features(luma_av_unit PUBLIC cxx_std_20)

target_link_libraries(luma_av_unit PUBLIC 
   luma_av::luma_av
   GTest::GTest
)
