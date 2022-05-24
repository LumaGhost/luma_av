
#include <gtest/gtest.h>

#include <iostream>
#include <luma_av/result.hpp>

using namespace luma_av;

TEST(result, outcome_example) {
  std::error_code ec = luma_av::errc::success;

  std::cout << "printed by std::error_code as " << ec
            << " with explanatory message " << ec.message() << std::endl;

  // We can compare ConversionErrc containing error codes to
  // generic conditions
  std::cout << "ec is equivalent to std::errc::invalid_argument = "
            << (ec == std::errc::invalid_argument) << std::endl;
  std::cout << "ec is equivalent to std::errc::result_out_of_range = "
            << (ec == std::errc::result_out_of_range) << std::endl;

  // can we compare to ints (ffmpeg error codes?)
  ASSERT_EQ(ec.value(), 0);
}

TEST(result, success) {
  auto r = result<void>{luma_av::outcome::success()};
  ASSERT_TRUE(r);
  r.value();
}

TEST(result, ffmpeg_code) {
  auto r = result<void>{errc{AVERROR_EOF}};
  ASSERT_FALSE(r);
  ASSERT_DEATH(r.value(), "");
}

TEST(result, ffmpeg_success) {
  auto r = detail::ffmpeg_code_to_result(0);
  ASSERT_TRUE(r);
  r.value();
}