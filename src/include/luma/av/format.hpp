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

using unique_or_null_format_ctx = detail::unique_or_null<AVFormatContext, 
                                                        detail::format_context_deleter>

// todo static in cpp
inline result<unique_or_null_format_ctx> alloc_format_ctx() noexcept {
    auto ctx = avformat_alloc_context();
    if (ctx) {
        return unique_or_null_format_ctx{ctx};
    } else {
        return luma::av::make_error_code(errc::alloc_failure);
    }
}

inline result<void> open_input(AVFormatContext** ps, const char* url,
		                AVInputFormat* fmt, AVDictionary** options) noexcept {
    return detail::ffmpeg_code_to_result(avformat_open_input(ps, url, fmt, options));
} 	

inline result<void> find_stream_info(AVFormatContext* ic,
		                      AVDictionary** options) noexcept {
    return detail::ffmpeg_code_to_result(avformat_find_stream_info(ic, options));
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
    : public unique_or_null_format_ctx {

    public:
    using base_type = unique_or_null_format_ctx;
    
    format_context() noexcept(luma::av::noexcept_novalue) 
        : base_type{alloc_format_ctx().value()} {

    }

    /*
    */
    format_context(const char* url) noexcept(luma::av::noexcept_novalue)
     : base_type{nullptr} {
        AVFormatContext* fctx = nullptr;
        detail::open_input(&fctx, url, nullptr, nullptr).value();
        this->reset(fctx);
        this->find_stream_info(nullptr).value();
    }

    result<void> find_stream_info(AVDictionary** options) noexcept {
        return detail::find_stream_info(this->get(), options);
    }

    // lighest weight easiest to misuse
    result<void> read_frame(AVPacket* pkt) noexcept {
        return detail::ffmpeg_code_to_result(av_read_frame(this->get(), pkt));
    }
    // if the user wants to manage the packet themselves
    result<void> read_frame(packet& pkt) noexcept {
        return this->read_frame(this->get());
    }
    // if the user always wants a copy of the packet
    result<packet> read_frame() noexcept(luma::av::noexcept_novalue) {
        auto pkt = packet{};
        LUMA_AV_OUTCOME_TRY(this->read_frame(pkt));
        return pkt;
    }
};


} // av
} // luma

#endif // LUMA_AV_FORMAT_HPP