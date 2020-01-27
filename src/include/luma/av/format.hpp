#ifndef LUMA_AV_FORMAT_HPP
#define LUMA_AV_FORMAT_HPP


/*
https://www.ffmpeg.org/doxygen/3.3/group__libavf.html
*/

extern "C" {
#include <libavformat/avformat.h>
}

#include <luma/av/packet.hpp>
#include <luma/av/result.hpp>

namespace luma {
namespace av {

namespace detail {

struct format_context_deleter {
    void operator()(AVFormatContext* fctx) const noexcept {
        // think close input is enough to completely free in all cases
        avformat_close_input(&fctx);
        // avformat_free_context(fctx);
    }
};

result<void> open_input(AVFormatContext** ps, const char* url,
		                AVInputFormat* fmt, AVDictionary** options) {
    return errc{avformat_open_input(ps, url, fmt, options)};
} 	

result<void> find_stream_info(AVFormatContext* ic,
		                      AVDictionary** options) {
    return errc{avformat_find_stream_info(ic, options)};
}

} // detail


/*
Most importantly an AVFormatContext contains:
    the input or output format. It is either autodetected or set by user for input;
        always set by user for output.
    an array of AVStreams, which describe all elementary streams stored in the file.
        AVStreams are typically referred to using their index in this array.
    an I/O context. It is either opened by lavf or set by user for input,
        always set by user for output (unless you are dealing with an AVFMT_NOFILE format).

*/
class format_context 
    : public detail::unique_or_null<AVFormatContext,
                                    detail::format_context_deleter> {

    public:
    using base_type = detail::unique_or_null<AVFormatContext,
                                             detail::format_context_deleter>;
    
    // how useful is this without exposing functions
    //  around opening the context and finding streams
    format_context() : base_type{alloc_format_ctx().value()} {

    }

    // would like sv but null terminated
    //  is const char* the next best thing?
    // or std::string? const char* doesnt 
    //  guarentee null terminated so is it even worth?
    /*
    "Note that a user-supplied AVFormatContext will be freed on failure."
        ive heard that this is pretty inconvenient for users.
        maybe we can have a constructor that helps users 
        have a more convenient way to deal with constructing
        from an existing context
    format options? avdictionary?
    feel like anything to do with AVFormat directly the user
        can use ffmpegs api? i dont think any of them are owning
        and im pretty sure the likes of av_probe_input_format and company
        are all for advanced use
    good default to call find stream info for the user?
    also this const char* constructor is based on the reader
        side of the format context. in writing the "url" would
        be where the new video or whatever will be saved
        may need two separate format context types to avoid that confusion
    */
    format_context(const char* url) : base_type{nullptr} {
        AVFormatContext* fctx = nullptr;
        detail::open_input(&fctx, url, nullptr, nullptr).value();
        this->reset(fctx);
        this->find_stream_info(nullptr);
    }

    result<void> find_stream_info(AVDictionary** options) {
        return detail::find_stream_info(this->get(), options);
    }

    // lighest weight easiest to misuse
    result<void> read_frame(AVPacket* pkt) noexcept {
        return errc{av_read_frame(this->get(), pkt)};
    }
    // if the user wants to manage the packet themselves
    result<void> read_frame(packet& pkt) noexcept {
        return this->read_frame(this->get());
    }
    // if the user always wants a copy of the packet
    result<packet> read_frame() {
        // packet should prob be a member
        //  so we dont waste allocations when read frame fails
        //  and it def will in real world network conditions
        auto pkt = packet{};
        LUMA_AV_OUTCOME_TRY(this->read_frame(pkt));
        return pkt;
    }
    // todo read frame should set the media type of the packet
    // other helper functions for working with the stream array in the format context

    private:
    result<base_type> alloc_format_ctx() {
        auto ctx = avformat_alloc_context();
        if (ctx) {
            return base_type{ctx};
        } else {
            return errc::alloc_failure;
        }
    }
};


} // av
} // luma

#endif // LUMA_AV_FORMAT_HPP