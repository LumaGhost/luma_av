

#ifndef LUMA_AV_CODEC_HPP
#define LUMA_AV_CODEC_HPP

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <ranges>

#include <memory>

#include <luma_av/result.hpp>
#include <luma_av/frame.hpp>
#include <luma_av/packet.hpp>
#include <luma_av/util.hpp>

/*
https://ffmpeg.org/doxygen/3.2/group__lavc__decoding.html#ga8f5b632a03ce83ac8e025894b1fc307a
https://ffmpeg.org/doxygen/3.2/structAVCodecContext.html
*/

namespace luma_av {

namespace detail {

inline result<const AVCodec*> codec_error_handling(const AVCodec* codec) noexcept {
    if (codec) {
        return codec;
    } else {
        // think the error is always codec not found
        return errc::codec_not_found;
    }
}

// overload set since its c++ and we can do that
inline result<const AVCodec*> find_decoder(enum AVCodecID id) noexcept {
    const AVCodec* codec = avcodec_find_decoder(id);
    return codec_error_handling(codec);
}
// think ffmpeg is expecting null terminated, so no sv here :/
inline result<const AVCodec*> find_decoder(const cstr_view name) noexcept {
    const AVCodec* codec = avcodec_find_decoder_by_name(name.c_str());
    return codec_error_handling(codec);
}



}// detail



// https://ffmpeg.org/doxygen/3.2/structAVCodecParameters.html
class CodecPar {

    struct codec_par_deleter {
    void operator()(AVCodecParameters* par) const noexcept {
        avcodec_parameters_free(&par);
    }
    };

    using unique_par = std::unique_ptr<AVCodecParameters, codec_par_deleter>;

        // prob all static in the cpp eventually?
    //  something less visible than private functions
    static result<unique_par> alloc_codec_par() noexcept {
        auto par =  avcodec_parameters_alloc();
        if (par) {
            return unique_par{par};
        } else {
            return errc::alloc_failure;
        }
    }
    result<void> copy_par(AVCodecParameters* out_par, const AVCodecParameters* par) noexcept {
        return detail::ffmpeg_code_to_result(avcodec_parameters_copy(out_par, par));
    }
    unique_par par_;
    explicit CodecPar(AVCodecParameters* par) : par_{par} {}
    public:
    using ffmpeg_ptr_type = AVCodecParameters*;
    static result<CodecPar> make() noexcept {
        LUMA_AV_OUTCOME_TRY(par, alloc_codec_par());
        return CodecPar{par.release()};
    }
    static result<CodecPar> make(const AVCodecParameters* other) noexcept {
        LUMA_AV_OUTCOME_TRY(par, CodecPar::make());
        LUMA_AV_OUTCOME_TRY_FF(avcodec_parameters_copy(par.get(), other));
        return std::move(par);
    }
    static result<CodecPar> make(CodecPar const& other) noexcept {
        LUMA_AV_OUTCOME_TRY(par, CodecPar::make());
        LUMA_AV_OUTCOME_TRY_FF(avcodec_parameters_copy(par.get(), other.get()));
        return std::move(par);
    }
    // we dont rly have an invariant besides being not null so we can let the user set the underlying ptr directly
    static result<CodecPar> FromOwner(AVCodecParameters* other) noexcept {
        LUMA_AV_ASSERT(other);
        return CodecPar{other};
    }
    static result<CodecPar> make(const AVCodecContext* other) noexcept {
        LUMA_AV_OUTCOME_TRY(par, CodecPar::make());
        LUMA_AV_OUTCOME_TRY_FF(avcodec_parameters_from_context(par.get(), other));
        return std::move(par);
    }

    result<void> SetFromCtx(const AVCodecContext* other) noexcept {
        LUMA_AV_OUTCOME_TRY_FF(avcodec_parameters_from_context(par_.get(), other));
        return luma_av::outcome::success();
    }

    AVCodecParameters* get() noexcept {
        return par_.get();
    }
    const AVCodecParameters* get() const noexcept {
        return par_.get();
    }

    ~CodecPar() noexcept = default;
    CodecPar(CodecPar const&) noexcept = delete;
    CodecPar& operator=(CodecPar const&) noexcept = delete;
    CodecPar(CodecPar&&) noexcept = default;
    CodecPar& operator=(CodecPar&&) noexcept = default;

};

class CodecContext {
    struct CodecContextDeleter {
    void operator()(AVCodecContext* ctx) const noexcept {
        avcodec_free_context(&ctx);
    }
    };
    using unique_codec_ctx = std::unique_ptr<AVCodecContext, CodecContextDeleter>;
        // static in cpp
    static result<unique_codec_ctx> alloc_context(const AVCodec* codec) noexcept {
        auto ctx = avcodec_alloc_context3(codec);
        if (ctx) {
            return unique_codec_ctx{ctx};
        } else {
            // i think this is always an alloc failure?
            return errc::alloc_failure;
        }
    }
    /**
    invariant: ctx isnt null (except after move where we only support asignment)
                codec not null. codec is the codec that can be used to open the context
    */
    unique_codec_ctx ctx_;
    const AVCodec* codec_; 
    CodecContext(AVCodecContext* ctx, const AVCodec* codec) : ctx_{ctx}, codec_{codec} {}
    public:


    static result<CodecContext> make(const cstr_view codec_name) noexcept {
        LUMA_AV_OUTCOME_TRY(codec, detail::find_decoder(codec_name));
        LUMA_AV_OUTCOME_TRY(ctx, alloc_context(codec));
        return CodecContext{ctx.release(), codec};
    }

    static result<CodecContext> make(const AVCodec* codec) noexcept {
        LUMA_AV_ASSERT(codec);
        LUMA_AV_OUTCOME_TRY(ctx, alloc_context(codec));
        return CodecContext{ctx.release(), codec};
    }

    static result<CodecContext> make(const cstr_view codec_name, const NonOwning<CodecPar> par) noexcept {
        LUMA_AV_OUTCOME_TRY(ctx, CodecContext::make(codec_name));
        LUMA_AV_OUTCOME_TRY(ctx.SetPar(par));
        return std::move(ctx);
    }
    static result<CodecContext> make(const AVCodec* codec, const NonOwning<CodecPar> par) noexcept {
        LUMA_AV_OUTCOME_TRY(ctx, CodecContext::make(codec));
        LUMA_AV_OUTCOME_TRY(ctx.SetPar(par));
        return std::move(ctx);
    }

    result<void> SetPar(const NonOwning<CodecPar> par) noexcept {
        LUMA_AV_OUTCOME_TRY_FF(avcodec_parameters_to_context(ctx_.get(), par.ptr()));
        return luma_av::outcome::success();
    }
    result<CodecPar> GetPar() noexcept {
        return CodecPar::make(ctx_.get());
    }



    CodecContext(const CodecContext&) = delete;
    CodecContext& operator=(const CodecContext&) = delete;
    CodecContext(CodecContext&&) noexcept = default;
    CodecContext& operator=(CodecContext&&) noexcept = default;


    /*
    warning:
    "The input buffer, avpkt->data must be AV_INPUT_BUFFER_PADDING_SIZE 
        larger than the actual read bytes because some optimized bitstream readers
        read 32 or 64 bits at once and could read over the end.
    "Do not mix this API with the legacy API (like avcodec_decode_video2())
        on the same AVCodecContext. It will return unexpected results
         now or in future libavcodec versions."
    */
    result<void> send_packet(const AVPacket* p) noexcept {
        auto ec = avcodec_send_packet(ctx_.get(), p);
        return detail::ffmpeg_code_to_result(ec);
    }

    // convenience overload for our own packet
    result<void> send_packet(const packet& p) noexcept {
        return this->send_packet(p.get());
    }

    result<void> recieve_frame(AVFrame* frame) noexcept {
        auto ec = avcodec_receive_frame(ctx_.get(), frame);
        return detail::ffmpeg_code_to_result(ec);
    }
    result<void> recieve_frame(Frame& frame) noexcept {
        auto ec = avcodec_receive_frame(ctx_.get(), frame.get());
        return detail::ffmpeg_code_to_result(ec);
    }

    const AVCodec* codec() const noexcept {
        return codec_;
    }
    const AVCodecContext* get() const noexcept {
        return ctx_.get();
    }
    AVCodecContext* get() noexcept {
        return ctx_.get();
    }
};

// not sure if i want the packet/frame workspace to be part of the class yet
// on one hand u inevitably need that to encode/decode. on the other there is more
//  than one way to manage that workspace. meanwhile passing around both
//  the ctx and the workspace can be a pain if ur always forced to do that. 
// u can try supporting both too. there are a lot of options. 
// in the end i think u need the option to contain one in the class
//  and at that point we should have some distinction from encoder and deocder
//  so we dont have to deal with 4 combinations of states or allocate workspaces we dont need
// for now sticking with out
/**
i think the codec context can be a detail class at this point?
then the user uses the encoder and decoder. 
alternatively (cause i think theres use in sparating a "codec info" class from "encode/decode state")
we could make the contexr pre decoding. then u open that and end up with either an opened encoder or decoder
tbh i kinda like that a lot better. i like clarity on opened/unopened. cause theyre two separate states
and they even have separate apis in a way. at least the opened has an additional api.
also i dont think u can sumultaneously be an encoder and decoder with the same context (although u dont
care until u open and start the encode/decode cause otherwise their apis are the same)
i think thats better expressed as three types (context, open encoder, open decoder)
oh and also yea u still want the codec ctx class to represent the combination of the codec and the ctx
cause i forgot u need both when u open it

so now the plan is a codec context class to represent pre opening stuff like changing params
then "opening" that by transfering ownership to a separate encoder/decoder type that exposes
the encoding and decoding api of the context. 
*/
class Encoder {
    
    Encoder(CodecContext ctx, packet pkt) noexcept : ctx_{std::move(ctx)}, encoder_packet_{std::move(pkt)} {}
    public:
    // more ctors soonTM pass ur own packet or something
    static result<Encoder> make(CodecContext ctx, AVDictionary**  options = nullptr) noexcept {
        LUMA_AV_OUTCOME_TRY_FF(avcodec_open2(ctx.get(), ctx.codec(), options));
        LUMA_AV_OUTCOME_TRY(pkt, packet::make());
        return Encoder{std::move(ctx), std::move(pkt)};
    }
    static result<Encoder> make(const cstr_view codec_name, 
                                AVDictionary**  options = nullptr) noexcept {
        LUMA_AV_OUTCOME_TRY(ctx, CodecContext::make(codec_name));
        return Encoder::make(std::move(ctx), options);
    }
    static result<Encoder> make(const cstr_view codec_name, 
                                const NonOwning<CodecPar> par, 
                                AVDictionary**  options = nullptr) noexcept {
        LUMA_AV_OUTCOME_TRY(ctx, CodecContext::make(codec_name, par));
        return Encoder::make(std::move(ctx), options);
    }
    // not using any of the send_packet overloads to drain
    //  i want all those overloads to mean the same thing. im not even calling them here
    //  those overloads are just to send packets. the meaning of passing nullptr is
    //  subject to change (e.g. what if packet can convert from nullptr like regardless
    //  those semantics now silently depends on this code and what it means to pass nullptr to 
    //  these functions. id rather just be explicit)
    // more formally: passing nullptr to send_packet/send_frame is not guarenteed to drain
    //  use these functions to drain
    result<void> start_draining() noexcept {
        auto ec = avcodec_send_frame(ctx_.get(), nullptr);
        return detail::ffmpeg_code_to_result(ec);
    }
    
    result<void> send_frame(const AVFrame* f) noexcept {
        auto ec = avcodec_send_frame(ctx_.get(), f);
        return detail::ffmpeg_code_to_result(ec);
    }
    result<void> send_frame(const Frame& f) noexcept {
        auto ec = avcodec_send_frame(ctx_.get(), f.get());
        return detail::ffmpeg_code_to_result(ec);
    }
    result<void> recieve_packet() noexcept {
        auto ec = avcodec_receive_packet(ctx_.get(), encoder_packet_.get());
        return detail::ffmpeg_code_to_result(ec);
    }
    // if the user doesnt want their own packet after encoding the frame
    packet const& view_packet() noexcept {
        return encoder_packet_;
    }
    // if they do want their own packet
    result<packet> ref_packet() noexcept {
        return packet::make(encoder_packet_.get(), packet::shallow_copy);
    }
    private:
    CodecContext ctx_;
    packet encoder_packet_;
};

/**
like this approach a lot because the user gets control over all of the memory.
(pending the right support for passing ur own packet in the encoder class)
this function only handles the encoding algorithm
i would ideally like to reuse this algo
*/
template <std::ranges::range Frames, class OutputIt>
result<void> Encode(Encoder& enc, Frames const& frames, OutputIt packet_out) noexcept {
    for (auto const& frame : frames) {
        LUMA_AV_OUTCOME_TRY(enc.send_frame(frame));
        if (auto res = enc.recieve_packet()) {
            LUMA_AV_OUTCOME_TRY(pkt, enc.ref_packet());
            *packet_out = std::move(pkt);
        } else if (res.error().value() == AVERROR(EAGAIN)) {
            continue;
        } else {
            return luma_av::outcome::failure(res.error());
        }
    }
    return luma_av::outcome::success();
}

template <class OutputIt>
result<void> Drain(Encoder& enc, OutputIt packet_out) noexcept {
    LUMA_AV_OUTCOME_TRY(enc.start_draining());
    while (true) {
        if (auto res = enc.recieve_packet()) {
            LUMA_AV_OUTCOME_TRY(pkt, enc.ref_packet());
            *packet_out = std::move(pkt);
        } else if (res.error().value() == AVERROR_EOF) {
            return luma_av::outcome::success();
        } else {
            return luma_av::outcome::failure(res.error());
        }
    }
}

class Decoder {
    
    Decoder(CodecContext ctx, Frame f) noexcept : ctx_{std::move(ctx)}, decoder_frame_{std::move(f)} {}
    public:
    // more ctors soonTM pass ur own packet or something
    static result<Decoder> make(CodecContext ctx, AVDictionary**  options = nullptr) noexcept {
        LUMA_AV_OUTCOME_TRY_FF(avcodec_open2(ctx.get(), ctx.codec(), options));
        LUMA_AV_OUTCOME_TRY(f, Frame::make());
        return Decoder{std::move(ctx), std::move(f)};
    }
    static result<Decoder> make(const cstr_view codec_name, 
                                AVDictionary**  options = nullptr) noexcept {
        LUMA_AV_OUTCOME_TRY(ctx, CodecContext::make(codec_name));
        return Decoder::make(std::move(ctx), options);
    }
    static result<Decoder> make(const cstr_view codec_name, 
                                const NonOwning<CodecPar> par, 
                                AVDictionary**  options = nullptr) noexcept {
        LUMA_AV_OUTCOME_TRY(ctx, CodecContext::make(codec_name, par));
        return Decoder::make(std::move(ctx), options);
    }

    result<void> start_draining() noexcept {
        auto ec = avcodec_send_packet(ctx_.get(), nullptr);
        return detail::ffmpeg_code_to_result(ec);
    }
    
    result<void> send_packet(const AVPacket* p) noexcept {
        auto ec = avcodec_send_packet(ctx_.get(), p);
        return detail::ffmpeg_code_to_result(ec);
    }
    result<void> send_packet(const packet& p) noexcept {
        auto ec = avcodec_send_packet(ctx_.get(), p.get());
        return detail::ffmpeg_code_to_result(ec);
    }
    result<void> recieve_frame() noexcept {
        auto ec = avcodec_receive_frame(ctx_.get(), decoder_frame_.get());
        return detail::ffmpeg_code_to_result(ec);
    }

    Frame const& view_frame() noexcept {
        return decoder_frame_;
    }

    result<Frame> ref_frame() noexcept {
        return Frame::make(decoder_frame_.get(), Frame::shallow_copy);
    }
    private:
    CodecContext ctx_;
    Frame decoder_frame_;
};

template <std::ranges::range Packets, class OutputIt>
result<void> Decode(Decoder& dec, Packets const& packets, OutputIt frame_out) noexcept {
    for (auto const& packet : packets) {
        LUMA_AV_OUTCOME_TRY(dec.send_packet(packet));
        if (auto res = dec.recieve_frame()) {
            LUMA_AV_OUTCOME_TRY(f, dec.ref_frame());
            *frame_out = std::move(f);
        } else if (res.error().value() == AVERROR(EAGAIN)) {
            continue;
        } else {
            return luma_av::outcome::failure(res.error());
        }
    }
    return luma_av::outcome::success();
}

template <class OutputIt>
result<void> Drain(Decoder& dec, OutputIt frame_out) noexcept {
    LUMA_AV_OUTCOME_TRY(dec.start_draining());
    while (true) {
        if (auto res = dec.recieve_frame()) {
            LUMA_AV_OUTCOME_TRY(f, dec.ref_frame());
            *frame_out = std::move(f);
        } else if (res.error().value() == AVERROR_EOF) {
            return luma_av::outcome::success();
        } else {
            return luma_av::outcome::failure(res.error());
        }
    }
}





struct EncClosure {
    Encoder& enc;
    template<class F>
    result<std::reference_wrapper<const packet>> operator()(result<F> const& frame_res) noexcept {
        LUMA_AV_OUTCOME_TRY(frame, frame_res);
        return EncodeImpl(frame);
    }
    template<class F>
    result<std::reference_wrapper<const packet>> operator()(F const& frame) noexcept {
        return EncodeImpl(frame);
    }
    template<class F>
    result<std::reference_wrapper<const packet>> EncodeImpl(F const& frame) noexcept {
        LUMA_AV_OUTCOME_TRY(enc.send_frame(frame));
        LUMA_AV_OUTCOME_TRY(enc.recieve_packet());
        return enc.view_packet();
    }

};
const auto encode_view = [](Encoder& enc){
    return std::views::transform([&](const auto& frame) {
        return EncClosure{enc}(frame);
    });
};
namespace views {
const auto encode = encode_view;
} // views

struct DecClosure {
    Decoder& dec;
    template<class Pkt>
    result<std::reference_wrapper<const Frame>> operator()(result<Pkt> const& packet_res) noexcept {
        LUMA_AV_OUTCOME_TRY(packet, packet_res);
        return DecodeImpl(packet);
    }
    template<class Pkt>
    result<std::reference_wrapper<const Frame>> operator()(Pkt const& packet) noexcept {
        return DecodeImpl(packet);
    }

    template<class Pkt>
    result<std::reference_wrapper<const Frame>> DecodeImpl(Pkt const& packet) noexcept {
        LUMA_AV_OUTCOME_TRY(dec.send_packet(packet));
        LUMA_AV_OUTCOME_TRY(dec.recieve_frame());
        return dec.view_frame();
    }
};
const auto decode_view = [](Decoder& dec){
    return std::views::transform([&](const auto& packet_res) {
        return DecClosure{dec}(packet_res);
    });
};

namespace views {
const auto decode = decode_view;
} // views


// i dont understand why these specific concepts
template <std::ranges::view R>
class decode_view2 : public std::ranges::view_interface<decode_view2<R>> {
public:
decode_view2() noexcept = default;
explicit decode_view2(R base, Decoder& dec) 
    : base_{std::move(base)}, dec_{std::addressof(dec)} {

}

// think these accessors are specific to this view and can be whatever i want?
auto base() const noexcept -> R {
    return base_;
}
auto decoder() const noexcept -> Decoder& {
    return *dec_;
}

// we prob wont have const qualified begin
// eventhough begin must be constant time we are allowed to do a linear operation then cache it. 
auto begin() {
    return iterator<false>{*this};
}
auto begin() const {
    return iterator<true>{*this};
}

auto end() {
    return end_impl(*this);
}
auto end() const {
    return end_impl(*this);
}

// we only require a sized range if the user actaully calls .size
//  we wont be able to give this anyway most likely so good thing its not required
// compute distance is used to implement these in the video
// auto size() requires std::ranges::sized_range<R>;
// auto size() const requires std::ranges::sized_range<R>;

private:
R base_{};
Decoder* dec_ = nullptr;

template <bool is_const>
class iterator;

// not sure exactly why static member but im following along
template <class Self>
static auto end_impl(Self& self) {
    // their impl is calling end on the base
    // ik that wont be right for me but im following along for now
    return std::ranges::end(self.base_);
}

// they also added a "compute distance" helper to help calculate the number of elements remaining or something
//  dont think we're actually going to be a sized range so not sure if that applies to us

};

// not sure exactly why u do the guide this way but ik tldr "all_view makes this work with more types nicely"
//  also ofc we dont want to generate a bunch of classes for all the different forwarding refs so we strip that
template <std::ranges::viewable_range R>
decode_view2(R&&, Decoder&) -> decode_view2<std::ranges::views::all_t<R>>;

namespace detail {

template <bool is_const, class T>
using MaybeConst_t = std::conditional_t<is_const, const T, T>;

} // detail

template <std::ranges::view R>
template <bool is_const>
class decode_view2<R>::iterator {
    // public:
    // this alias is my own
    using output_type = result<std::reference_wrapper<const Frame>>;
    // private:
    using parent_t = detail::MaybeConst_t<is_const, decode_view2<R>>;
    using base_t = detail::MaybeConst_t<is_const, R>;
    friend iterator<not is_const>;

    parent_t* parent_ = nullptr;
    mutable std::ranges::iterator_t<base_t> current_{};
    mutable bool draining_ = false;
    mutable bool done_draining_ = false;
    mutable std::ptrdiff_t skip_count_{0};
    mutable std::optional<output_type> cached_frame_;
    // other impl details to manage the iterating
    // in the video they use a step value

    // not sure if this is their impl detail or a required function
    //  think its theirs though
    iterator& advance(std::ranges::range_difference_t<base_t>);

    // they bring compute distance here too

public:

using difference_type = std::ranges::range_difference_t<base_t>;
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

// this is where we're actually allowed to do work
// skipping elements from the prev view is ok. incrementing during operator* may not be pog
// so deref is required to be const. but like... p sure that means equality preserving const?
// there is nothing i could do to make this thread safe anyway. like regardless
// pushing frames into the decoder isnt thread safe
output_type operator*() const {
    // means deref end/past the end if this hits
    LUMA_AV_ASSERT(!done_draining_);
    // i fucked up my counter logic if this fires
    LUMA_AV_ASSERT(skip_count_ >= -1);
    // if the user hasnt incremented we just want to return the last frame if there is one
    //  that way *it == *it is true
    if ((skip_count_== -1) && (cached_frame_)) {
        return *cached_frame_;
    }
    auto& dec = parent_->decoder();
    for(;current_!=std::ranges::end(parent_->base_); ++current_) {
        LUMA_AV_OUTCOME_TRY(dec.send_packet(*current_));
        if (auto res = dec.recieve_frame()) {
            if (skip_count_ == 0) {
                auto out = output_type{dec.view_frame()};
                cached_frame_ = out;
                skip_count_ -= 1;
                return out;
            } else {
                skip_count_ -= 1;
                continue;
            }
        } else if (res.error().value() == AVERROR(EAGAIN)) {
            continue;
        } else {
            return res.error();
        }
    }

    // draining
    // if we hit here there are no more packets to decode and we still dont have a frame
    if (!draining_) {
        LUMA_AV_OUTCOME_TRY(dec.start_draining());
        draining_ = true;
    }
    while(true) {
        if (auto res = dec.recieve_frame()) {
            if (skip_count_ == 0) {
                auto out = output_type{dec.view_frame()};
                cached_frame_ = out;
                skip_count_ -= 1;
                return out;
            } else {
                skip_count_ -= 1;
                continue;
            } 
        } else if (res.error().value() == AVERROR_EOF) {
            done_draining_ = true;
            return errc::decode_range_end;
        } else {
            return luma_av::outcome::failure(res.error());
        }
    }

    
}

/**
// can we just make ++ a no op? only * will do the incrementing as necessary
// or does that just make this totally troll?
// maybe incrementing can send? that would require a deref though
// dont think thats ok
// i feel like the noop is fine as long as it eventually returns end and doesnt break normal range stuff

think this might destroy interactions with other views like take though. take would prob be implemented
with clever incrementing. now we get into this weird thing where intuitively ur thinking abt the output
being the decoded frames so like take(2) would take the first 2 decoded frames. thats what we want. 
but realistically if our increment is a no op then so would take 2
so we probably dont want to violate increment being a thing. we lose support for interacting with other ranges

the next thing we can compromise on is being lazy. if we deref during the increment we wont be lazy
but at least we can work with other ranges. tbh i think not being lazy is unacceptable. 

what if increment just increments current? that doesnt rly work cause this ++ is supposed to represent
an increment in our range. if we just increment current then we're basicly applying the "take"
or whatever op to the packets before decoding. not what we want.
unfortnuately theres not rly a lazy way to refer to the "next" frame that i know of

afaik though this iterator needs to behave basicaly like an iterator to a vector<frame>
like ++ needs to refer to the next frame after decoding. but we wont know until we decode
so we rly cant express the state after decoding . e.g. with take(2)

idk maybe we can do something REAAAAAAAAAALY clever with an internal counter
like incrementing our iterator represents some filter that happens when calling *
that may work but idk. also how does something like filter view work? i feel like our problem is similar?
u need to deref the prev element to figure out if u actually want to iterate over it or not

ok derefing in the comparison operators for the end sintenel is fair game
https://github.com/gcc-mirror/gcc/blob/16e2427f50c208dfe07d07f18009969502c25dc8/libstdc%2B%2B-v3/include/std/ranges#L1854

ok i think i have an idea. begin will return a cached frame.
ok fuck we cant even do that cause we wont have a packet asdf.
regardless begin ultimately has to point to a frame. or a pseudo representation of a frame
BEFORE any derefrencing happens
at that point begin has to just return our own iter and we abstract it

ok so lets say ++ increments an internal counter. and on the deref we count frames and dont
return until we reach the current count? then we reset the count.
only issue there is what if we dont get enough? we can return eof
thats getting a bit better. but i still think thatll violate some 
guarentees that these algos want. for example how does end behave at that point?
we have infinite/unknown amnt of elements so we should be fine?
not quite. imagine an algo wants the 1st 3rd and 5th frame. lets say theres only 4 frames
we'll give them the first and third but instead of a 5th frame we'll give them empty
but it'll look like a frame. worse the algo could ask for 100 and we only have 4 frames.
the way u handle that is i think with the end. both those scenarios i descrive should be
invalid "increment past end" things. i think.
i dont think i can fully avoid a blank frame at the end though.
but i think i can abstract a filter view on top that will remove the last "empty frame"
fuckign poggers this just may actually work. we even fit in draining. sdfkjas;dklfjaskl;dfjal;skdfj

alright so to summarize. ++ just increments an internal counter. when we deref
we will skip that many decoded frames to simulate stepping over the output frames
if we're out of packets in the base range when we deref then we enter draining mode
which basically just means we can read frames from the decoder without needing to supply a packet.
so eventhough the packet range is over we can still read until we see a frame
and ofc we still skip at the users request.
if we still cant get a frame then we return a special error signalling that decoding is over.
at that point we set our internal "done draining" state. our end comparison simply checks that
state and if we're done draining then we compare true with "end" and if we're not then we dont.
deref when we're already done draining, i.e. we compared true with end, im just firing an assertion.
if we just let it roll, currently itd basically create an infinite loop, producing the decoding end error
and then we'd be filtering them. so instead just fire an assertion

back to the example what if the user wants 1 3 and 5 and we only have 4 frames?
we would start with 1 3 and the decode end error, but we'd strip that off with another view
leaving 1 and 3 which i believe is correct.

*/
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

// in the talk they do bidirectional ranges too
// dont think we can support that? if we get passed a bidirectional range
//  we can still only go forward. may be wrong abt that though
// wont have random access either afaik?

bool operator==(std::ranges::sentinel_t<base_t> const& other) const {
    return done_draining_;
}

bool operator==(iterator const& other) const 
requires std::equality_comparable<std::ranges::iterator_t<base_t>> {
    return current_ == other;
}

// they have these in the video not sure what our impl would look like
// shortcuts for swap(*iter, *iter) and std::move(*iter);
// friend auto iter_move(iterator& it) {
//     return *it;
// }
// friend void iter_swap(iterator const&, iterator const&)
// requires std::ranges::indirectly_swapable<iterator>;

// we also wont have random access stuff afaik like front back and []
// def wont have back cause i can p much tell off rip we wont be a common range
//  we're going to need a sentinel. 

};

namespace detail {

inline const auto filter_uwu_view = std::views::filter([](const auto& res){
    if (res) {
        return true;
    } else if (res.error() == errc::decode_range_end) {
        return false;
    } else {
        return true;
    }
});

inline const auto filter_uwu = filter_uwu_view;


template <class F>
class decode_range_adaptor_closure {
    F f_;
    Decoder* dec_;
    public:
    decode_range_adaptor_closure(F f, Decoder& dec) 
        : f_{std::move(f)}, dec_{std::addressof(dec)} {

    }
    template <std::ranges::viewable_range R>
    decltype(auto) operator()(R&& r) {
        return std::invoke(f_, std::forward<R>(r), *dec_);
    }

    template <std::ranges::viewable_range R>
    decltype(auto) operator()(R&& r) const {
         return std::invoke(f_, std::forward<R>(r), *dec_);
    }
    // template <std::ranges::input_range R>
    // requires std::ranges::viewable_range<R>
    // template <class R>
    // friend decltype(auto) operator|(R&& r, decode_range_adaptor_closure&& closure) {
    //     return std::invoke(std::move(closure), std::forward<R>(r));
    // }
    // template <class R>
    // friend decltype(auto) operator|(decode_range_adaptor_closure const& closure, R&& r) {
    //     return std::invoke(closure, std::forward<R>(r));
    // }
    // template <class R>
    // friend decltype(auto) operator|(decode_range_adaptor_closure const& closure, R&& r);
};

template <class F, std::ranges::viewable_range R>
decltype(auto) operator|(R&& r, decode_range_adaptor_closure<F> const& closure) {
    return closure(std::forward<R>(r));
}


class decode_view_fn {
public:
template <class R>
auto operator()(R&& r, Decoder& dec) const {
    return decode_view2{std::views::all(std::forward<R>(r)), dec} | filter_uwu;
    // return decode_view2{std::views::all(std::forward<R>(r)), dec};
}
auto operator()(Decoder& dec) const {
    return decode_range_adaptor_closure<decode_view_fn>{decode_view_fn{}, dec};
}
};

inline const auto decode2 = decode_view_fn{};

} // detail


namespace views {
const auto real_decode = detail::decode_view_fn{};
} // views



} // luma_av

#endif // LUMA_AV_CODEC_HPP