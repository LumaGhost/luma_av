

#ifndef LUMA_AV_UTIL_HPP
#define LUMA_AV_UTIL_HPP


#include <concepts>
#include <functional>

#ifdef LUMA_AV_ENABLE_ASSERTION_LOG
#include <iostream>
#endif // LUMA_AV_ENABLE_ASSERTION_LOG


namespace luma_av {
namespace detail {

template <std::invocable F>
class final_action {
    public:
    final_action(F f) noexcept : f_{std::move(f)} {}

    ~final_action() noexcept {
        std::invoke(std::move(f_));
    }

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
    std::cerr << "assertion failure: "
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


} // luma_av





#endif // LUMA_AV_UTIL_HPP