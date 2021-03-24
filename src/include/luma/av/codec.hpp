

#ifndef LUMA_AV_CODEC_HPP
#define LUMA_AV_CODEC_HPP

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <ranges>

#include <memory>

#include <luma/av/result.hpp>
#include <luma/av/frame.hpp>
#include <luma/av/detail/unique_or_null.hpp>
#include <luma/av/packet.hpp>

/*
https://ffmpeg.org/doxygen/3.2/group__lavc__decoding.html#ga8f5b632a03ce83ac8e025894b1fc307a
https://ffmpeg.org/doxygen/3.2/structAVCodecContext.html
*/

namespace luma {
namespace av {

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
inline result<const AVCodec*> find_decoder(const std::string& name) noexcept {
    const AVCodec* codec = avcodec_find_decoder_by_name(name.c_str());
    return codec_error_handling(codec);
}

struct codec_par_deleter {
    void operator()(AVCodecParameters* par) const noexcept {
        avcodec_parameters_free(&par);
    }
};

struct codec_context_deleter {
    void operator()(AVCodecContext* ctx) const noexcept {
        avcodec_free_context(&ctx);
    }
};

}// detail

// https://ffmpeg.org/doxygen/3.4/structAVCodec.html#a16e4be8873bd93ac84c7b7d86455d518
/**
 convenience wrapper around an AVCodec ptr that represents a reference to a global codec
    owned by the ffmpeg api
 */
class codec {
    public:

    // automatically throws. use the free function to handle yourself
    explicit codec(enum AVCodecID id) noexcept
     : codec_{detail::find_decoder(id).value()} {

    }

    explicit codec(const std::string& name) noexcept
    : codec_{detail::find_decoder(name).value()} {

    }

    // contract that codec isnt null
    explicit codec(const AVCodec* codec) noexcept
     : codec_{codec} {
        assert(codec_);
    }

    auto get() const noexcept -> const AVCodec* {
        return codec_;
    }

    codec(const codec&) noexcept = default;
    codec& operator=(const codec&) noexcept = default;

    private:
        const AVCodec* codec_;
};

// if people want to use the error code api
inline result<codec> find_decoder(enum AVCodecID id) noexcept {
    LUMA_AV_OUTCOME_TRY(codec_ptr, detail::find_decoder(id));
    return codec{codec_ptr};
}
inline result<codec> find_decoder(const std::string& name) noexcept {
    LUMA_AV_OUTCOME_TRY(codec_ptr, detail::find_decoder(name));
    return codec{codec_ptr};
}


// https://ffmpeg.org/doxygen/3.2/structAVCodecParameters.html
class codec_parameters : public detail::unique_or_null<AVCodecParameters, detail::codec_par_deleter> {
    public:

    using base_type = detail::unique_or_null<AVCodecParameters, detail::codec_par_deleter>;
    // do i need this using here?
    //  i didnt need it in the tests. i forget the rules
    using base_type::get;

    codec_parameters() noexcept
    : base_type{alloc_codec_par().value()} {
    }

    codec_parameters(const AVCodecParameters* par) noexcept
        : base_type{alloc_codec_par().value()} {
        copy_par(this->get(), par).value();
    }

    codec_parameters(const codec_parameters& other) noexcept
    : base_type{alloc_codec_par().value()} {
        copy_par(this->get(), other.get()).value();
    }

    codec_parameters& operator=(const codec_parameters& other) noexcept {
        // think this is all you need. it should completely overwrite
        //  the first codec par with the second
        copy_par(this->get(), other.get()).value();
        return *this;
    }

    codec_parameters(codec_parameters&&) noexcept = default;
    codec_parameters& operator=(codec_parameters&&) noexcept = default;


    private:
    // prob all static in the cpp eventually?
    //  something less visible than private functions
    result<base_type> alloc_codec_par() noexcept {
        auto par =  avcodec_parameters_alloc();
        if (par) {
            return base_type{par};
        } else {
            return errc::alloc_failure;
        }
    }
    result<void> copy_par(AVCodecParameters* out_par, const AVCodecParameters* par) noexcept {
        return detail::ffmpeg_code_to_result(avcodec_parameters_copy(out_par, par));
    }
};

class codec_context : public detail::unique_or_null<AVCodecContext, detail::codec_context_deleter>{

    public:
    using base_type = detail::unique_or_null<AVCodecContext, detail::codec_context_deleter>;
    using base_type::get;

    explicit codec_context(const codec& codec) noexcept
        : base_type{alloc_context(codec).value()}, codec_{codec.get()} {

    }

    explicit codec_context(const codec& codec, const codec_parameters& par) noexcept
        : codec_context{codec} {
        codec_ctx_from_par(this->get(), par.get()).value();
    }

    codec_context(const codec_context&) = delete;
    codec_context& operator=(const codec_context&) = delete;
    codec_context(codec_context&&) noexcept = default;
    codec_context& operator=(codec_context&&) noexcept = default;

    result<codec_parameters> codec_par() const noexcept {
        auto par = codec_parameters{};
        LUMA_AV_OUTCOME_TRY(codec_par_from_ctx(par.get(), this->get()));
        return par;
    }


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
        auto ec = avcodec_send_packet(this->get(), p);
        return detail::ffmpeg_code_to_result(ec);
    }

    // convenience overload for our own packet
    result<void> send_packet(const packet& p) noexcept {
        return this->send_packet(p.get());
    }

    result<void> recieve_frame(AVFrame* frame) noexcept {
        auto ec = avcodec_receive_frame(this->get(), frame);
        return detail::ffmpeg_code_to_result(ec);
    }
    result<void> recieve_frame(Frame& frame) noexcept {
        auto ec = avcodec_receive_frame(this->get(), frame.get());
        return detail::ffmpeg_code_to_result(ec);
    }

    const AVCodec* codec() const noexcept {
        return codec_;
    }


    private:
    // static in cpp
    result<base_type> alloc_context(const av::codec& codec) noexcept {
        auto ctx = avcodec_alloc_context3(codec.get());
        if (ctx) {
            return base_type{ctx};
        } else {
            // i think this is always an alloc failure?
            return errc::alloc_failure;
        }
    }
    result<void> codec_par_from_ctx(AVCodecParameters* par, const AVCodecContext* ctx) const noexcept {
        return detail::ffmpeg_code_to_result(avcodec_parameters_from_context(par, ctx));
    }
    result<void> codec_ctx_from_par(AVCodecContext* ctx, const AVCodecParameters* par) const noexcept {
        return detail::ffmpeg_code_to_result(avcodec_parameters_to_context(ctx, par));
    }
    const AVCodec* codec_ = nullptr; 
    // frame decoder_frame_;
    // packet encoder_packet_;
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
    
    Encoder(codec_context ctx, packet pkt) noexcept : ctx_{std::move(ctx)}, encoder_packet_{std::move(pkt)} {}
    public:
    // more ctors soonTM pass ur own packet or something
    static result<Encoder> make(codec_context ctx, AVDictionary**  options = nullptr) noexcept {
        LUMA_AV_OUTCOME_TRY_FF(avcodec_open2(ctx.get(), ctx.codec(), options));
        LUMA_AV_OUTCOME_TRY(pkt, packet::make());
        return Encoder{std::move(ctx), std::move(pkt)};
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
    packet& view_packet() noexcept {
        return encoder_packet_;
    }
    // if they do want their own packet
    result<packet> ref_packet() noexcept {
        return packet::make(encoder_packet_.get(), packet::shallow_copy);
    }
    private:
    codec_context ctx_;
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
            return luma::av::outcome::failure(res.error());
        }
    }
    return luma::av::outcome::success();
}

template <class OutputIt>
result<void> Drain(Encoder& enc, OutputIt packet_out) noexcept {
    LUMA_AV_OUTCOME_TRY(enc.start_draining());
    while (true) {
        if (auto res = enc.recieve_packet()) {
            LUMA_AV_OUTCOME_TRY(pkt, enc.ref_packet());
            *packet_out = std::move(pkt);
        } else if (res.error().value() == AVERROR_EOF) {
            return luma::av::outcome::success();
        } else {
            return luma::av::outcome::failure(res.error());
        }
    }
}

class Decoder {
    
    Decoder(codec_context ctx, Frame f) noexcept : ctx_{std::move(ctx)}, decoder_frame_{std::move(f)} {}
    public:
    // more ctors soonTM pass ur own packet or something
    static result<Decoder> make(codec_context ctx, AVDictionary**  options = nullptr) noexcept {
        LUMA_AV_OUTCOME_TRY_FF(avcodec_open2(ctx.get(), ctx.codec(), options));
        LUMA_AV_OUTCOME_TRY(f, Frame::make());
        return Decoder{std::move(ctx), std::move(f)};
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

    Frame& view_frame() noexcept {
        return decoder_frame_;
    }

    result<Frame> ref_frame() noexcept {
        return Frame::make(decoder_frame_.get(), Frame::shallow_copy);
    }
    private:
    codec_context ctx_;
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
            return luma::av::outcome::failure(res.error());
        }
    }
    return luma::av::outcome::success();
}

template <class OutputIt>
result<void> Drain(Decoder& dec, OutputIt frame_out) noexcept {
    LUMA_AV_OUTCOME_TRY(dec.start_draining());
    while (true) {
        if (auto res = dec.recieve_frame()) {
            LUMA_AV_OUTCOME_TRY(f, dec.ref_frame());
            *frame_out = std::move(f);
        } else if (res.error().value() == AVERROR_EOF) {
            return luma::av::outcome::success();
        } else {
            return luma::av::outcome::failure(res.error());
        }
    }
}





struct EncClosure {
    Encoder& enc;
    template<class F>
    result<std::reference_wrapper<packet>> operator()(result<F> const& frame_res) noexcept {
        LUMA_AV_OUTCOME_TRY(frame, frame_res);
        return EncodeImpl(frame);
    }
    template<class F>
    result<std::reference_wrapper<packet>> operator()(F const& frame) noexcept {
        return EncodeImpl(frame);
    }
    template<class F>
    result<std::reference_wrapper<packet>> EncodeImpl(F const& frame) noexcept {
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
    result<std::reference_wrapper<Frame>> operator()(result<Pkt> const& packet_res) noexcept {
        LUMA_AV_OUTCOME_TRY(packet, packet_res);
        return DecodeImpl(packet);
    }
    template<class Pkt>
    result<std::reference_wrapper<Frame>> operator()(Pkt const& packet) noexcept {
        return DecodeImpl(packet);
    }

    template<class Pkt>
    result<std::reference_wrapper<Frame>> DecodeImpl(Pkt const& packet) noexcept {
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



} // av
} // luma

#endif // LUMA_AV_CODEC_HPP