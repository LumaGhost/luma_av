#ifndef LUMA_AV_PACKET_HPP
#define LUMA_AV_PACKET_HPP

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <memory>

#include <luma/av/result.hpp>

// https://ffmpeg.org/doxygen/3.2/group__lavc__packet.html

namespace luma {
namespace av {

class packet {

    public:
    // null functionality

    // default actually allocates and gives a usable packet
    packet() : pkt_{alloc_packet().value()}{

    }

    // strong type?
    explicit packet(int size) : packet{} {
        new_packet(pkt_.get(), size).value();
    }

    // there is no packet equivalent of avfame_make_writable
    // but looking here https://ffmpeg.org/doxygen/2.5/frame_8c_source.html#l00411
    // at the source of avfame_make_writable we can get about the same 
    //  solution by creating an avpacket on the stack then doing whatever
    //  with it e.g. passing it to an encoder then doing a deep copy of the packet
    explicit packet(const AVPacket* pkt) : packet{} {
        copy_packet(pkt_.get(), pkt).value();
    }

    // packets are too big for implicit copy?
    // if we do copies i think av_copy_packet does a full deep copy
    // https://ffmpeg.org/doxygen/3.2/avpacket_8c_source.html#l00264
    packet(const packet&) = delete;
    packet& operator=(const packet&) = delete;

    // the only two options ive come up for move semantics
    //  are either a nullptr state or policies
    // policies are just so much and they make for expensive moves
    //  (packet/frame/whatever alloc)
    // null states make for really cheap moves but they can be annoying for the user
    //  and also me making the class
    // i do like the null state a lot more when the default ctor doesnt result in null
    //   just makes the class feel a lot more natural to use
    // with a null state we will have 3 states though like avframe would
    //  completely null, no buffers, buffers
    packet(packet&&) = default;
    packet& operator=(packet&&) = default;

    // i guess we can go with get here isntead of like avpacket_ptr
    //  so that we can plug in all of the null functionality with crtp?
    AVPacket* get() noexcept {
        return pkt_.get();
    }
const AVPacket* get() const noexcept {
        return pkt_.get();
    }

    private:
    struct packet_deleter {
        void operator()(AVPacket* pkt) const noexcept {
            av_packet_free(&pkt);
            // looks like this is deprecated and av_packet_unref is the 
            //  one we should use instead?
        }
    };
    using packet_ptr = std::unique_ptr<AVPacket, packet_deleter>;
    // static in cpp prob
    // https://ffmpeg.org/doxygen/3.2/avpacket_8c_source.html#l00051
    //  packet alloc zero initialzes all of the packet memory
    //  worth calling avpacket init on top of that?
    //  yes there are some non zero defaults https://ffmpeg.org/doxygen/3.2/avpacket_8c_source.html#l00033
    result<packet_ptr> alloc_packet() {
        auto pkt = av_packet_alloc();
        if (pkt) {
            av_init_packet(pkt);
            return packet_ptr{pkt};
        } else {
            return errc::alloc_failure;
        }
    }
    // alloc packet and buffers
    // this resets the packet to default fields
    //  so its a little different than allocating the frame buffers
    result<void> new_packet(AVPacket* pkt, int size) {
        return errc{av_new_packet(pkt, size)};
    }
    result<void> copy_packet(AVPacket* dst, const AVPacket* src) {
        // todo ugh its deprecated. why doesnt ffmpeg fuck with
        //  deep copies of avpacket?
        return errc{av_copy_packet(dst, src)};
    }
    packet_ptr pkt_ = nullptr;

};

} // av
} // luma

#endif 