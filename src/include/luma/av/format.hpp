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

    using unique_fctx = detail::unique_ptr<AVFormatContext,
                                           detail::format_context_deleter>


    /**
    static private is generally awk i think but in this case i think we actually do want a class local free function
    that supports the private impl. ig the argument of u should use a private class instead still applies. 
    but because result makes ctors awkward a class here would just be a clunkier version of what we have
    (a unique ptr and a free function)
    */
    static result<unique_fctx> alloc_format_ctx() noexcept {
        auto ctx = avformat_alloc_context();
        if (ctx) {
            return unique_or_null_format_ctx{ctx};
        } else {
            return luma::av::make_error_code(errc::alloc_failure);
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

    static result<format_context> make(const char* url) noexcept {
        AVFormatContext* fctx = nullptr;\
        LUMA_AV_OUTCOME_TRY_FF(avformat_open_input(&fctx, url, nullptr, nullptr));
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
        return this->read_frame(this->get());
    }
    // if the user always wants a copy of the packet
    result<packet> read_frame() noexcept {
        LUMA_AV_OUTCOME_TRY(pkt, packet::make());
        LUMA_AV_OUTCOME_TRY(this->read_frame(pkt));
        return pkt;
    }
};


} // av
} // luma

#endif // LUMA_AV_FORMAT_HPP