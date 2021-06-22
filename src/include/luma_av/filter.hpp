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

#include <luma_av/frame.hpp>
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

        // the parse call may take ownership
        auto iptr = inputs.release();
        auto optr = outputs.release();
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

class FilterSession {
    public:

    static result<FilterSession> make(FilterGraph graph) noexcept {
        LUMA_AV_OUTCOME_TRY(frame, Frame::make());
        return FilterSession{std::move(frame), std::move(graph)};
    }

    result<void> AddSrcFrame(Frame& frame) noexcept {
        // assert that we have buffers. calling without buffers is a bug afaik
        LUMA_AV_OUTCOME_TRY_FF(av_buffersrc_add_frame_flags(graph_.src_context(),
                                             frame.get(), AV_BUFFERSRC_FLAG_KEEP_REF));
        return luma_av::outcome::success();
    } 

    result<void> MarkEOF() noexcept {
        LUMA_AV_OUTCOME_TRY_FF(av_buffersrc_add_frame_flags(graph_.src_context(),
                                             nullptr, AV_BUFFERSRC_FLAG_KEEP_REF));
        return luma_av::outcome::success();
    } 

    result<NotNull<Frame*>> GetSinkFrame() noexcept {
        LUMA_AV_OUTCOME_TRY_FF(av_buffersink_get_frame(graph_.sink_context(), frame_.get()));
        return std::addressof(frame_);
    }  

    private:
    FilterSession(Frame frame, FilterGraph graph) noexcept
        : frame_{std::move(frame)}, graph_{std::move(graph)} {}
    Frame frame_;
    FilterGraph graph_;
};

namespace detail {

// overloads for all of the different types of frames we support 
//  from the input range
struct AddFrameClosure {
    FilterSession& filter_;
    result<void> operator()(NotNull<Frame*> frame) noexcept {
        return filter_.AddSrcFrame(*frame);
    }
    result<void> operator()(Frame& frame) noexcept {
        return filter_.AddSrcFrame(frame);
    }
    result<void> operator()(result<Frame>& frame_res) noexcept {
        LUMA_AV_OUTCOME_TRY(frame, frame_res);
        return filter_.AddSrcFrame(frame);
    }
    result<void> operator()(result<NotNull<Frame*>> const& frame_res) noexcept {
        LUMA_AV_OUTCOME_TRY(frame, frame_res);
        return filter_.AddSrcFrame(*frame);
    }
    result<void> operator()(result<std::reference_wrapper<const Frame>> frame_res) noexcept {
        LUMA_AV_OUTCOME_TRY(frame, frame_res);
        // ill get rid of this overload soon (:
        return filter_.AddSrcFrame(const_cast<Frame&>(frame.get()));
    }
};


// i dont understand why these specific concepts
template <std::ranges::view R>
class filter_graph_view_impl : public std::ranges::view_interface<filter_graph_view_impl<R>> {
public:
filter_graph_view_impl() noexcept = default;
explicit filter_graph_view_impl(R base, FilterSession& filter) 
    : base_{std::move(base)}, filter_{std::addressof(filter)} {

}

// think these accessors are specific to this view and can be whatever i want?
auto base() const noexcept -> R {
    return base_;
}
auto filter() const noexcept -> FilterSession& {
    return *filter_;
}

// i dont think our view can be const qualified but im leaving these for now just in case
auto begin() {
    return iterator<false>{*this};
}
// think we can const qualify begin and end if the underlying range can
// auto begin() const {
//     return iterator<true>{*this};
// }

// we steal the end sentinel from the base range and use our operator== to handle
//  deciding when to end. we should prob have our own sentinel to avoid extra overloads
//  and potential caveats but idk how
auto end() {
    return std::ranges::end(base_);
}
// auto end() const {
//     return std::ranges::end(base_);
// }

private:
R base_{};
FilterSession* filter_ = nullptr;

template <bool is_const>
class iterator;

};

// not sure exactly why u do the guide this way but ik tldr "all_view makes this work with more types nicely"
//  also ofc we dont want to generate a bunch of classes for all the different forwarding refs so we strip that
template <std::ranges::viewable_range R>
filter_graph_view_impl(R&&, FilterSession&) -> filter_graph_view_impl<std::ranges::views::all_t<R>>;


template <std::ranges::view R>
template <bool is_const>
class filter_graph_view_impl<R>::iterator {
    using output_type = result<NotNull<Frame*>>;
    using parent_t = detail::MaybeConst_t<is_const, filter_graph_view_impl<R>>;
    using base_t = detail::MaybeConst_t<is_const, R>;
    friend iterator<not is_const>;

    parent_t* parent_ = nullptr;
    mutable std::ranges::iterator_t<base_t> current_{};
    mutable bool reached_eof_ = false;
    mutable std::ptrdiff_t skip_count_{0};
    mutable std::optional<output_type> cached_frame_;

public:

// using difference_type = std::ranges::range_difference_t<base_t>;
using difference_type = std::ptrdiff_t;
// not sure what our value type is. think its the frame reference from out of the decoder
using value_type = output_type;
// uncommenting causes a build failure oops ???
// using iterator_category = std::input_iterator_tag;

iterator() = default;

explicit iterator(parent_t& parent) 
 : parent_{std::addressof(parent)},
    current_{std::ranges::begin(parent.base_)} {
}

template <bool is_const_other = is_const>
explicit iterator(iterator<is_const_other> const& other)
requires is_const and std::convertible_to<std::ranges::iterator_t<R>, std::ranges::iterator_t<base_t>>
: parent_{other.parent_}, current_{other.current_} {
}

std::ranges::iterator_t<base_t> base() const {
    return current_;
}

// note const as in *it == *it. not const as in thread safe
output_type operator*() const {
    // means deref end/past the end if this hits
    LUMA_AV_ASSERT(!reached_eof_);
    // i fucked up my counter logic if this fires
    LUMA_AV_ASSERT(skip_count_ >= -1);
    // if the user hasnt incremented we just want to return the last frame if there is one
    //  that way *it == *it is true
    if ((skip_count_== -1) && (cached_frame_)) {
        return *cached_frame_;
    }
    auto& filt = parent_->filter();
    for(;current_!=std::ranges::end(parent_->base_); ++current_) {
        LUMA_AV_OUTCOME_TRY(detail::AddFrameClosure{filt}(*current_));
        if (auto res = filt.GetSinkFrame()) {
            if (skip_count_ <= 0) {
                auto out = output_type{res.value()};
                cached_frame_ = out;
                skip_count_ = -1;
                ++current_;
                return out;
            } else {
                skip_count_ -= 1;
                continue;
            }
        } else if (res.error().value() == AVERROR(EAGAIN)) {
            continue;
        } else {
            reached_eof_ = true;
            return res.error();
        }
    }
    reached_eof_ = true;
    return errc::detail_filter_range_end;
}

iterator& operator++() {
    skip_count_ += 1;
    return *this;
}

void operator++(int) {
    ++*this;
}

// dont rly understand this but following along
// ig that if we have a forward range we need to actually return an iterator
// but why not just return *this. why is temp made first?
iterator operator++(int) 
requires std::ranges::forward_range<base_t> {
    auto temp = *this;
    ++*this;
    return temp;
}

bool operator==(std::ranges::sentinel_t<base_t> const& other) const {
    return reached_eof_;
}

bool operator==(iterator const& other) const 
requires std::equality_comparable<std::ranges::iterator_t<base_t>> {
    auto filter_or_null = [](auto parent) -> FilterSession* {
        if (parent) {
            return parent->filter_;
        } else {
            return nullptr;
        }
    };
    return filter_or_null(parent_) == filter_or_null(other.parent_) &&
        current_ == other.current_ &&
        reached_eof_ == other.reached_eof_ &&
        skip_count_ == other.skip_count_ &&
        cached_frame_.has_value() == other.cached_frame_.has_value();
}

};

inline const auto filter_range_end_view = std::views::filter([](const auto& res){
    if (res) {
        return true;
    } else if (res.error() == errc::detail_filter_range_end) {
        return false;
    } else {
        return true;
    }
});

inline const auto filter_range_end = filter_range_end_view;


template <class F>
class filter_graph_view_impl_range_adaptor_closure {
    F f_;
    FilterSession* filter_;
    public:
    filter_graph_view_impl_range_adaptor_closure(F f, FilterSession& filt) 
        : f_{std::move(f)}, filter_{std::addressof(filt)} {

    }
    template <std::ranges::viewable_range R>
    decltype(auto) operator()(R&& r) {
        return std::invoke(f_, std::forward<R>(r), *filter_);
    }

    template <std::ranges::viewable_range R>
    decltype(auto) operator()(R&& r) const {
         return std::invoke(f_, std::forward<R>(r), *filter_);
    }
};

template <class F, std::ranges::viewable_range R>
decltype(auto) operator|(R&& r, filter_graph_view_impl_range_adaptor_closure<F> const& closure) {
    return closure(std::forward<R>(r));
}


class filter_graph_view_impl_fn {
public:
template <class R>
auto operator()(R&& r, FilterSession& filt) const {
    // idk why the deduction guide doesnt work 
    return filter_graph_view_impl{
        std::views::all(std::forward<R>(r)), filt} | filter_range_end;
}
auto operator()(FilterSession& filt) const {
    return filter_graph_view_impl_range_adaptor_closure<
            filter_graph_view_impl_fn>{filter_graph_view_impl_fn{}, filt};
}
};

} // detail

inline const auto filter_graph_view = detail::filter_graph_view_impl_fn{};

namespace views {
inline const auto filter_graph = filter_graph_view;

} // views

} // luma_av

#endif // LUMA_AV_FILTER_HPP