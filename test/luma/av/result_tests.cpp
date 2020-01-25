
#include <iostream>

#include <luma/av/result.hpp>


#include <gtest/gtest.h>

TEST(result_unit, outcome_example) {
  std::error_code ec = luma::av::errc::success;

  std::cout << "printed by std::error_code as "
    << ec << " with explanatory message " << ec.message() << std::endl;

  // We can compare ConversionErrc containing error codes to generic conditions
  std::cout << "ec is equivalent to std::errc::invalid_argument = "
    << (ec == std::errc::invalid_argument) << std::endl;
  std::cout << "ec is equivalent to std::errc::result_out_of_range = "
    << (ec == std::errc::result_out_of_range) << std::endl;

  // can we compare to ints (ffmpeg error codes?)
  ASSERT_EQ(ec.value(), AVERROR_EXIT);
}