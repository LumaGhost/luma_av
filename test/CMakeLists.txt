cmake_minimum_required (VERSION 3.10)
project (luma_av_tests)

# https://github.com/conan-io/cmake-conan
# Download automatically, you can also just copy the conan.cmake file
if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
   message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
   file(DOWNLOAD "https://github.com/conan-io/cmake-conan/raw/v0.14/conan.cmake"
                 "${CMAKE_BINARY_DIR}/conan.cmake")
endif()

include(${CMAKE_BINARY_DIR}/conan.cmake)

# https://bintray.com/beta/#/conan-community/conan/boost:conan/1.71.0:stable?tab=overview
conan_cmake_run(REQUIRES boost/1.71.0@conan/stable
                         gtest/1.8.0@conan/stable
                BASIC_SETUP 
                BUILD missing)

add_executable(luma_http_tests test_main.cpp
                               luma/av/frame_tests.cpp)
target_compile_features(luma_http_tests PUBLIC cxx_std_17)
target_include_directories(luma_http_tests PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/include)
target_link_libraries(luma_http_tests PUBLIC ${CONAN_LIBS})
