#ifndef LUMA_AV_PACKET_HPP
#define LUMA_AV_PACKET_HPP

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <memory>
#include <span>

#include <luma_av/result.hpp>
#include <luma_av/util.hpp>

// https://ffmpeg.org/doxygen/3.2/group__lavc__packet.html

namespace luma_av {

namespace detail {

inline void packet_buffer_unref(NotNull<AVPacket*> pkt) noexcept {
    av_buffer_unref(&pkt->buf);
    pkt->data = nullptr;
    pkt->size = 0;
}

}

/**
 wrapper for AVPacket
 this class maintains the reference counting semantics available with AVPacket
 each Packet contains unique ownership of an AVPacket. but the AVPacket itself can potentially
 own or share ownership of a buffer.
 move only, but reference counted copies can be created explicity using the `make` methods
 */
class Packet {

    struct packet_deleter {
        void operator()(AVPacket* pkt) const noexcept {
            av_packet_free(&pkt);
            // looks like this is deprecated and av_packet_unref is the 
            //  one we should use instead?
        }
    };
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

    Packet(AVPacket* pkt) noexcept : pkt_{pkt} {

    }

    public:

    /**
    create a new avpacket and initialize to default values
    does not intitialze the packets buffer.
    */
    static result<Packet> make() noexcept {
        LUMA_AV_OUTCOME_TRY(pkt, alloc_packet());
        return Packet{pkt.release()};
    }

    /**
    allocate a packet and initialize with a new buffer. specifying the size of the buffer
    */
    static result<Packet> make(int size) noexcept {
        LUMA_AV_OUTCOME_TRY(pkt, Packet::make());
        LUMA_AV_OUTCOME_TRY(pkt.new_buffer(size));
        // we're actually calling the result ctor here so we need to move
        //  otherwise it tries to find copy (and fails)
        return std::move(pkt);
    }

    static result<Packet> make(const AVPacket* in_pkt) noexcept {
        LUMA_AV_ASSERT(in_pkt);
        LUMA_AV_OUTCOME_TRY(pkt, Packet::make());
        LUMA_AV_OUTCOME_TRY_FF(av_packet_copy_props(pkt.pkt_.get(), in_pkt));
        LUMA_AV_OUTCOME_TRY_FF(av_packet_ref(pkt.pkt_.get(), in_pkt));
        return std::move(pkt);
    }
    static result<Packet> make(const Packet& in_pkt) noexcept {
        return Packet::make(in_pkt.get());
    }

    Packet(const Packet&) = delete;
    Packet& operator=(const Packet&) = delete;

    Packet(Packet&&) noexcept = default;
    Packet& operator=(Packet&&) noexcept = default;

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

    const AVPacket* release() && noexcept {
        return pkt_.release();
    }

    /**
        view of the packets internal buffer
    */
    std::span<const uint8_t> span() const noexcept {
        LUMA_AV_ASSERT(this->has_buffer());
        return {pkt_.get()->data, static_cast<std::size_t>(pkt_.get()->size)};
    }
    std::span<uint8_t> span() noexcept {
        LUMA_AV_ASSERT(this->has_buffer());
        LUMA_AV_ASSERT(this->is_writable());
        return {pkt_.get()->data, static_cast<std::size_t>(pkt_.get()->size)};
    }

    /**
     returns true if the packet has a buffer attached. does not 
     ensure unique ownership of the buffer
    */
    bool has_buffer() const noexcept {
        if (!pkt_->buf) {
            return false;
        } else {
            LUMA_AV_ASSERT(pkt_->data);
            LUMA_AV_ASSERT(pkt_->size > 0);
            return true;
        }
    }

    /**
    replace the current buffer with a newly allocated buffer of a specificed size
    */
    result<void> new_buffer(int size) noexcept {
        if (this->has_buffer()) {
            detail::packet_buffer_unref(pkt_.get());
        }
        LUMA_AV_OUTCOME_TRY_FF(av_new_packet(pkt_.get(), size));
        return luma_av::outcome::success();
    }

    /**
        replace the internal buffer with a new buffer. ownership is transfered from
        the caller to the packet. 
    */
    result<void> reset_buffer(luma_av::Owner<uint8_t*> data, int size) noexcept {
        // either ref coutned with on ref, or no buffer at all
        // otherwise i think this function leaks
        if (this->has_buffer()) {
            detail::packet_buffer_unref(pkt_.get());
        }
        LUMA_AV_OUTCOME_TRY_FF(av_packet_from_data(pkt_.get(), data, size));
        return luma_av::outcome::success();
    }
    /**
        replace the internal buffer with a new buffer. rather than transfering ownership,
        a new buffer is allocated and the input data is copied into the new buffer
    */
    result<void> reset_buffer_copy(std::span<const uint8_t> data) noexcept {
        const auto buff_size = static_cast<int>(data.size());
        auto buff = static_cast<uint8_t*>(av_malloc(buff_size));
        LUMA_AV_ASSERT(buff);
        std::copy(data.begin(), data.end(), buff);
        LUMA_AV_OUTCOME_TRY(this->reset_buffer(buff, buff_size));
        return luma_av::outcome::success();
    }

    /**
      if the buffer has more than one owner, 
      create a new buffer and copy the contents of the current buffer.
      the resulting buffer is writable i.e. there is only one owner
     */
    result<void> make_writable() {
        LUMA_AV_ASSERT(this->has_buffer());
        LUMA_AV_OUTCOME_TRY_FF(av_buffer_make_writable(&pkt_->buf));
        return luma_av::outcome::success();
    }

    /**
     whether or not the frame is "writable" i.e. the packet buffer has only one owner
    */
    bool is_writable() const noexcept {
        // non ref counted frames are never assumed writable
        // https://www.ffmpeg.org/doxygen/trunk/frame_8c_source.html#l00595
        if (!this->has_buffer()) {
            return false;
        }
        return av_buffer_is_writable(pkt_->buf) == 1;
    }

};

} // luma_av

#endif 