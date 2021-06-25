

#include <luma/av/detail/unique_or_null.hpp>

#include <gtest/gtest.h>

using namespace luma::av::detail;

namespace {

// test from the perspective of a derived type
//   since the main purpose of this class is to add
//   functionality to a derived type
struct test_ptr : unique_or_null<std::string, std::default_delete<std::string>> {};

} // anon

/**
*/
TEST(unique_or_null, null_state_non_const) {
    auto ptr = test_ptr{nullptr};
    ASSERT_FALSE(ptr);
    ASSERT_FALSE(ptr != nullptr);
    ASSERT_FALSE(nullptr != ptr);
    ASSERT_EQ(ptr, nullptr);
    ASSERT_EQ(nullptr, ptr);
    ASSERT_EQ(ptr.get(), nullptr);
}

/**
*/
TEST(unique_or_null, null_state_const) {
    const auto ptr = test_ptr{nullptr};
    ASSERT_FALSE(ptr);
    ASSERT_FALSE(ptr != nullptr);
    ASSERT_FALSE(nullptr != ptr);
    ASSERT_EQ(ptr, nullptr);
    ASSERT_EQ(nullptr, ptr);
    ASSERT_EQ(ptr.get(), nullptr);
}