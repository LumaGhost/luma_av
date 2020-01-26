#ifndef LUMA_AV_DETAIL_UNIQUE_OR_NULL_HPP
#define LUMA_AV_DETAIL_UNIQUE_OR_NULL_HPP


/*
best name i could think of so far
i want to signal semantics that are mostly 
    what std::unique_ptr has but some slight tweaks
    also i want to somehow try to signal that
    this also adds a null state to the derived type
*/

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

    // no default ctor, must be explicitly initialized with null
    unique_or_null(std::nullptr_t) noexcept : ptr_{nullptr} {

    }

    // this is a detail class and i dont
    //  need the function object ctors yet
    //  so im not going to make them

    // construct from raw owning ptr
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

    // other functionality that interacts with the null state
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

    // deref operator? i dont think any api uses AVCodecContext&
    //  so there would be no need for operator()*
    // operator-> could be considered i think since people definitely
    //  want to access codec context params
    //  feel like our own type safe accessors are prob a better approach
    //  than just letting the user deref since its way less encapsulaton.
    //  we have the get method but i think that will hopefully get branded
    //  as the one "youre on your own" accecss point and its geared towards
    //  advanced users. i feel like its harder to communicate that a deref
    //  operator is only for advanced users. on that note we may want to
    //  consider a name other than "get" if its only for advanced users
    //  i liked the slightly verbose naming on the frame one. pros of naming
    //  "get" is that we have a consistent interface that would allow us to
    //   use crtp to plug in the null ptr semantics on each type
    //   thats going to behave that way
    //   a big factor for me is how complex the invariant is.
    //   if there isnt much to mess up than a friendly "get"
    //   and maybe even a deref is probably fine
    //  i think the ideall is that the user doesnt really have
    //  to deal with the AVCodecContext*, but if they do i think get is enough
    //  i also dont like having operator-> without operator*

    private:
    unique_ptr_type ptr_;
};


} // detail
} // av
} // luma



#endif // LUMA_AV_DETAIL_UNIQUE_OR_NULL_HPP