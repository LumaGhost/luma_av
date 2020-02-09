

#ifndef LUMA_AV_RESULT_HPP
#define LUMA_AV_RESULT_HPP

extern "C" {
#include <libavutil/common.h>
#include <libavutil/error.h>
}

#include <system_error>

#include <boost/outcome.hpp>

#define LUMA_AV_OUTCOME_TRY BOOST_OUTCOME_TRY

namespace luma {
namespace av {

namespace outcome = BOOST_OUTCOME_V2_NAMESPACE;

enum class errc : int
{
  success     = 0,
  eof         = AVERROR_EOF,
  codec_not_found = 1, 
  alloc_failure = AVERROR(ENOMEM)
};

using error_code = std::error_code;

namespace detail
{
  // Define a custom error code category derived from std::error_category
  class errc_category : public std::error_category
  {
  public:
    // Return a short descriptive name for the category
    virtual const char *name() const noexcept override final { return "luma_av_errc"; }
    // Return what each enum means in text
    virtual std::string message(int errnum) const override final
    {
      constexpr auto buff_size = AV_ERROR_MAX_STRING_SIZE;
      if (errnum == 0) {
          return "success";
      }
      char err_buff[buff_size];
      auto ec = av_strerror(errnum, err_buff, buff_size);

      return std::string(err_buff, buff_size);
    }
  };
} // detail

// Declare a global function returning a static instance of the custom category
inline luma::av::detail::errc_category const& errc_category()
{
  static luma::av::detail::errc_category c;
  return c;
}

// Overload the global make_error_code() free function with our
// custom enum. It will be found via ADL by the compiler if needed.
inline std::error_code make_error_code(luma::av::errc e)
{
  return {static_cast<int>(e), errc_category()};
}

template <class T>
using result = luma::av::outcome::std_result<T, luma::av::error_code>;

namespace detail {

inline result<void> ffmpeg_code_to_result(int ffmpeg_code) {
  if (ffmpeg_code == 0) {
    return luma::av::outcome::success();
  } else {
    return luma::av::make_error_code(errc{ffmpeg_code});
  }
}

}

} // av
} // luma


namespace std
{
  // Tell the C++ 11 STL metaprogramming that enum ConversionErrc
  // is registered with the standard error code system
  template <> struct is_error_code_enum<luma::av::errc> : true_type
  {
  };
} // std

#endif // LUMA_AV_RESULT_HPP
