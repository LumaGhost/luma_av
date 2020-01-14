cmake_minimum_required (VERSION 3.10)
project (luma_av)

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
                BASIC_SETUP 
                BUILD missing)

add_library(luma_http INTERFACE)
target_compile_features(luma_http INTERFACE cxx_std_17)
target_include_directories(luma_http INTERFACE include)
target_link_libraries(luma_http INTERFACE ${CONAN_LIBS})
