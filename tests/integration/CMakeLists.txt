cmake_minimum_required (VERSION 3.13)

if(NOT TARGET luma_av::luma_av)
   find_package(luma_av CONFIG REQUIRED)
endif()

add_executable(luma_av_integration 
               ffmpeg_compare/avio_reading_ffmpeg_ex.cpp 
               ffmpeg_compare/decode_video_ffmpeg_ex.cpp
               ffmpeg_compare/filter_video_ffmpeg_ex.cpp
               luma_av/codec_tests.cpp
)
target_compile_features(luma_av_integration PUBLIC cxx_std_20)

target_link_libraries(luma_av_integration PUBLIC 
   luma_av::luma_av
   GTest::gtest_main
)