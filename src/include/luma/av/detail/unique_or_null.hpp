#ifndef LUMA_AV_DETAIL_UNIQUE_OR_NULL_HPP
#define LUMA_AV_DETAIL_UNIQUE_OR_NULL_HPP

#include <memory>

namespace luma {
namespace av {
namespace detail {


template <class T, class Deleter>
class unique_or_null {

    public:
    using pointer = T*;
    using element_type = T;
    using deleter_type = Deleter;
    using unique_ptr_type = std::unique_ptr<element_type, deleter_type>;

    unique_or_null(std::nullptr_t) noexcept : ptr_{nullptr} {

    }

    /**
     * construct from raw owning ptr
     */
    unique_or_null(T* ptr) : ptr_{ptr} {}

    unique_or_null& operator=(std::nullptr_t) noexcept {
        ptr_ = nullptr;
        return *this;
    }

    unique_or_null(const unique_or_null&) = delete;
    unique_or_null& operator=(const unique_or_null&) = delete;
    unique_or_null(unique_or_null&&) = default;
    unique_or_null& operator=(unique_or_null&&) = default;

    explicit operator bool() const noexcept {
        return bool{ptr_};
    }

    friend bool operator==(const unique_or_null& ptr, std::nullptr_t) noexcept {
        return ptr.ptr_ == nullptr;
    }
    friend bool operator==(std::nullptr_t, const unique_or_null& ptr) noexcept {
        return ptr.ptr_ == nullptr;
    }
    friend bool operator!=(const unique_or_null& ptr, std::nullptr_t) noexcept {
        return ptr.ptr_ != nullptr;
    }
    friend bool operator!=(std::nullptr_t, const unique_or_null& ptr) noexcept {
        return ptr.ptr_ != nullptr;
    }

    void reset(pointer ptr = nullptr) noexcept {
        ptr_.reset(ptr);
    }
    pointer release() noexcept {
        return ptr_.release();
    }

    pointer get() noexcept {
        return ptr_.get();
    }

    const pointer get() const noexcept {
        return ptr_.get();
    }

    private:
    unique_ptr_type ptr_;
};


} // detail
} // av
} // luma



#endif // LUMA_AV_DETAIL_UNIQUE_OR_NULL_HPP