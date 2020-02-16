

#ifndef LUMA_AV_CODEC_HPP
#define LUMA_AV_CODEC_HPP

extern "C" {
#include <libavcodec/avcodec.h>
}

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

inline result<const AVCodec*> codec_error_handling(const AVCodec* codec) {
    if (codec) {
        return codec;
    } else {
        // think the error is always codec not found
        return errc::codec_not_found;
    }
}

// overload set since its c++ and we can do that
inline result<const AVCodec*> find_decoder(enum AVCodecID id) {
    const AVCodec* codec = avcodec_find_decoder(id);
    return codec_error_handling(codec);
}
// think ffmpeg is expecting null terminated, so no sv here :/
inline result<const AVCodec*> find_decoder(const std::string& name) {
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
    explicit codec(enum AVCodecID id) : codec_{detail::find_decoder(id).value()} {

    }

    explicit codec(const std::string& name) : codec_{detail::find_decoder(name).value()} {

    }

    // contract that codec isnt null
    explicit codec(const AVCodec* codec) : codec_{codec} {

    }

    auto get() const noexcept -> const AVCodec* {
        return codec_;
    }

    codec(const codec&) = default;
    codec& operator=(const codec&) = default;

    private:
        const AVCodec* codec_;
};

// if people want to use the error code api
inline result<codec> find_decoder(enum AVCodecID id) {
    LUMA_AV_OUTCOME_TRY(codec_ptr, detail::find_decoder(id));
    return codec{codec_ptr};
}
inline result<codec> find_decoder(const std::string& name) {
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

    codec_parameters() : base_type{alloc_codec_par().value()} {
    }

    codec_parameters(const AVCodecParameters* par) 
        : base_type{alloc_codec_par().value()} {
        copy_par(this->get(), par).value();
    }

    codec_parameters(const codec_parameters& other) : base_type{alloc_codec_par().value()} {
        copy_par(this->get(), other.get()).value();
    }

    codec_parameters& operator=(const codec_parameters& other) {
        // think this is all you need. it should completely overwrite
        //  the first codec par with the second
        copy_par(this->get(), other.get()).value();
        return *this;
    }

    codec_parameters(codec_parameters&&) = default;
    codec_parameters& operator=(codec_parameters&&) = default;


    private:
    // prob all static in the cpp eventually?
    //  something less visible than private functions
    result<base_type> alloc_codec_par() {
        auto par =  avcodec_parameters_alloc();
        if (par) {
            return base_type{par};
        } else {
            return errc::alloc_failure;
        }
    }
    result<void> copy_par(AVCodecParameters* out_par, const AVCodecParameters* par) {
        return detail::ffmpeg_code_to_result(avcodec_parameters_copy(out_par, par));
    }
};

class codec_context : public detail::unique_or_null<AVCodecContext, detail::codec_context_deleter>{

    public:
    using base_type = detail::unique_or_null<AVCodecContext, detail::codec_context_deleter>;
    using base_type::get;

    explicit codec_context(const codec& codec) 
        : base_type{alloc_context(codec).value()}, codec_{codec.get()}{

    }

    explicit codec_context(const codec& codec, const codec_parameters& par) 
        : codec_context{codec} {
        codec_ctx_from_par(this->get(), par.get()).value();
    }

    codec_context(const codec_context&) = delete;
    codec_context& operator=(const codec_context&) = delete;

    result<codec_parameters> codec_par() const {
        auto par = codec_parameters{};
        LUMA_AV_OUTCOME_TRY(codec_par_from_ctx(par.get(), this->get()));
        return par;
    }


    result<void> open(AVDictionary**  options) {
        auto ec = avcodec_open2(this->get(), codec_, options);
        return detail::ffmpeg_code_to_result(ec);
    }

    bool is_open() const noexcept {
        // this is ffmpegs implementation
        //  idk why this function isnt const
        // https://ffmpeg.org/doxygen/3.2/group__lavc__misc.html#ga906dda732e79eac12067c6d7ea19b630
        return !!this->get()->internal;
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
    result<void> send_packet(const AVPacket* p) {
        auto ec = avcodec_send_packet(this->get(), p);
        return detail::ffmpeg_code_to_result(ec);
    }

    // convenience overload for our own packet
    result<void> send_packet(const packet& p) {
        return this->send_packet(p.get());
    }

    result<void> send_frame(const AVFrame* f) {
        auto ec = avcodec_send_frame(this->get(), f);
        return detail::ffmpeg_code_to_result(ec);
    }

    result<void> recieve_frame(AVFrame* frame) {
        auto ec = avcodec_receive_frame(this->get(), frame);
        return detail::ffmpeg_code_to_result(ec);
    }
    result<void> recieve_frame(frame& frame) {
        auto ec = avcodec_receive_frame(this->get(), frame.get());
        return detail::ffmpeg_code_to_result(ec);
    }

    result<void> recieve_packet(packet& p) {
        auto ec = avcodec_receive_packet(this->get(), p.get());
        return detail::ffmpeg_code_to_result(ec);
    }
    result<void> recieve_packet(AVPacket* p) {
        auto ec = avcodec_receive_packet(this->get(), p);
        return detail::ffmpeg_code_to_result(ec);
    }

    // convenience to go from packet to frame in one call
    result<frame> decode(const AVPacket* p) {
        LUMA_AV_OUTCOME_TRY(this->send_packet(p));
        LUMA_AV_OUTCOME_TRY(this->recieve_frame(decoder_frame_));
        return frame{decoder_frame_.get()};
    }
    // overload for luma av packet

    // same frame overloads from send_frame would
    //  be supported here
    result<packet> encode(const AVFrame* f) {
        LUMA_AV_OUTCOME_TRY(this->send_frame(f));
        LUMA_AV_OUTCOME_TRY(this->recieve_packet(encoder_packet_));
        return packet{encoder_packet_.get()};
    }

    private:
    // static in cpp
    result<base_type> alloc_context(const codec& codec) {
        auto ctx = avcodec_alloc_context3(codec.get());
        if (ctx) {
            return base_type{ctx};
        } else {
            // i think this is always an alloc failure?
            return errc::alloc_failure;
        }
    }
    result<void> codec_par_from_ctx(AVCodecParameters* par, const AVCodecContext* ctx) const {
        return detail::ffmpeg_code_to_result(avcodec_parameters_from_context(par, ctx));
    }
    result<void> codec_ctx_from_par(AVCodecContext* ctx, const AVCodecParameters* par) const {
        return detail::ffmpeg_code_to_result(avcodec_parameters_to_context(ctx, par));
    }
    const AVCodec* codec_ = nullptr; 
    frame decoder_frame_;
    packet encoder_packet_;
};




} // av
} // luma

#endif // LUMA_AV_CODEC_HPP