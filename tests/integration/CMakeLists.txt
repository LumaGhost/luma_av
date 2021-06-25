cmake_minimum_required (VERSION 3.13)

if(NOT TARGET luma_av::luma_av)
   find_package(luma_av CONFIG REQUIRED)
endif()

find_package(GTest REQUIRED)

add_executable(luma_av_integration 
               avio_reading_int_tests.cpp 
               filter_int_tests.cpp
               decode_video_int_tests.cpp
)
target_compile_features(luma_av_integration PUBLIC cxx_std_20)

target_link_libraries(luma_av_integration PUBLIC 
   luma_av::luma_av
   GTest::GTest
)