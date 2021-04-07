#ifndef LUMA_AV_FORMAT_HPP
#define LUMA_AV_FORMAT_HPP


/*
https://www.ffmpeg.org/doxygen/3.3/group__libavf.html
*/

extern "C" {
#include <libavformat/avformat.h>
}

#include <luma_av/packet.hpp>
#include <luma_av/result.hpp>
#include <luma_av/util.hpp>

namespace luma_av {

/*
Most importantly an AVFormatContext contains:
    the input or output format. It is either autodetected or set by user for input;
        always set by user for output.
    an array of AVStreams, which describe all elementary streams stored in the file.
        AVStreams are typically referred to using their index in this array.
    an I/O context. It is either opened by lavf or set by user for input,
        always set by user for output (unless you are dealing with an AVFMT_NOFILE format).

*/
class format_context {

    /**
    trying to keep all format ctx memory management in this class. not 100% sure if it will work out
    but by default im saying if u want a managed format ctx this class should be able to do it
    and rn im communicating that by keeping the deleter and shit in this class. maybe we can move them to an
    impl class in a detail header to clean up but besides that i like this
    */
    struct format_context_deleter {
    void operator()(AVFormatContext* fctx) const noexcept {
        // think close input is enough to completely free in all cases
        avformat_close_input(&fctx);
        // avformat_free_context(fctx);
    }
    };

    using unique_fctx = std::unique_ptr<AVFormatContext,
                                        format_context_deleter>;


    /**
    static private is generally awk i think but in this case i think we actually do want a class local free function
    that supports the private impl. ig the argument of u should use a private class instead still applies. 
    but because result makes ctors awkward a class here would just be a clunkier version of what we have
    (a unique ptr and a free function)
    */
    static result<unique_fctx> alloc_format_ctx() noexcept {
        auto ctx = avformat_alloc_context();
        if (ctx) {
            return unique_fctx{ctx};
        } else {
            return luma_av::make_error_code(errc::alloc_failure);
        }
    }

    unique_fctx fctx_{};

    format_context() noexcept = default;
    format_context(AVFormatContext* ctx) noexcept
        : fctx_{ctx} {

    }
    public:

    ~format_context() noexcept = default;
    format_context(format_context const&) = delete;
    format_context& operator=(format_context const&) = delete;
    format_context(format_context&&) noexcept = default;
    format_context& operator=(format_context&&) noexcept = default;

    static result<format_context> make() noexcept {
        LUMA_AV_OUTCOME_TRY(ctx, alloc_format_ctx());
        return format_context{ctx.release()};
    }

    static result<format_context> make(const cstr_view url) noexcept {
        AVFormatContext* fctx = nullptr;\
        LUMA_AV_OUTCOME_TRY_FF(avformat_open_input(&fctx, url.c_str(), nullptr, nullptr));
        auto ctx = format_context{fctx};
        LUMA_AV_OUTCOME_TRY(ctx.find_stream_info(nullptr));
        return ctx;
    }

    result<void> find_stream_info(AVDictionary** options) noexcept {
        return detail::ffmpeg_code_to_result(avformat_find_stream_info(fctx_.get(), options));
    }

    // lighest weight easiest to misuse
    result<void> read_frame(AVPacket* pkt) noexcept {
        return detail::ffmpeg_code_to_result(av_read_frame(fctx_.get(), pkt));
    }
    // if the user wants to manage the packet themselves
    result<void> read_frame(packet& pkt) noexcept {
        return this->read_frame(pkt.get());
    }
    // if the user always wants a copy of the packet
    result<packet> read_frame() noexcept {
        LUMA_AV_OUTCOME_TRY(pkt, packet::make());
        LUMA_AV_OUTCOME_TRY(this->read_frame(pkt));
        return std::move(pkt);
    }
};

/**
gives us a packet workspace. helps with range functionality
*/
class Reader {
    public:
    static result<Reader> make(format_context fctx) noexcept {
        LUMA_AV_OUTCOME_TRY(pkt, packet::make());
        return Reader{std::move(fctx), std::move(pkt)};
    }
    static result<Reader> make(const cstr_view url) noexcept {
        LUMA_AV_OUTCOME_TRY(fctx, format_context::make(url));
        LUMA_AV_OUTCOME_TRY(pkt, packet::make());
        return Reader{std::move(fctx), std::move(pkt)};
    }
    result<void> ReadFrameInPlace() noexcept {
        return fctx_.read_frame(reader_packet_);
    }
    result<packet> ReadFrame() noexcept {
        LUMA_AV_OUTCOME_TRY(ReadFrameInPlace());
        return ref_packet();
    }
    
    /*
    we dont need to provide any overloads that use packets as out params
    this class already has its own packet.
    we can provide any customization for managing the packet in the constructor
    */

    packet& view_packet() noexcept {
        return reader_packet_;
    }
    result<packet> ref_packet() noexcept {
        return packet::make(reader_packet_, packet::shallow_copy);
    }
    private:
    Reader(format_context fctx, packet reader_packet) 
        : reader_packet_{std::move(reader_packet)}, fctx_{std::move(fctx_)} {}
    packet reader_packet_;
    format_context fctx_;

};

namespace detail {
// i dont understand why these specific concepts
template <std::ranges::view R>
class input_reader_view : public std::ranges::view_interface<input_reader_view<R>> {
public:

input_reader_view() noexcept = default;
explicit input_reader_view(R base, Reader& reader) 
    : base_{std::move(base)}, reader_{std::addressof(reader)} {

}

// think these accessors are specific to this view and can be whatever i want?
auto base() const noexcept -> R {
    return base_;
}
auto reader() const noexcept -> Reader& {
    return *reader_;
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
Reader* reader_ = nullptr;

template <bool is_const>
class iterator;

};


template <std::ranges::viewable_range R>
input_reader_view(R&&, Reader&) -> input_reader_view<std::ranges::views::all_t<R>>;


template <std::ranges::view R>
template <bool is_const>
class input_reader_view<R>::iterator {
    using output_type = result<std::reference_wrapper<const packet>>;
    using parent_t = detail::MaybeConst_t<is_const, input_reader_view<R>>;
    using base_t = detail::MaybeConst_t<is_const, R>;
    friend iterator<not is_const>;

    parent_t* parent_ = nullptr;
    mutable std::ranges::iterator_t<base_t> current_{};
    mutable bool reached_eof_ = false;
    mutable std::ptrdiff_t skip_count_{0};
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

// note const as in *it == *it. not const as in thread safe
output_type operator*() const {
    // means deref end/past the end if this hits
    LUMA_AV_ASSERT(!reached_eof_);
    // i fucked up my counter logic if this fires
    LUMA_AV_ASSERT(skip_count_ >= -1);
    // if the user hasnt incremented we just want to return the last frame if there is one
    //  that way *it == *it is true
    if ((skip_count_== -1) && (cached_packet_)) {
        return *cached_packet_;
    }
    auto& reader = parent_->reader();
    while(true) {
        if (auto res = reader.ReadFrameInPlace()) {
            if (skip_count_ <= 0) {
                auto out = output_type{reader.view_packet()};
                cached_packet_ = out;
                skip_count_ = -1;
                return out;
            } else {
                skip_count_ -= 1;
                continue;
            }
        } else if (res.error().value() == AVERROR_EOF) {
            reached_eof_ = true;
            return errc::detail_reader_range_end;
        } else {
            return res.error();
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
    return reached_eof_;
}

bool operator==(iterator const& other) const 
requires std::equality_comparable<std::ranges::iterator_t<base_t>> {
    return parent_->reader_ == other.parent_->reader_ &&
    current_ == other.current_ &&
    reached_eof_ == other.reached_eof_ &&
    skip_count_ == other.skip_count_ &&
    cached_packet_.has_value() == other.cached_packet_.has_value();
}

};

inline const auto filter_reader_uwu_view = std::views::filter([](const auto& res){
    if (res) {
        return true;
    } else if (res.error() == errc::detail_reader_range_end) {
        return false;
    } else {
        return true;
    }
});

inline const auto filter_reader_uwu = filter_reader_uwu_view;

class input_reader_view_fn {
public:
auto operator()(Reader& reader) const {
    return input_reader_view{std::views::single(0), reader} | filter_reader_uwu;
}
};

} // detail

inline const auto read_input_view = detail::input_reader_view_fn{};

namespace views {
inline const auto read_input = read_input_view;
} // views



} // luma_av

#endif // LUMA_AV_FORMAT_HPP