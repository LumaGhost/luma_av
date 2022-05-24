cmake_minimum_required (VERSION 3.13)

if(NOT TARGET luma_av::luma_av)
   find_package(luma_av CONFIG REQUIRED)
endif()

add_executable(luma_av_unit 
               frame_tests.cpp
               result_tests.cpp
)
target_compile_features(luma_av_unit PUBLIC cxx_std_20)

target_link_libraries(luma_av_unit PUBLIC 
   luma_av::luma_av
   GTest::gtest_main
)
gtest_discover_tests(luma_av_unit
                    TEST_SUFFIX .Unit
                    TEST_LIST UnitTests
)
