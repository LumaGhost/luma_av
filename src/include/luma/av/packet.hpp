#ifndef LUMA_AV_PACKET_HPP
#define LUMA_AV_PACKET_HPP

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <memory>

#include <luma/av/result.hpp>
#include <luma/av/detail/unique_or_null.hpp>

// https://ffmpeg.org/doxygen/3.2/group__lavc__packet.html

namespace luma_av {

class packet {

    struct packet_deleter {
        void operator()(AVPacket* pkt) const noexcept {
            av_packet_free(&pkt);
            // looks like this is deprecated and av_packet_unref is the 
            //  one we should use instead?
        }
    };
    using This = packet;
    using unique_pkt = std::unique_ptr<AVPacket, packet_deleter>;

/**
    * allocaate a new packet with avpacket alloc, call avpacket init on the resulting packet
    *  avpacket init performs additional initialization beyond what av packet alloc does
    *  https://ffmpeg.org/doxygen/3.2/avpacket_8c_source.html#l00033
    */
    static result<unique_pkt> alloc_packet() noexcept {
        auto pkt = av_packet_alloc();
        if (pkt) {
            av_init_packet(pkt);
            return unique_pkt{pkt};
        } else {
            return luma_av::make_error_code(errc::alloc_failure);
        }
    }

    unique_pkt pkt_;

    packet(AVPacket* pkt) noexcept : pkt_{pkt} {

    }

    public:

    static result<packet> make() noexcept {
        LUMA_AV_OUTCOME_TRY(pkt, alloc_packet());
        return packet{pkt.release()};
    }

    static result<packet> make(int size) noexcept {
        LUMA_AV_OUTCOME_TRY(pkt, This::make());
        LUMA_AV_OUTCOME_TRY_FF(av_new_packet(pkt.pkt_.get(), size));
        // we're actually calling the result ctor here so we need to move
        //  otherwise it tries to find copy (and fails)
        return std::move(pkt);
    }

    /**
    not sure how the invariant will work out yet but i rly want to support shalow copies. thats a huge
    optimization i feel like itd be untrue to the lib to leave it off the table. that said
    i def want an api thats clear abt shalow or deep copy and allows the user to know explicitly if 
    theyre creating/having a writable packet or not. 
    i wanted to keep the names uniformly "make" so at that point a tag type helps disambiguate
    */
    struct shallow_copy_t {};
    static constexpr auto shallow_copy = shallow_copy_t{}; 
    static result<packet> make(const AVPacket* in_pkt, shallow_copy_t) noexcept {
        LUMA_AV_OUTCOME_TRY(pkt, This::make());
        LUMA_AV_OUTCOME_TRY_FF(av_packet_copy_props(pkt.pkt_.get(), in_pkt));
        LUMA_AV_OUTCOME_TRY_FF(av_packet_ref(pkt.pkt_.get(), in_pkt));
        return std::move(pkt);
    }
    static result<packet> make(const packet& in_pkt, shallow_copy_t) noexcept {
        return This::make(in_pkt.get(), shallow_copy);
    }

    static result<packet> make(const AVPacket* in_pkt) noexcept {
        LUMA_AV_OUTCOME_TRY(pkt, This::make(in_pkt, shallow_copy));
        LUMA_AV_OUTCOME_TRY_FF(av_buffer_make_writable(&pkt.pkt_->buf));
        return std::move(pkt);
    }

    packet(const packet&) = delete;
    packet& operator=(const packet&) = delete;

    packet(packet&&) noexcept = default;
    packet& operator=(packet&&) noexcept = default;

    /**
    maybe change the name. but for now a tiny amnt of code uses this so im keeping it for now
    to me this is potentially a "trait". in the general software sense just something abt a group of types
    thats intentionally the same ability. e.g. in this case the ability return the underlying raw ptr
    in the c++ sense thats a concept. no need to derive or use virtual or anyhing :puke: just implement
    some functionality with a standard interface (:
    or we could leave it up to the individual classes
    */
    AVPacket* get() noexcept {
        return pkt_.get();
    }

    const AVPacket* get() const noexcept {
        return pkt_.get();
    }


};

} // luma_av

#endif 