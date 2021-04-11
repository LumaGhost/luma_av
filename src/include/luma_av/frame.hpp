
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

    // todo static in cpp
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
    using This = Frame;

    struct VideoParams {
        int width{};
        int height{};
        AVPixelFormat format{};
        int alignment{};
    };

    // would have used a free friend but fuck adl
    int ApplyParams(VideoParams const& par) noexcept {
        frame_->width = par.width;
        frame_->height = par.height;
        frame_->format = par.format;
        return par.alignment;
    }

    struct AudioParams {
        int nb_samples{};
        int channel_layout{};
        int alignment{};
    };

    int ApplyParams(AudioParams const& par) noexcept {
        frame_->nb_samples = par.nb_samples;
        frame_->channel_layout = par.channel_layout;
        return par.alignment;
    }

    using FrameBufferParams = std::variant<VideoParams, AudioParams>;
    /**
    current invariant (wip):
        - frame is never null (except after move. but use after move is not supported outside of assignment)
        - if the frame has buffers allocated (i.e. data and linesize initialized) then the following is true
            - buff_par_ contains either video or audio params
            - the corresponding frame fields are set to the values defined by the video or audio params object
    */
    unique_frame frame_;
    std::optional<FrameBufferParams> buff_par_;

    
    // just gona say ub if its not actually an image
    // in practice i'll just terminate
    VideoParams const& video_params() const noexcept {
        return std::get<VideoParams>(buff_par_.value());
    }

    public:
    AVFrame* get() noexcept {
        return frame_.get();
    }
    const AVFrame* get() const noexcept {
        return frame_.get();
    }

    using ffmpeg_ptr_type = const AVFrame*;
    Frame(Frame const&) = delete;
    Frame& operator=(Frame const&) = delete;
    Frame(Frame&&) noexcept = default;
    Frame& operator=(Frame&&) noexcept = default;

    // https://ffmpeg.org/doxygen/trunk/group__lavu__picture.html#ga5b6ead346a70342ae8a303c16d2b3629
    result<void> alloc_buffer(FrameBufferParams const& par) noexcept {
        const auto align = std::visit([&](auto&& alt){return this->ApplyParams(alt);}, par);
        av_frame_unref(frame_.get());
        LUMA_AV_OUTCOME_TRY_FF(av_frame_get_buffer(frame_.get(), align));
        buff_par_ = par;
        return luma_av::outcome::success();
    }

    static result<This> make() noexcept {
        LUMA_AV_OUTCOME_TRY(frame, checked_frame_alloc());
        return This{frame.release()};
    }

    // not sure i can actually do this without violating my invariant
    // how do i check for buffers etc?
    static This FromOwner(AVFrame* owned_frame) noexcept {
        return This{owned_frame};
    }
    struct shallow_copy_t{};
    static constexpr auto shallow_copy = shallow_copy_t{};

    // same here. need a way to know if we need to initialize the buffer params
    // https://ffmpeg.org/doxygen/trunk/group__lavu__frame.html#ga46d6d32f6482a3e9c19203db5877105b
    static result<This> make(const NonOwning<Frame> in_frame, shallow_copy_t) noexcept {
        auto* new_frame = av_frame_clone(in_frame.ptr());
        if (!new_frame) {
            return luma_av::outcome::failure(errc::alloc_failure);
        }
        return This{new_frame};
    }

    static result<This> make(FrameBufferParams const& par) noexcept {
        LUMA_AV_OUTCOME_TRY(frame, This::make());
        LUMA_AV_OUTCOME_TRY(frame.alloc_buffer(par));
        return std::move(frame);
    }

    // https://ffmpeg.org/doxygen/trunk/group__lavu__picture.html#ga5b6ead346a70342ae8a303c16d2b3629
    // not sure of the size type yet
    result<std::size_t> ImageBufferSize() const noexcept {
        auto const& video_params = this->video_params();
        auto buff_size =  av_image_get_buffer_size(video_params.format, 
                                video_params.width, video_params.height, video_params.alignment);
        if (buff_size < 0) {
            return luma_av::errc{buff_size};
        }
        return luma_av::outcome::success(buff_size);
    }

    result<Buffer> CopyToImageBuffer() const noexcept {
        LUMA_AV_OUTCOME_TRY(size, ImageBufferSize());
        LUMA_AV_OUTCOME_TRY(buff, Buffer::make(size));
        auto const& video_params = this->video_params();
        LUMA_AV_OUTCOME_TRY_FF(av_image_copy_to_buffer(
            buff.data(), buff.ssize(), frame_->data, frame_->linesize,
            video_params.format, video_params.width, video_params.height,
            video_params.alignment
        ));
        return std::move(buff);
    }

    private:
    friend void Unref(Frame& dst) {
        av_frame_unref(dst.frame_.get());
        dst.buff_par_ = std::nullopt;
    }
    friend result<void> RefImpl(Frame& dst, Frame const& src) {
        assert(src.buff_par_);
        if (dst.buff_par_) {
            Unref(dst);
        }
        LUMA_AV_OUTCOME_TRY_FF(av_frame_ref(dst.frame_.get(), src.frame_.get()));
        dst.buff_par_ = src.buff_par_;
        return luma_av::outcome::success();
    }
    public:

    // https://ffmpeg.org/doxygen/trunk/group__lavu__frame.html#ga88b0ecbc4eb3453eef3fbefa3bddeb7c
    result<void> RefTo(Frame& dst) const noexcept {
        return RefImpl(dst, *this);
    }

    result<void> RefFrom(Frame const& src) noexcept {
        return RefImpl(*this, src);
    }

    /**
    https://ffmpeg.org/doxygen/trunk/group__lavu__frame.html#ga709e62bc2917ffd84c5c0f4e1dfc48f7
    */
    private:
    friend void MoveReImpl(Frame& dst, Frame& src) noexcept {
        assert(src.buff_par_);
        if (dst.buff_par_) {
            Unref(dst);
        }
        av_frame_move_ref(dst.frame_.get(), src.frame_.get());
        dst.buff_par_ = src.buff_par_;
    }
    public:
    void MoveRefTo(Frame& dst) noexcept {
        MoveReImpl(dst, *this);
    }

    void MoveRefFrom(Frame& src) noexcept {
        MoveReImpl(*this, src);
    }

    /**
    currently thinking its a mistake to call these on a frame with no buffers

    also yea why isnt the ffmpeg api const here?
    */
    bool IsWritable() noexcept {
        assert(buff_par_);
        auto res = av_frame_is_writable(frame_.get());
        return res > 0;
    }
    // https://ffmpeg.org/doxygen/trunk/group__lavu__frame.html#gadd5417c06f5a6b419b0dbd8f0ff363fd
    result<void> MakeWritable() noexcept {
        assert(buff_par_);
        LUMA_AV_OUTCOME_TRY_FF(av_frame_make_writable(frame_.get()));
        return luma_av::outcome::success();
    }

    uint8_t** data() noexcept {
        assert(buff_par_);
        return frame_.get()->data;
    }

    const uint8_t* const* data() const noexcept {
        assert(buff_par_);
        return frame_.get()->data;
    }

    int* linesize() noexcept {
        assert(buff_par_);
        return frame_.get()->linesize;
    }

    const int* linesize() const noexcept {
        assert(buff_par_);
        return frame_.get()->linesize;
    }

    int width() const noexcept {
        return video_params().width;
    }
    int height() const noexcept {
        return video_params().height;
    }
    AVPixelFormat pix_fmt() const noexcept {
        return video_params().format;
    }

};

} // luma_av

#endif // LUMA_AV_FRAME_HPP