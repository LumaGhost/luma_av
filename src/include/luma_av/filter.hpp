#ifndef LUMA_AV_FILTER_HPP
#define LUMA_AV_FILTER_HPP


/*
https://www.ffmpeg.org/doxygen/3.3/group__libavf.html
*/

extern "C" {
#include <libavformat/avformat.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
}

#include <map>
#include <memory>

#include <luma_av/result.hpp>
#include <luma_av/util.hpp>

namespace luma_av {

class FilterGraphArgs {
    public:
    FilterGraphArgs() noexcept = default;

    FilterGraphArgs& VideoSize(Width w, Height h) noexcept {
        return VideoSize(detail::ToUnderlying(w), detail::ToUnderlying(h));
    }
    FilterGraphArgs& VideoSize(int w, int h) noexcept {
        std::stringstream s;
        s << w << "x" << h;
        return SetPair("video_size", std::move(s).str());
    }
    FilterGraphArgs& PixFormat(AVPixelFormat fmt) noexcept {
        return SetPair("pix_fmt", std::to_string(static_cast<int>(fmt)));
    }
    FilterGraphArgs& TimeBase(AVRational timebase) noexcept {
        std::stringstream s;
        s << timebase.num << "/" << timebase.den;
        return SetPair("time_base", std::move(s).str());
    }
    FilterGraphArgs& AspectRatio(AVRational aspect_ratio) noexcept {
        std::stringstream s;
        s << aspect_ratio.num << "/" << aspect_ratio.den;
        return SetPair("pixel_aspect", std::move(s).str());
    }
    FilterGraphArgs& SetPair(std::string_view key, std::string_view val) noexcept {
        arg_pairs_.insert_or_assign(std::string{key}, std::string{val});
        return *this;
    }
    auto const& container() const noexcept {
        return arg_pairs_;
    }
    private:
    std::map<std::string, std::string> arg_pairs_;
};


template <std::ranges::range ArgPairs>
std::string FormatFilterArgs(ArgPairs const& args) noexcept {
    std::stringstream ss;
    for (auto const& [key, val] : args) {
        ss << key << "=" << val << ":";
    }
    auto str = std::move(ss).str();
    if (!str.empty()) {
        // remove the last :
        str.pop_back();
    }
    return str;
}

namespace detail {
const char* null_if_empty(std::string const& s) noexcept {
    if (s.empty()) {
        return nullptr;
    } else {
        return s.c_str();
    }
}

struct FilterInOutDeleter {
    void operator()(AVFilterInOut* iof) noexcept {
        avfilter_inout_free(&iof);
    }
};

using unique_filter_inout = std::unique_ptr<AVFilterInOut, FilterInOutDeleter>;

result<unique_filter_inout> AllocFilterInOut() noexcept {
    auto iof = avfilter_inout_alloc();
    if (!iof) {
        return errc::alloc_failure;
    } else {
        return unique_filter_inout{iof};
    }
}

};

result<const AVFilter*> FindFilter(const cstr_view name) noexcept {
    auto filt = avfilter_get_by_name(name.c_str());
    if (!filt) {
        return errc::filter_not_found;
    } else {
        return filt;
    }
}

class FilterGraph {

    struct FilterGraphDeleter {
        void operator()(AVFilterGraph* fg) noexcept {
            avfilter_graph_free(&fg);
        }
    };

    using unique_filter_graph = std::unique_ptr<AVFilterGraph, FilterGraphDeleter>;

    static result<unique_filter_graph> AllocFilterGraph() noexcept {
        auto fg = avfilter_graph_alloc();
        if (!fg) {
            return errc::alloc_failure;
        }  else {
            return unique_filter_graph(fg);
        }
    }

    AVFilterContext* buffersink_ctx_ = nullptr;
    AVFilterContext* buffersrc_ctx_ = nullptr;
    unique_filter_graph fg_;
    FilterGraph(AVFilterGraph* fg) : fg_{fg} {}
    public:

    static result<FilterGraph> make() noexcept {
        LUMA_AV_OUTCOME_TRY(fg, AllocFilterGraph());
        return FilterGraph{fg.release()};
    }

    result<void> CreateSrcFilter(NotNull<const AVFilter*> filter, const cstr_view name, 
                                 FilterGraphArgs const& args = FilterGraphArgs{}) noexcept {
        auto arg_str = luma_av::FormatFilterArgs(args.container());
        LUMA_AV_OUTCOME_TRY_FF(avfilter_graph_create_filter(&buffersrc_ctx_, filter, name.c_str(),
                                       detail::null_if_empty(arg_str), nullptr, fg_.get())); 
        return luma_av::outcome::success();
    }
    result<void> CreateSinkFilter(NotNull<const AVFilter*> filter, const cstr_view name, 
                                 FilterGraphArgs const& args = FilterGraphArgs{}) noexcept {
        auto arg_str = luma_av::FormatFilterArgs(args.container());
        LUMA_AV_OUTCOME_TRY_FF(avfilter_graph_create_filter(&buffersink_ctx_, filter, name.c_str(),
                                       detail::null_if_empty(arg_str), nullptr, fg_.get())); 
        return luma_av::outcome::success();
    }

    result<void> SetSinkFilterFormats(std::span<const AVPixelFormat> fmts) {
        LUMA_AV_OUTCOME_TRY_FF(av_opt_set_int_list(buffersink_ctx_, "pix_fmts", fmts.data(),
                                                   AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN));
        return luma_av::outcome::success();
    }

    result<void> FinalizeConfig(const cstr_view filters_descr) noexcept {
        LUMA_AV_OUTCOME_TRY(inputs, detail::AllocFilterInOut());
        LUMA_AV_OUTCOME_TRY(outputs, detail::AllocFilterInOut());
        
        outputs->name       = av_strdup("in");
        outputs->filter_ctx = buffersrc_ctx_;
        outputs->pad_idx    = 0;
        outputs->next       = nullptr;

        inputs->name       = av_strdup("out");
        inputs->filter_ctx = buffersink_ctx_;
        inputs->pad_idx    = 0;
        inputs->next       = nullptr;

        auto iptr = inputs.get();
        auto optr = outputs.get();
        LUMA_AV_OUTCOME_TRY_FF(avfilter_graph_parse_ptr(fg_.get(), filters_descr.c_str(),
                                    &iptr, &optr, nullptr));
        // the parse call may reseat these to something we have to free
        inputs.reset(iptr);
        outputs.reset(optr);

        LUMA_AV_OUTCOME_TRY_FF(avfilter_graph_config(fg_.get(), nullptr));

        return luma_av::outcome::success();
    }

    AVFilterContext* src_context() noexcept {
        return buffersrc_ctx_;
    }

    AVFilterContext* sink_context() noexcept {
        return buffersink_ctx_;
    }

};

} // luma_av

#endif // LUMA_AV_FILTER_HPP