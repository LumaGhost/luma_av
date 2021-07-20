
#ifndef LUMA_AV_PARSER_HPP
#define LUMA_AV_PARSER_HPP

extern "C" {
#include <libswscale/swscale.h>
}


#include <concepts>
#include <functional>
#include <optional>

#include <luma_av/codec.hpp>
#include <luma_av/result.hpp>
#include <luma_av/util.hpp>

namespace luma_av {

class ParserContext {

struct ParserDeleter {
    void operator()(AVCodecParserContext* s) {
        av_parser_close(s);
    }
};
using unique_parser_ctx = std::unique_ptr<AVCodecParserContext, ParserDeleter>;
static result<unique_parser_ctx> InitParser(int codec_id) {
    auto parser = av_parser_init(codec_id);
    if (!parser) {
        return errc::parser_not_found;
    } else {
        return unique_parser_ctx{parser};
    }
}

unique_parser_ctx parser_;
CodecContext codec_ctx_;

ParserContext(AVCodecParserContext* parser, CodecContext codec_ctx) noexcept
    : parser_{parser}, codec_ctx_{std::move(codec_ctx)}  {

}

public:

static result<ParserContext> make(CodecContext ctx) noexcept {
    LUMA_AV_OUTCOME_TRY(parser, InitParser(ctx.codec()->id));
    return ParserContext{parser.release(), std::move(ctx)};
}
// [data out, data remaining]
std::pair<result<std::span<const uint8_t>>, std::span<const uint8_t>> ParseStep(
                                                std::span<const uint8_t> in_buff) noexcept {
    uint8_t* data_out = nullptr;
    int size_out = 0;
    const auto ret = av_parser_parse2(parser_.get(), codec_ctx_.get(), 
                                std::addressof(data_out), 
                                std::addressof(size_out),
                                in_buff.data(), in_buff.size(), AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
    if (ret < 0) {
        return {errc{ret}, in_buff};
    } else if (!(data_out) or (size_out == 0)) {
        return {errc::parser_hungry_uwu, in_buff.subspan(ret)};
    } else {
        return {std::span<uint8_t>{data_out, size_out}, in_buff.subspan(ret)};
    }
}
std::pair<result<void>, std::span<const uint8_t>> ParseStep(Packet& out_pkt, 
                                        std::span<const uint8_t> in_buff) noexcept {
    const auto [out_buff, remaining] = this->ParseStep(in_buff);
    if(!out_buff) {
        return {out_buff.error(), remaining};
    }
    if (auto res = out_pkt.reset_buffer_copy(out_buff.value()); 
            !res) {
            return {res.error(), remaining};
    } else {
        return {luma_av::outcome::success(), remaining};
    }
}

};


class Parser {
    ParserContext parser_;
    Packet out_pkt_;
    Parser(ParserContext parser, Packet pkt)
        : parser_{std::move(parser)}, out_pkt_{std::move(pkt)} {
    }
    public:
    template <class codec_id> 
    requires std::convertible_to<codec_id, cstr_view> 
        || std::convertible_to<codec_id, AVCodecID>
    static result<Parser> make(codec_id id) noexcept {
        LUMA_AV_OUTCOME_TRY(codec, find_decoder(id));
        LUMA_AV_OUTCOME_TRY(codec_ctx, CodecContext::make(codec));
        LUMA_AV_OUTCOME_TRY(parser_ctx, ParserContext::make(std::move(codec_ctx)));
        LUMA_AV_OUTCOME_TRY(pkt, Packet::make());
        return Parser{std::move(parser_ctx), std::move(pkt)};
    }

    static result<Parser> make(ParserContext parser_ctx) noexcept {
        LUMA_AV_OUTCOME_TRY(pkt, Packet::make());
        return Parser{std::move(parser_ctx), std::move(pkt)};
    }

    std::pair<result<std::reference_wrapper<const Packet>>, 
        std::span<const uint8_t>> ParseStep(std::span<const uint8_t> in_buff) noexcept { 
        const auto [res, buff] = parser_.ParseStep(out_pkt_, in_buff);
        if (res) {
            return {luma_av::outcome::success(std::cref(out_pkt_)), buff};
        } else {
            return {res.error(), buff};
        }
    }
};

template <class OutputIt>
std::pair<result<void>, std::span<const uint8_t>> ParseAll(Parser& parser, 
                        std::span<const uint8_t> in_buff, OutputIt output) {
    auto parse_me_uwu = in_buff;
    while (true) {
        const auto [pkt, buff] = parser.ParseStep(parse_me_uwu);
        if (pkt) {
            *output = Packet::make(pkt.value().get(), Packet::shallow_copy);
        } else if (pkt.error() == errc::parser_hungry_uwu) {
            parse_me_uwu = buff;
        } else {
            return {pkt.error(), buff};
        }
        if (buff.empty()) {
            return {luma_av::outcome::success(), buff};
        }
    }
    
}


namespace detail {

template <std::ranges::view R>
class parser_view_impl : public std::ranges::view_interface<parser_view_impl<R>> {
public:
parser_view_impl() noexcept = default;
explicit parser_view_impl(R base, Parser& parser) 
    : base_{std::move(base)}, parser_{std::addressof(parser)} {

}

// think these accessors are specific to this view and can be whatever i want?
auto base() const noexcept -> R {
    return base_;
}
auto parser() const noexcept -> Parser& {
    return *parser_;
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
Parser* parser_ = nullptr;

template <bool is_const>
class iterator;

};

// not sure exactly why u do the guide this way but ik tldr "all_view makes this work with more types nicely"
//  also ofc we dont want to generate a bunch of classes for all the different forwarding refs so we strip that
template <std::ranges::viewable_range R>
parser_view_impl(R&&, Parser&) -> parser_view_impl<std::ranges::views::all_t<R>>;


template <std::ranges::view R>
template <bool is_const>
class parser_view_impl<R>::iterator {
    using output_type = result<std::reference_wrapper<const Packet>>;
    using parent_t = detail::MaybeConst_t<is_const, parser_view_impl<R>>;
    using base_t = detail::MaybeConst_t<is_const, R>;
    friend iterator<not is_const>;

    parent_t* parent_ = nullptr;
    mutable std::ranges::iterator_t<base_t> current_{};
    mutable bool done_parsing_uwu_{false};
    mutable std::ptrdiff_t skip_count_{0};
    mutable std::span<const uint8_t> current_span_;
    mutable std::optional<output_type> cached_packet_;

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

// if the ffmpeg parser refuses to read all of ur data (i.e. return an empty span in the end)
//  this will loop forever and i cant do anything abt that sorry but i dont think the ffmpeg parser would do that
// yea like that applies to all of these. if ffmpeg never returns neither will i
// note const as in *it == *it. not const as in thread safe
output_type operator*() const {
    // means deref end/past the end if this hits
    LUMA_AV_ASSERT(!done_parsing_uwu_);
    // i fucked up my counter logic if this fires
    LUMA_AV_ASSERT(skip_count_ >= -1);
    // if the user hasnt incremented we just want to return the last frame if there is one
    //  that way *it == *it is true
    if ((skip_count_== -1) && (cached_packet_)) {
        return *cached_packet_;
    }

    while (true) {
        if (current_span_.empty()) {
            // if our current span is empty and we haave no more spans left we're done
            if (current_ == std::ranges::end(parent_->base())) {
                done_parsing_uwu_ = true;
                return errc::detail_parser_range_end;
            } else {
                current_span_ = *current_;
                ++current_;
            }
        }
        const auto [pkt, next_span] = parent_->parser().ParseStep(current_span_);
        current_span_ = next_span;
        if (pkt) {
            if (skip_count_ <= 0) {
                auto out = output_type{pkt.value()};
                cached_packet_ = out;
                skip_count_ = -1;
                return out;
            } else {
                skip_count_ -= 1;
                continue;
            }
        } else if (pkt.error() == errc::parser_hungry_uwu) {
            continue;
        } else {
            done_parsing_uwu_ = true;
            return pkt.error();
        }
    }

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
    return done_parsing_uwu_;
}

bool operator==(iterator const& other) const 
requires std::equality_comparable<std::ranges::iterator_t<base_t>> {
    auto parser_or_null = [](auto parent) -> Parser* {
        if (parent) {
            return parent->parser_;
        } else {
            return nullptr;
        }
    };
    return parser_or_null(parent_) == parser_or_null(other.parent_) &&
    current_ == other.current_ &&
    done_parsing_uwu_ == other.done_parsing_uwu_ &&
    current_span_.data() == other.current_span_.data() &&
    skip_count_ == other.skip_count_ &&
    cached_packet_.has_value() == other.cached_packet_.has_value();
}

};

inline const auto filter_parsey_error_uwu_view = std::views::filter([](const auto& res){
    if (res) {
        return true;
    } else if (res.error() == errc::detail_parser_range_end) {
        return false;
    } else {
        return true;
    }
});

inline const auto filter_parsey_error_uwu = filter_parsey_error_uwu_view;


template <class F>
class parser_range_adaptor_closure {
    F f_;
    Parser* parser_;
    public:
    parser_range_adaptor_closure(F f, Parser& parser) 
        : f_{std::move(f)}, parser_{std::addressof(parser)} {

    }
    template <std::ranges::viewable_range R>
    decltype(auto) operator()(R&& r) {
        return std::invoke(f_, std::forward<R>(r), *parser_);
    }

    template <std::ranges::viewable_range R>
    decltype(auto) operator()(R&& r) const {
         return std::invoke(f_, std::forward<R>(r), *parser_);
    }
};

template <class F, std::ranges::viewable_range R>
decltype(auto) operator|(R&& r, parser_range_adaptor_closure<F> const& closure) {
    return closure(std::forward<R>(r));
}


class parser_range_fn {
public:
template <class R>
auto operator()(R&& r, Parser& parser) const {
    return parser_view_impl{std::views::all(std::forward<R>(r)), parser} 
        | filter_parsey_error_uwu;
}
auto operator()(Parser& parser) const {
    return parser_range_adaptor_closure<parser_range_fn>{parser_range_fn{}, parser};
}
};

} // detail

inline const auto parse_packets_view = detail::parser_range_fn{};

namespace views {
inline const auto parse_packets = parse_packets_view;
} // views

} // luma_av



#endif // LUMA_AV_PARSER_HPP