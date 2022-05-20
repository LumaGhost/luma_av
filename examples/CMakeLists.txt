cmake_minimum_required (VERSION 3.13)

if(NOT TARGET luma_av::luma_av)
   find_package(luma_av CONFIG REQUIRED)
endif()

# avio reading example
add_executable(avio_reading_example
               avio_reading_example.cpp
)
target_compile_features(avio_reading_example PUBLIC cxx_std_20)

target_link_libraries(avio_reading_example PUBLIC 
   luma_av::luma_av
   GTest::gtest_main
)

# decode video example
add_executable(decode_video_example
               decode_video_example.cpp
)
target_compile_features(decode_video_example PUBLIC cxx_std_20)

target_link_libraries(decode_video_example PUBLIC 
   luma_av::luma_av
   GTest::gtest_main
)

# filtering video example
add_executable(filtering_video_example
               filtering_video_example.cpp
)
target_compile_features(filtering_video_example PUBLIC cxx_std_20)

target_link_libraries(filtering_video_example PUBLIC 
   luma_av::luma_av
   GTest::gtest_main
)
