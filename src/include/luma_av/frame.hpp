
#ifndef LUMA_AV_FRAME_HPP
#define LUMA_AV_FRAME_HPP

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
}

#include <memory>
#include <optional>
#include <span>
#include <variant>

#include <luma_av/result.hpp>
#include <luma_av/util.hpp>

namespace luma_av {

/**
 unique ownership of a buffer allocated with avmalloc
*/
class Buffer {
    struct AVBuffDeleter {
        void operator()(uint8_t* av_ptr) {
            av_free(&av_ptr);
        }
    };
    using buffer_ptr = std::unique_ptr<uint8_t, AVBuffDeleter>;

    buffer_ptr buff_;
    std::span<uint8_t> view_;

    Buffer(uint8_t* av_ptr, const std::size_t size) : buff_{av_ptr} {
        view_ = std::span<uint8_t>{buff_.get(), size};
    }

    public:

    static result<Buffer> make(std::size_t size) noexcept {
        auto buff = static_cast<uint8_t*>(av_malloc(size));
        if (!buff) {
            return luma_av::outcome::failure(luma_av::errc::alloc_failure);
        }
        return Buffer{buff, size};
    }

    Owner<uint8_t*> release() noexcept {
        return buff_.release();
    }

    uint8_t* data() noexcept {
        return buff_.get();
    }
    const uint8_t* data() const noexcept {
        return buff_.get();
    }

    std::size_t size() const noexcept {
        return view_.size();
    }

    int ssize() const noexcept {
        return static_cast<int>(view_.size());
    }


    std::span<uint8_t> view() noexcept {
        return view_;
    }
    std::span<const uint8_t> view() const noexcept {
        return {view_.data(), view_.size()};
    }

};

struct VideoParams {
    int width_{};
    int height_{};
    AVPixelFormat format_{};

    int width() const noexcept {
        return width_;
    }
    auto& width(int w) noexcept {
        width_ = w;
        return *this;
    }

    int height() const noexcept {
        return height_;
    }
    auto& height(int h) noexcept {
        height_ = h;
        return *this;
    }

    AVPixelFormat format() const noexcept {
        return format_;
    }
    auto& format(AVPixelFormat fmt) noexcept {
        format_ = fmt;
        return *this;
    }

};

struct AudioParams {
    int nb_samples_{};
    int channel_layout_{};

    int nb_samples() const noexcept {
        return nb_samples_;
    }
    auto& nb_samples(int nb) noexcept {
        nb_samples_ = nb;
        return *this;
    }

    int channel_layout() const noexcept {
        return channel_layout_;
    }
    auto& channel_layout(int c) noexcept {
        channel_layout_ = c;
        return *this;
    }
};

using FrameBufferParams = std::variant<VideoParams, AudioParams>;

namespace detail {
inline void ApplyParams(NotNull<AVFrame*> frame, VideoParams const& par) noexcept {
    frame->width = par.width();
    frame->height = par.height();
    frame->format = par.format();
}

inline void ApplyParams(NotNull<AVFrame*> frame, AudioParams const& par) noexcept {
    frame->nb_samples = par.nb_samples();
    frame->channel_layout = par.channel_layout();
}

inline void ApplyParams(NotNull<AVFrame*> frame, FrameBufferParams const& par) noexcept {
    std::visit([&](auto&& alt){return detail::ApplyParams(frame, alt);}, par);
}

/**
    if the frame satisfies the buffer invariant return FrameBufferParams
    representing the frames parameters
    otherwise return none
*/
inline std::optional<FrameBufferParams> get_buffer_params(AVFrame const* frame) noexcept {
    // a buffer is always a requirement
    if (!frame->data) {
        return std::nullopt;
    }

    // video frame
    if (frame->linesize) {
        // its gona be callers bug if width/height/format arent set but buffers are.
        //  i cant rly safely work with those buffers without that info
        LUMA_AV_ASSERT(frame->width != 0);
        LUMA_AV_ASSERT(frame->height != 0);
        LUMA_AV_ASSERT(frame->format != 0);
        return VideoParams {.width_ = frame->width,
                            .height_ = frame->height,
                            .format_ = static_cast<AVPixelFormat>(frame->format)};
    } else { // audio frame
        LUMA_AV_ASSERT(frame->nb_samples != 0);
        LUMA_AV_ASSERT(frame->channel_layout != 0);
        return AudioParams{
                .nb_samples_ = frame->nb_samples,
                .channel_layout_ = frame->channel_layout};
    }
}

inline bool holds_video_buffer(NotNull<AVFrame const*> frame) noexcept {
    const auto buff_par = detail::get_buffer_params(frame);
    if (!buff_par) {
        return false;
    } else {
        return std::holds_alternative<VideoParams>(*buff_par);
    }
}

inline bool holds_audio_buffer(NotNull<AVFrame const*> frame) noexcept {
    const auto buff_par = detail::get_buffer_params(frame);
    if (!buff_par) {
        return false;
    } else {
        return std::holds_alternative<AudioParams>(*buff_par);
    }
}

inline bool holds_any_valid_buffer(NotNull<AVFrame const*> frame) noexcept {
    return detail::get_buffer_params(frame).has_value();
}

inline result<void> RefFrameImpl(NotNull<AVFrame*> dst, NotNull<AVFrame const*> src) noexcept {
    const auto dst_buff = detail::get_buffer_params(dst);
    const auto src_buff = detail::get_buffer_params(src);
    LUMA_AV_ASSERT(src_buff);
    if (dst_buff) {
        av_frame_unref(dst);
    }
    LUMA_AV_OUTCOME_TRY_FF(av_frame_ref(dst, src));
    detail::ApplyParams(dst, src_buff.value());
    return luma_av::outcome::success();
}
/**
https://ffmpeg.org/doxygen/trunk/group__lavu__frame.html#ga709e62bc2917ffd84c5c0f4e1dfc48f7
*/
inline void MoveFrameRefImpl(NotNull<AVFrame*> dst, NotNull<AVFrame*> src) noexcept {
    const auto dst_buff = detail::get_buffer_params(dst);
    const auto src_buff = detail::get_buffer_params(src);
    LUMA_AV_ASSERT(src_buff);
    if (dst_buff) {
        av_frame_unref(dst);
    }
    av_frame_move_ref(dst, src);
    detail::ApplyParams(dst, src_buff.value());
}
} // detail

/**
https://ffmpeg.org/doxygen/trunk/group__lavu__frame.html
*/
class Frame {


    struct frame_deleter {
        void operator()(AVFrame* frame) const noexcept {
            av_frame_free(&frame);
        }
    };

    using unique_frame = std::unique_ptr<AVFrame, frame_deleter>;

    static result<unique_frame> checked_frame_alloc() noexcept {
        auto* frame = av_frame_alloc();
        if (frame) {
            return unique_frame{frame};
        } else {
            return luma_av::make_error_code(errc::alloc_failure);
        }
    }

    Frame(AVFrame* frame) noexcept : frame_{frame} {
        
    }

    /**
    current invariant (wip):
        - frame is never null (except after move. but use after move is not supported outside of assignment)
    */
    unique_frame frame_;


    public:
    Frame(Frame const&) = delete;
    Frame& operator=(Frame const&) = delete;
    Frame(Frame&&) noexcept = default;
    Frame& operator=(Frame&&) noexcept = default;

    // https://ffmpeg.org/doxygen/trunk/group__lavu__picture.html#ga5b6ead346a70342ae8a303c16d2b3629
    result<void> alloc_buffer(FrameBufferParams const& par) noexcept {
        detail::ApplyParams(frame_.get(), par);
        av_frame_unref(frame_.get());
        LUMA_AV_OUTCOME_TRY_FF(av_frame_get_buffer(frame_.get(), default_alignment));
        return luma_av::outcome::success();
    }
    static result<Frame> make() noexcept {
        LUMA_AV_OUTCOME_TRY(frame, checked_frame_alloc());
        return Frame{frame.release()};
    }

    static constexpr auto default_alignment = int{32};

    static Frame FromOwner(AVFrame* owned_frame) noexcept {
        auto frame = Frame{owned_frame};
        return std::move(frame);
    }

    // https://ffmpeg.org/doxygen/trunk/group__lavu__frame.html#ga46d6d32f6482a3e9c19203db5877105b
    static result<Frame> make(NotNull<AVFrame const*> in_frame) noexcept {
        auto* new_frame = av_frame_clone(in_frame);
        if (!new_frame) {
            return luma_av::outcome::failure(errc::alloc_failure);
        }
        auto out_frame = Frame{new_frame};
        return std::move(out_frame);
    }

    static result<Frame> make(FrameBufferParams const& par) noexcept {
        LUMA_AV_OUTCOME_TRY(frame, Frame::make());
        LUMA_AV_OUTCOME_TRY(frame.alloc_buffer(par));
        return std::move(frame);
    }

    // https://ffmpeg.org/doxygen/trunk/group__lavu__picture.html#ga5b6ead346a70342ae8a303c16d2b3629
    // not sure of the size type yet
    result<std::size_t> ImageBufferSize() const noexcept {
        const auto video_params = this->video_params();
        auto buff_size =  av_image_get_buffer_size(video_params.format(), 
                                video_params.width(), video_params.height(), default_alignment);
        if (buff_size < 0) {
            return luma_av::errc{buff_size};
        }
        return luma_av::outcome::success(buff_size);
    }

    result<Buffer> CopyToImageBuffer() const noexcept {
        LUMA_AV_OUTCOME_TRY(size, ImageBufferSize());
        LUMA_AV_OUTCOME_TRY(buff, Buffer::make(size));
        const auto video_params = this->video_params();
        LUMA_AV_OUTCOME_TRY_FF(av_image_copy_to_buffer(
            buff.data(), buff.ssize(), frame_->data, frame_->linesize,
            video_params.format(), video_params.width(), video_params.height(),
            default_alignment
        ));
        return std::move(buff);
    }

    /**
      av_frame_ref with memory safety checks
     */
    // https://ffmpeg.org/doxygen/trunk/group__lavu__frame.html#ga88b0ecbc4eb3453eef3fbefa3bddeb7c
    result<void> RefTo(Frame& dst) const noexcept {
        return detail::RefFrameImpl(dst.get(), this->get());
    }
    result<void> RefFrom(Frame const& src) noexcept {
        return detail::RefFrameImpl(this->get(), src.get());
    }

    /**
      av_frame_move_ref with memory safety checks
     */
    void MoveRefTo(Frame& dst) noexcept {
        detail::MoveFrameRefImpl(dst.get(), this->get());
    }
    void MoveRefFrom(Frame& src) noexcept {
        detail::MoveFrameRefImpl(this->get(), src.get());
    }

    /**
        checks that the frame is writable i.e. the internal buffer is 
        allocated and has only one owner
    */
    bool IsWritable() noexcept {
        if (!detail::holds_any_valid_buffer(frame_.get())) {
            return false;
        }
        // not sure why the ffmpeg api isnt const here?
        auto res = av_frame_is_writable(frame_.get());
        return res > 0;
    }
    // https://ffmpeg.org/doxygen/trunk/group__lavu__frame.html#gadd5417c06f5a6b419b0dbd8f0ff363fd
    result<void> MakeWritable() noexcept {
        LUMA_AV_ASSERT(holds_any_valid_buffer(frame_.get()));
        LUMA_AV_OUTCOME_TRY_FF(av_frame_make_writable(frame_.get()));
        return luma_av::outcome::success();
    }

    NotNull<uint8_t**> data() noexcept {
        LUMA_AV_ASSERT(holds_any_valid_buffer(frame_.get()));
        LUMA_AV_ASSERT(this->IsWritable());
        return frame_.get()->data;
    }
    NotNull<const uint8_t* const*> data() const noexcept {
        LUMA_AV_ASSERT(holds_any_valid_buffer(frame_.get()));
        return frame_.get()->data;
    }

    // only valid on video frames (i think)
    NotNull<int*> linesize() noexcept {
        LUMA_AV_ASSERT(holds_video_buffer(frame_.get()));
        return frame_.get()->linesize;
    }
    NotNull<const int*> linesize() const noexcept {
        LUMA_AV_ASSERT(holds_video_buffer(frame_.get()));
        return frame_.get()->linesize;
    }

    int width() const noexcept {
        return this->video_params().width();
    }
    int height() const noexcept {
        return this->video_params().height();
    }
    AVPixelFormat pix_fmt() const noexcept {
        return this->video_params().format();
    }

    // dont depend on specific behavior if called on an audio frame
    VideoParams video_params() const noexcept {
        return std::get<VideoParams>(detail::get_buffer_params(frame_.get()).value());
    }
    // dont depend on specific behavior if called on a video frame
    AudioParams audio_params() const noexcept {
        return std::get<AudioParams>(detail::get_buffer_params(frame_.get()).value());
    }

    AVFrame* get() noexcept {
        return frame_.get();
    }
    const AVFrame* get() const noexcept {
        return frame_.get();
    }
    AVFrame* release() && noexcept {
        return frame_.release();
    }
    

};

} // luma_av

#endif // LUMA_AV_FRAME_HPP