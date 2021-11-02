

#ifndef LUMA_AV_RESULT_HPP
#define LUMA_AV_RESULT_HPP

extern "C" {
#include <libavutil/common.h>
#include <libavutil/error.h>
}

#include <system_error>

#include <3rd_party/outcome/2.1.5/outcome.hpp>

#define LUMA_AV_OUTCOME_TRY OUTCOME_TRY

namespace luma_av {

#ifdef OUTCOME_V2_NAMESPACE
namespace outcome = OUTCOME_V2_NAMESPACE;
#else
#error "outcome namespace not defined"
#endif

enum class errc : int
{
  success     = 0,
  eof         = AVERROR_EOF,
  codec_not_found = 1, 
  alloc_failure = AVERROR(ENOMEM),
  scale_init_failure,
  decode_range_end,
  detail_reader_range_end,
  parser_not_found,
  parser_hungry_uwu,
  detail_parser_range_end,
  detail_filter_range_end,
  filter_not_found,
  end, // no more data will be sent
  again // no data yet but if u send again u may get some
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
      if (ec != 0) {
        return std::string{"luma_av: error ffmpeg api: "}.append("unknown error code: ").append(std::to_string(errnum));
      }
      auto av_msg = std::string(err_buff, buff_size);
      return std::string{"luma_av: error ffmpeg api: "}.append(std::move(av_msg));
    }
  };
} // detail

// Declare a global function returning a static instance of the custom category
inline luma_av::detail::errc_category const& errc_category() noexcept {
  static luma_av::detail::errc_category c;
  return c;
}

// Overload the global make_error_code() free function with our
// custom enum. It will be found via ADL by the compiler if needed.
inline std::error_code make_error_code(luma_av::errc e) noexcept {
  return {static_cast<int>(e), errc_category()};
}

#ifdef LUMA_AV_EXCEPTION_NOVALUE_POLICY
template <class T>
using result = luma_av::outcome::std_result<T, luma_av::error_code>;
#else 
template <class T>
using result = luma_av::outcome::std_result<T, luma_av::error_code, luma_av::outcome::policy::terminate>;
#endif // LUMA_AV_NOEXCEPT_NOVALUE_POLICY


#ifdef LUMA_AV_NOEXCEPT_STDLIB
inline constexpr auto noexcept_stdlib = true;
#else 
inline constexpr auto noexcept_stdlib = false;
#endif // LUMA_AV_NOEXCEPT_STDLIB


namespace detail {

inline result<void> as_result(int ffmpeg_code) noexcept {
  if (ffmpeg_code < 0) {
    return luma_av::make_error_code(luma_av::errc{ffmpeg_code});
  } else {
    return luma_av::outcome::success();
  }
}

inline result<void> ffmpeg_code_to_result(int ffmpeg_code) noexcept {
  return detail::as_result(ffmpeg_code);
}

} // detail

#define LUMA_AV_OUTCOME_TRY_FF(statement) \
LUMA_AV_OUTCOME_TRY(std::invoke([&]()->luma_av::result<void> {  \
    return luma_av::detail::ffmpeg_code_to_result(statement);  \
})) 
/**
and if u want the result for some reason i say just call ffmpeg_code_to_result urself
i dont like the boilerplate of always having to make a function just to wrap that. 
the try macro helps a lot. and for other scenarios i think the function is fine. not even
a macro can simplify that for u. we could make the name shorter though lmao. like just as_result
i like this approach over always making wrappers cause it saves a lot of function boilerplate
(not just to write but to understand and mantain. esp when theyre all actually doing the samet hing)
but also cause the ffmpeg call is more direct
*/


} // luma_av


namespace std
{
  // Tell the C++ 11 STL metaprogramming that enum ConversionErrc
  // is registered with the standard error code system
  template <> struct is_error_code_enum<luma_av::errc> : true_type
  {
  };
} // std

#endif // LUMA_AV_RESULT_HPP
