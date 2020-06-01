

#ifndef LUMA_AV_RESULT_HPP
#define LUMA_AV_RESULT_HPP

extern "C" {
#include <libavutil/common.h>
#include <libavutil/error.h>
}

#include <system_error>

#include <boost/outcome.hpp>
#ifdef LUMA_AV_NOEXCEPT_NOVALUE_POLICY
#include <boost/policy/terminate.hpp>
#endif // LUMA_AV_NOEXCEPT_NOVALUE_POLICY

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
      if (errnum == 0) {
          return "success";
      }
      constexpr auto buff_size = AV_ERROR_MAX_STRING_SIZE;
      char err_buff[buff_size];
      auto ec = av_strerror(errnum, err_buff, buff_size);
      return std::string(err_buff, buff_size);
    }
  };
} // detail

// Declare a global function returning a static instance of the custom category
inline luma::av::detail::errc_category const& errc_category() noexcept {
  static luma::av::detail::errc_category c;
  return c;
}

// Overload the global make_error_code() free function with our
// custom enum. It will be found via ADL by the compiler if needed.
inline std::error_code make_error_code(luma::av::errc e) noexcept {
  return {static_cast<int>(e), errc_category()};
}

#ifdef LUMA_AV_NOEXCEPT_NOVALUE_POLICY
template <class T>
using result = luma::av::outcome::std_result<T, luma::av::error_code, luma::av::outcome::policy::terminate>;
inline constexpr auto noexcept_novalue = true;
#else 
template <class T>
using result = luma::av::outcome::std_result<T, luma::av::error_code>;
inline constexpr auto noexcept_novalue = false;
#endif // LUMA_AV_NOEXCEPT_NOVALUE_POLICY


#ifdef LUMA_AV_NOEXCEPT_STDLIB
inline constexpr auto noexcept_stdlib = true;
#else 
inline constexpr auto noexcept_stdlib = false;
#endif // LUMA_AV_NOEXCEPT_STDLIB

#ifdef LUMA_AV_NOEXCEPT_CONTRACTS
inline constexpr auto noexcept_contracts = true;
#else 
inline constexpr auto noexcept_contracts = false;
#endif // LUMA_AV_NOEXCEPT_CONTRACTS


namespace detail {

inline result<void> ffmpeg_code_to_result(int ffmpeg_code) noexcept {
  if (ffmpeg_code == 0) {
    return luma::av::outcome::success();
  } else {
    return luma::av::make_error_code(luma::av::errc{ffmpeg_code});
  }
}

} // detail

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
