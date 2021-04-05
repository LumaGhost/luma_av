
#ifndef LUMA_AV_SWSCALE_HPP
#define LUMA_AV_SWSCALE_HPP

extern "C" {
#include <libswscale/swscale.h>
}


#include <concepts>
#include <functional>
#include <optional>

#include <luma_av/frame.hpp>
#include <luma_av/result.hpp>
#include <luma_av/util.hpp>


namespace luma_av {

enum class Width : int {};
enum class Height : int {};

class ScaleFilter {

};

class ScaleOpts {
    public:
    constexpr ScaleOpts() noexcept = default;
    constexpr ScaleOpts(const int width, const int height, const AVPixelFormat fmt) 
        : width_{width}, height_{height}, format_{fmt} {

    }
    constexpr ScaleOpts(const Width width, const Height height, const AVPixelFormat fmt) 
        : width_{detail::ToUnderlying(width)}, height_{detail::ToUnderlying(height)},
          format_{fmt} {

    }
    constexpr auto width() const noexcept -> int {
        return width_;
    }
    constexpr auto height() const noexcept -> int {
        return height_;
    }
    constexpr auto format() const noexcept -> AVPixelFormat {
        return format_;
    }
    private:
    int width_{};
    int height_{};
    AVPixelFormat format_{};
    std::optional<ScaleFilter> filter_;
};

class ScaleContext {

    struct SwsCtxDeleter {
        void operator()(SwsContext* ctx) const noexcept {
            sws_freeContext(ctx);
        }
    };

    using swsctx_ptr = std::unique_ptr<SwsContext, SwsCtxDeleter>;
    swsctx_ptr swsctx_;
    ScaleOpts dst_opts_;
    ScaleOpts src_opts_;

    static result<swsctx_ptr> AllocContext(const ScaleOpts& src_opts, const ScaleOpts& dst_opts,
                                     int flags, SwsFilter* srcFilter, SwsFilter* dstFilter,
                                    const double* param) {
        auto ctx = sws_getContext(src_opts.width(), src_opts.height(), src_opts.format(),
                                  dst_opts.width(), dst_opts.height(), dst_opts.format(), 
                                  flags, srcFilter, dstFilter, param);
        if (!ctx) {
            return errc::scale_init_failure;
        } else {
            return swsctx_ptr{ctx};
        }
    }	

    ScaleContext(SwsContext* ctx, ScaleOpts src_opts,
                 ScaleOpts dst_opts) : swsctx_{ctx}, 
                 src_opts_{std::move(src_opts)},
                 dst_opts_{std::move(dst_opts)}  {

    }

    public:

    static result<ScaleContext> make(const ScaleOpts& src_opts, const ScaleOpts& dst_opts) noexcept {
        LUMA_AV_OUTCOME_TRY(ctx, AllocContext(src_opts, dst_opts, 0, nullptr, nullptr, nullptr));
        return ScaleContext{ctx.release(), src_opts, dst_opts};
    };

    result<void> Scale(Frame const& input_frame, Frame& output_frame) noexcept {
        LUMA_AV_OUTCOME_TRY_FF(sws_scale(swsctx_.get(),
                                         input_frame.data(), input_frame.linesize(),
                                         src_opts_.height(), src_opts_.width(),
                                         output_frame.data(), output_frame.linesize()));
        return luma_av::outcome::success();
    }

};

class ScaleSession {
    Frame out_frame_;
    ScaleOpts dst_opts_;
    std::optional<ScaleContext> ctx_;
    ScaleSession(Frame out_frame, ScaleOpts dst_opts) : 
        out_frame_{std::move(out_frame)}, dst_opts_{dst_opts} {

    }
    ScaleSession(Frame out_frame, ScaleOpts dst_opts, ScaleContext ctx) : 
        out_frame_{std::move(out_frame)}, dst_opts_{dst_opts},
        ctx_{std::move(ctx)} {

    }
    public:
    static result<ScaleSession> make(const ScaleOpts& src_opts, const ScaleOpts& dst_opts) noexcept {
        LUMA_AV_OUTCOME_TRY(ctx, ScaleContext::make(src_opts, dst_opts));
        LUMA_AV_OUTCOME_TRY(frame, Frame::make());
        return ScaleSession{std::move(frame), dst_opts, std::move(ctx)};
    };
    static result<ScaleSession> make(const ScaleOpts& dst_opts) noexcept {
        LUMA_AV_OUTCOME_TRY(frame, Frame::make());
        return ScaleSession{std::move(frame), dst_opts};
    };


    result<std::reference_wrapper<const Frame>> Scale(Frame const& src_frame) {
        if (!ctx_) {
            LUMA_AV_OUTCOME_TRY(ctx, ScaleContext::make(
                        {src_frame.width(), 
                        src_frame.height(),
                        src_frame.pix_fmt()}, dst_opts_));
            ctx_ = std::move(ctx);
        }
        LUMA_AV_OUTCOME_TRY(ctx_->Scale(src_frame, out_frame_));
        return out_frame_;
    }
};

struct ScaleClosure {
    ScaleSession& sws;
    result<std::reference_wrapper<const Frame>> operator()(
            result<std::reference_wrapper<const Frame>> const& frame_res) noexcept {
        LUMA_AV_OUTCOME_TRY(frame, frame_res);
        return sws.Scale(frame);
    }
    result<std::reference_wrapper<const Frame>> operator()(Frame const& frame) noexcept {
        return sws.Scale(frame);
    }
};
const auto scale_view = [](ScaleSession& sws){
    return std::views::transform([&](const auto& frame) {
        return ScaleClosure{sws}(frame);
    });
};

namespace views {
const auto scale = scale_view;
} // views



} // luma_av



namespace luma_av_literals {

inline luma_av::Width operator ""_w (unsigned long long int width) noexcept {
    return luma_av::Width{static_cast<int>(width)};
}
inline luma_av::Height operator ""_h (unsigned long long int height) noexcept {
    return luma_av::Height{static_cast<int>(height)};
}

} // luma_av_literals





#endif // LUMA_AV_SWSCALE_HPP