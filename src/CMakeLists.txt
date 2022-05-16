cmake_minimum_required (VERSION 3.10)
project (luma_av VERSION 0.0.0)

add_library(luma_av STATIC)
target_compile_features(luma_av PUBLIC cxx_std_20)
target_include_directories(luma_av PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(luma_av PUBLIC 
    outcome::outcome
    ffmpeg::ffmpeg
)

target_sources(luma_av PRIVATE codec.cpp format.cpp)



add_library(luma_av::luma_av ALIAS luma_av)
