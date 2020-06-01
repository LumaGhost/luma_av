#ifndef LUMA_AV_PACKET_HPP
#define LUMA_AV_PACKET_HPP

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <memory>

#include <luma/av/result.hpp>
#include <luma/av/detail/unique_or_null.hpp>

// https://ffmpeg.org/doxygen/3.2/group__lavc__packet.html

namespace luma {
namespace av {


namespace detail {

inline result<void> make_buffer_writable(AVBufferRef** buffy) noexcept {
    return detail::ffmpeg_code_to_result(av_buffer_make_writable(buffy));
}

inline result<void> packet_copy_props(AVPacket* dst, const AVPacket* src) noexcept {
    return detail::ffmpeg_code_to_result(av_packet_copy_props(dst, src));
}

inline result<void> packet_ref(AVPacket* src, const AVPacket* dst) noexcept {
    return detail::ffmpeg_code_to_result(av_packet_ref(src, dst));
}

inline result<void> make_packet_writable(AVPacket* pkt) noexcept {
    // then can i just do this and be good?
    LUMA_AV_OUTCOME_TRY(make_buffer_writable(&pkt->buf));
    return luma::av::outcome::success();
}

// i think this is a fully deep copy with unique ownership
inline result<void> packet_copy(AVPacket* dst, const AVPacket* src) noexcept {
    LUMA_AV_OUTCOME_TRY(packet_copy_props(dst, src));
    LUMA_AV_OUTCOME_TRY(packet_ref(dst, src));
    LUMA_AV_OUTCOME_TRY(make_buffer_writable(&dst->buf));
    return luma::av::outcome::success();
}

struct packet_deleter {
    void operator()(AVPacket* pkt) const noexcept {
        av_packet_free(&pkt);
        // looks like this is deprecated and av_packet_unref is the 
        //  one we should use instead?
    }
};

using unique_or_null_packet = detail::unique_or_null<AVPacket, packet_deleter>;

// todo static in cpp
/**
    * allocaate a new packet with avpacket alloc, call avpacket init on the resulting packet
    *  avpacket init performs additional initialization beyond what av packet alloc does
    *  https://ffmpeg.org/doxygen/3.2/avpacket_8c_source.html#l00033
    */
inline result<unique_or_null_packet> alloc_packet() noexcept {
    auto pkt = av_packet_alloc();
    if (pkt) {
        av_init_packet(pkt);
        return unique_or_null_packet{pkt};
    } else {
        return luma::av::make_error_code(errc::alloc_failure);
    }
}

inline result<void> new_packet(AVPacket* pkt, int size) noexcept {
    return detail::ffmpeg_code_to_result(av_new_packet(pkt, size));
}

} // detail

class packet : public detail::unique_or_null_packet {

    public:

    using base_type = detail::unique_or_null_packet;

    packet() noexcept(luma::av::noexcept_novalue) 
        : base_type{detail::alloc_packet().value()}{

    }

    // strong type?
    explicit packet(int size) noexcept(luma::av::noexcept_novalue)
     : packet{} {
        detail::new_packet(this->get(), size).value();
    }

    explicit packet(const AVPacket* pkt) noexcept(luma::av::noexcept_novalue)
     : packet{} {
        detail::packet_copy(this->get(), pkt).value();
    }

    packet(const packet&) = delete;
    packet& operator=(const packet&) = delete;

    packet(packet&&) noexcept = default;
    packet& operator=(packet&&) noexcept = default;

};

} // av
} // luma

#endif 