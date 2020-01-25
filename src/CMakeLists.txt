cmake_minimum_required (VERSION 3.10)
project (luma_av VERSION 0.0.0)

find_package(boost REQUIRED)
find_package(ffmpeg REQUIRED)

add_library(luma_av INTERFACE)
target_compile_features(luma_av INTERFACE cxx_std_17)
target_include_directories(luma_av INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(luma_av INTERFACE 
    boost::boost
    ffmpeg::ffmpeg
)

add_library(luma_av::luma_av ALIAS luma_av)
