
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

#include <luma/av/result.hpp>

namespace luma {
namespace av {

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

    result<Buffer> make(std::size_t size) noexcept {
        auto buff = static_cast<uint8_t*>(av_malloc(size));
        if (!buff) {
            return luma::av::outcome::failure(luma::av::errc::alloc_failure);
        }
        return Buffer{buff, size};
    }

    uint8_t* data() noexcept {
        return buff_.get();
    }

    std::size_t size() const noexcept {
        return view_.size();
    }

    int ssize() const noexcept {
        return static_cast<int>(view_.size());
    }


    std::span<uint8_t>& view() noexcept {
        return view_;
    }

};

/**
https://ffmpeg.org/doxygen/trunk/group__lavu__frame.html
*/
template <int alignment>
class basic_frame {


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
            return luma::av::make_error_code(errc::alloc_failure);
        }
    }

    basic_frame(AVFrame* frame) noexcept : frame_{frame} {

    }
    using This = basic_frame<alignment>;

    struct VideoParams {
        int width{};
        int height{};
        int format{};
    };

    // would have used a free friend but fuck adl
    void ApplyParams(VideoParams const& par) noexcept {
        frame_->width = par.width;
        frame_->height = par.height;
        frame_->format = par.format;
    }

    struct AudioParams {
        int nb_samples{};
        int chanel_layout{};
    };

    void ApplyParams(AudioParams const& par) noexcept {
        frame_->nb_samples = par.nb_samples;
        frame_->chanel_layout = par.chanel_layout;
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



    public:

    // https://ffmpeg.org/doxygen/trunk/group__lavu__picture.html#ga5b6ead346a70342ae8a303c16d2b3629
    result<void> alloc_buffer(FrameBufferParams const& par) noexcept {
        std::visit([&](auto&& alt){this->ApplyParams(alt);}, par);
        av_frame_unref(frame_.get());
        LUMA_AV_OUTCOME_TRY_FF(av_frame_get_buffer(frame_.get(), alignment));
        buff_par_ = par;
        return luma::av::outcome::success();
    }

    static result<This> make() noexcept {
        LUMA_AV_OUTCOME_TRY(frame, checked_frame_alloc());
        return This{frame.release()};
    }

    static This FromOwner(AVFrame* owned_frame) noexcept {
        return This{owned_frame};
    }
    struct shallow_copy_t{};
    static constexpr auto shallow_copy = shallow_copy_t{};

    // https://ffmpeg.org/doxygen/trunk/group__lavu__frame.html#ga46d6d32f6482a3e9c19203db5877105b
    static result<This> make(const AVFrame* in_frame, shallow_copy_t) noexcept {
        auto* new_frame = av_frame_clone(in_frame);
        if (!new_frame) {
            return luma::av::outcome::failure(errc::alloc_failure);
        }
        return This{new_frame};
    }

    static result<This> make(FrameBufferParams const& par) noexcept {
        LUMA_AV_OUTCOME_TRY(frame, This::make());
        LUMA_AV_OUTCOME_TRY(frame.alloc_buffer(par));
        return std::move(frame);
    }

    // just gona say ub if its not actually an image
    // in practice i'll just terminate
    VideoParams const& video_params() noexcept {
        return std::get<VideoParams>(buff_par_.value());
    }

    // https://ffmpeg.org/doxygen/trunk/group__lavu__picture.html#ga5b6ead346a70342ae8a303c16d2b3629
    // not sure of the size type yet
    result<std::size_t> ImageBufferSize() const noexcept {
        auto const& video_params = video_params();
        auto buff_size =  av_image_get_buffer_size(video_params.format, 
                                video_params.width, video_params.height, alignment);
        if (buff_size < 0) {
            return detail::ffmpeg_code_to_result(buff_size);
        }
        return luma::av::outcome::success(buff_size);
    }

    result<Buffer> CopyToImageBuffer() const noexcept {
        LUMA_AV_OUTCOME_TRY(size, ImageBufferSize());
        LUMA_AV_OUTCOME_TRY(buff, Buffer::make(size));
        auto const& video_params = video_params();
        LUMA_AV_OUTCOME_TRY_FF(av_image_copy_to_buffer(
            buff.data(), buff.ssize(), frame_->data, frame_->linesize,
            video_params.format, video_params.width, video_params.height,
            alignment
        ));
        return std::move(buff);
    }

    private:
    friend result<void> RefImpl(basic_frame& dst, basic_frame const& src) {
        assert(src.buff_par_);
        if (dst.buff_par_) {
            av_frame_unref(dst.frame_.get());
        }
        LUMA_AV_OUTCOME_TRY_FF(av_frame_ref(dst.frame_.get(), src.frame_.get()));
        return luma::av::outcome::success();
    }
    public:

    // https://ffmpeg.org/doxygen/trunk/group__lavu__frame.html#ga88b0ecbc4eb3453eef3fbefa3bddeb7c
    result<void> RefTo(basic_frame& dst) const noexcept {
        return RefImpl(dst, *this);
    }

    result<void> RefFrom(basic_frame const& src) const noexcept {
        return RefImpl(*this, src);
    }

    /**
    https://ffmpeg.org/doxygen/trunk/group__lavu__frame.html#ga709e62bc2917ffd84c5c0f4e1dfc48f7
    */
    private:
    friend void MoveReImpl(basic_frame& dst, basic_frame& src) noexcept {
        assert(src.buff_par_);
        if (dst.buff_par_) {
            av_frame_unref(dst.frame_.get());
        }
        av_frame_move_ref(dst.frame_.get(), src.frame_.get());
    }
    public:
    void MoveRefTo(basic_frame& dst) noexcept {
        MoveReImpl(dst, *this);
    }

    void MoveRefFrom(basic_frame& src) noexcept {
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
        LUMA_AV_OUTCOME_TRY_FF(frame_.get());
        return luma::av::outcome::success();
    }

    AVFrame* get() noexcept {
        return frame_.get();
    }
    const AVFrame* get() const noexcept {
        return frame_.get();
    }

};

using frame = basic_frame<32>;

} // av
} // luma

#endif // LUMA_AV_FRAME_HPP