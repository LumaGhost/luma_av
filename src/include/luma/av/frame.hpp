
#ifndef LUMA_AV_FRAME_HPP
#define LUMA_AV_FRAME_HPP

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
}

#include <memory>

#include <luma/av/result.hpp>
#include <luma/av/detail/unique_or_null.hpp>

namespace luma {
namespace av {

namespace detail {

struct frame_deleter {
    void operator()(AVFrame* frame) const noexcept {
        av_frame_free(&frame);
    }
};

using unique_or_null_frame = detail::unique_or_null<AVFrame, detail::frame_deleter>;

} // detail

// todo static in cpp
inline result<detail::unique_or_null_frame> checked_frame_alloc() noexcept {
    auto* frame = av_frame_alloc();
    if (frame) {
        return detail::unique_or_null_frame{frame};
    } else {
        return luma::av::make_error_code(errc::alloc_failure);
    }
}


template <int alignment>
class basic_frame : public detail::unique_or_null_frame {

    public:
    using base_type = detail::unique_or_null<AVFrame,
                                             detail::frame_deleter>;
    using base_type::get;
    public:

    basic_frame() noexcept(luma::av::noexcept_novalue) 
        : base_type{checked_frame_alloc().value()} {}

    /**
     * does a deep copy of the frame, validates the invariant with a contract.
     */
    explicit basic_frame(const AVFrame*) noexcept(luma::av::noexcept_novalue && luma::av::noexcept_contracts);

    // we can write them but i think implict copy is too expensive to be implicit
    basic_frame(const basic_frame&) = delete;
    basic_frame& operator=(const basic_frame&) = delete;

    basic_frame(basic_frame&& other) noexcept = default;
    basic_frame& operator=(basic_frame&& other) noexcept = default;


    int64_t best_effort_timestamp() const noexcept {
        return av_frame_get_best_effort_timestamp(this->get());
    }

    basic_frame& best_effort_timestamp(int64_t val) const noexcept {
        av_frame_set_best_effort_timestamp(this->get(), val);
        return *this;
    }

    int width() const noexcept {
        return this->get()->width;
    }
    int height() const noexcept {
        return this->get()->height;
    }
    int nb_samples() const noexcept {
        return this->get()->width;
    }
    int channel_layout() const noexcept {
        return this->get()->height;
    }
    int format() const noexcept {
        return this->get()->format;
    }

    result<void> alloc_buffers(int width, int height, int format) noexcept {
        av_frame_unref(this->get());
        this->get()->width = width;
        this->get()->height = height;
        this->get()->format = format;
        return detail::ffmpeg_code_to_result(av_frame_get_buffer(this->get(), alignment));
    }

    int num_planes() const noexcept {
        auto result = int{av_pix_fmt_count_planes(this->get()->format)};
        return result;
    }

    uint8_t* data(int idx) const noexcept(luma::av::noexcept_contracts) {
        assert(idx > 0 && idx < num_planes());
        return this->get()->data[idx];
    }

    int linesize(int idx) const noexcept(luma::av::noexcept_contracts) {
        assert(idx > 0 && idx < num_planes());
        return this->get()->linesize[idx];
    }
};

using frame = basic_frame<32>;

template <int align>
result<void> is_writable(basic_frame<align>& f) noexcept {
    // why isnt this const?
    auto ec = av_frame_is_writable(f.get());
    return detail::ffmpeg_code_to_result(ec);
}

template <int align>
result<void> make_writable(basic_frame<align>& f) noexcept {
    auto ec = av_frame_make_writable(f.get());
    return detail::ffmpeg_code_to_result(ec);
}

template <int align1, int align2>
result<void> copy_frame_props(basic_frame<align1>& dst, const basic_frame<align2>& src) noexcept {
    return detail::ffmpeg_code_to_result(av_frame_copy_props(dst.get(), src.get()));
}

template <int align>
result<void> copy_frame(basic_frame<align>& dst, const basic_frame<align>& src) noexcept {
    LUMA_AV_OUTCOME_TRY(luma::av::copy_frame_props(dst, src));
    auto ec = av_frame_get_buffer(dst.get(), align);
    return detail::ffmpeg_code_to_result(ec);
}

template <int align>
result<void> ref_frame(basic_frame<align>& dst, const basic_frame<align>& src) noexcept {
    LUMA_AV_OUTCOME_TRY(luma::av::copy_frame_props(dst, src));
    av_frame_unref(dst.get());
    return detail::ffmpeg_code_to_result(av_frame_ref(dst.get(), src.get()));
}

} // av
} // luma

#endif // LUMA_AV_FRAME_HPP