cmake_minimum_required (VERSION 3.10)
project (luma_av_tests)

if(NOT TARGET luma_av::luma_av)
   find_package(luma_av CONFIG REQUIRED)
endif()

find_package(GTest REQUIRED)

add_executable(luma_av_tests test_main.cpp
                             luma/av/detail/unique_or_null_tests.cpp
                             luma_av/decode_video_example.cpp
                             luma_av/avio_reading_example.cpp
                             luma_av/frame_tests.cpp
                             luma_av/codec_tests.cpp
                             luma_av/result_tests.cpp)
target_compile_features(luma_av_tests PUBLIC cxx_std_17)
target_include_directories(luma_av_tests PUBLIC 
   ${CMAKE_CURRENT_SOURCE_DIR}/src/include
)
target_link_libraries(luma_av_tests PUBLIC 
   luma_av::luma_av
   GTest::GTest
)
