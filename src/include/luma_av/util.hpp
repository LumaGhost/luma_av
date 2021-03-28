

#ifndef LUMA_AV_UTIL_HPP
#define LUMA_AV_UTIL_HPP


#include <concepts>
#include <functional>

#ifdef LUMA_AV_ENABLE_ASSERTION_LOG
#include <iostream>
#endif // LUMA_AV_ENABLE_ASSERTION_LOG


namespace luma_av {
namespace detail {

// based on https://github.com/microsoft/GSL/blob/v2.1.0/include/gsl/gsl_util#L57
template <std::invocable F>
class final_action {
    public:
    final_action(F f) noexcept : f_{std::move(f)} {}

    ~final_action() noexcept {
        std::invoke(std::move(f_));
    }
    final_action(final_action const&) = delete;
    final_action& operator=(final_action const&) = delete;
    final_action(final_action&&) = delete;
    final_action& operator=(final_action&&) = delete;

    private:
    F f_;
};

template <class F>
auto finally(F&& f) noexcept {
    return final_action<std::decay_t<F>>{std::forward<F>(f)};
}

#ifdef LUMA_AV_ENABLE_ASSERTION_LOG
inline [[noreturn]] void terimate(const std::source_location& location 
                                    = std::source_location::current()) noexcept {
    // https://en.cppreference.com/w/cpp/utility/source_location
    std::cerr << "luma_av assertion failure: "
              << location.file_name() << "("
              << location.line() << ":"
              << location.column() << ") `"
              << location.function_name() << "`\n";
    std::terminate();
};
#else
inline [[noreturn]] void terimate() noexcept {
    std::terminate();
};
#endif // LUMA_AV_ENABLE_ASSERTION_LOG

} // detail

// based on https://github.com/microsoft/GSL/blob/v2.1.0/include/gsl/gsl_assert#L154
#if defined(__clang__) || defined(__GNUC__)
#define LUMA_AV_LIKELY(x) __builtin_expect(!!(x), 1)
#else
#define LUMA_AV_LIKELY(x) (!!(x))
#endif // defined(__clang__) || defined(__GNUC__)

#ifdef LUMA_AV_ENABLE_ASSERTIONS
#define LUMA_AV_ASSERT(cond)
    (LUMA_AV_LIKELY(cond) ? static_cast<void>(0) : luma_av::detail::terminate())
#else
#define LUMA_AV_ASSERT(cond)
#endif // LUMA_AV_ENABLE_ASSERTIONS


class cstr_view {
    public:
    constexpr explicit cstr_view(const char* cstr) : cstr_{cstr} {}

    /*implicit*/ cstr_view(std::string const& str) : cstr_{str.c_str()} {}

    constexpr const char* c_str() const noexcept {
        return cstr_;
    }

    private:
    const char* cstr_;
};
namespace detail {
    template<class T, class U>
    concept SmartPtr = requires(U ptr) {
        { ptr.get() } -> std::convertible_to<T>;
    } && std::is_pointer_v<T>;
    template<class T>
    concept FFmpegWrapper = requires(T t) {
        { t.get() } -> std::convertible_to<typename T::ffmpeg_ptr_type>;
    };
};
template <detail::FFmpegWrapper T>
class NonOwning {
    public:
    using pointer = std::decay_t<typename T::ffmpeg_ptr_type>;
    using const_pointer = const pointer;
    /*implicit*/ NonOwning(T const& t) : ptr_{t.get()} {
    }
    // /*implicit*/ NonOwning(pointer ptr) : ptr_{ptr} {
    //     LUMA_AV_ASSERT(ptr_);
    // }
    /*implicit*/ NonOwning(const_pointer ptr) : ptr_{ptr} {
        LUMA_AV_ASSERT(ptr_);
    }
    const_pointer ptr() const noexcept {
        return ptr_;
    }
    pointer ptr() noexcept {
        return ptr_;
    }
    private:
    pointer ptr_;
};

} // luma_av

namespace luma_av_literals {

inline luma_av::cstr_view operator ""_cv (const char* cstr, std::size_t) noexcept {
    return luma_av::cstr_view{cstr};
}
} // luma_av_literals





#endif // LUMA_AV_UTIL_HPP