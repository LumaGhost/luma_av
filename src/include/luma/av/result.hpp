

#ifndef LUMA_AV_RESULT_HPP
#define LUMA_AV_RESULT_HPP

extern "C" {
#include <libavutil/common.h>
#include <libavutil/error.h>
}


#include <system_error>

#include <boost/outcome.hpp>

// our own outcome try macro so we can let the user choose between
//  standalone and boost outcome?
#define LUMA_AV_OUTCOME_TRY BOOST_OUTCOME_TRY

// https://www.boost.org/doc/libs/develop/libs/outcome/doc/html/motivation/plug_error_code.html
// https://www.ffmpeg.org/doxygen/2.3/error_8h_source.html

namespace luma {
namespace av {

namespace outcome = BOOST_OUTCOME_V2_NAMESPACE;

// should this be named errc or error_code?
enum class errc : int
{
  success     = 0,
  eof         = AVERROR_EOF,
  codec_not_found = 1,  // make sure this doesnt clash with any existing ffmpeg codes
  alloc_failure = AVERROR(ENOMEM) // maybe no_memory is a better name?
};

// i *think* this is how beast does it
//  their own errc enum and an alias to boost error code
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
      // todo contract that ec is positive?
      // or do i really need to use erno here? :(

      return std::string(err_buff, buff_size);
    }
    // OPTIONAL: Allow generic error conditions to be compared to me
    // virtual std::error_condition default_error_condition(int c) const noexcept override final
    // {
    //   switch (static_cast<ConversionErrc>(c))
    //   {
    //   case ConversionErrc::EmptyString:
    //     return make_error_condition(std::errc::invalid_argument);
    //   case ConversionErrc::IllegalChar:
    //     return make_error_condition(std::errc::invalid_argument);
    //   case ConversionErrc::TooLong:
    //     return make_error_condition(std::errc::result_out_of_range);
    //   default:
    //     // I have no mapping for this code
    //     return std::error_condition(c, *this);
    //   }
    // }
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


// error code: more lightweight but harder to rethrow as an exception
//  honestly though outcome seems really well design i doubt its much
//  overhead compared to a regular error code
template <class T>
using result = luma::av::outcome::std_result<T, luma::av::error_code>;

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
