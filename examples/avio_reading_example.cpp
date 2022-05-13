// example based on https://ffmpeg.org/doxygen/trunk/avio_reading_8c-example.html

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/file.h>
}

#include <fstream>
#include <gtest/gtest.h>
#include <luma_av/codec.hpp>
#include <luma_av/format.hpp>
#include <luma_av/parser.hpp>
#include <luma_av/swscale.hpp>

static auto kFileName = "./test_vids/fortnite_uwu.mp4";


struct BufferData {
    uint8_t *ptr;
    size_t size; ///< size left in the buffer
};
TEST(AvioReadingExample, MyExample) { 

    const auto input_filename = luma_av::cstr_view{kFileName};
    auto map_buff = luma_av::MappedFileBuff::make(input_filename).value();

    auto custom_reader = 
            [bd = BufferData{map_buff.data(), map_buff.size()}] 
            (uint8_t *buf, int buf_size) mutable -> int {
        // NOLINTBEGIN
        buf_size = FFMIN(buf_size, bd.size);
        if (!buf_size)
            return AVERROR_EOF;
        printf("ptr:%p size:%zu\n", bd.ptr, bd.size);
        /* copy internal buffer data to buf */
        memcpy(buf, bd.ptr, buf_size);
        bd.ptr  += buf_size;
        bd.size -= buf_size;
        return buf_size;
        // NOLINTEND
    };

    auto io_callbacks = luma_av::CustomIOFunctions{}.CustomRead(std::move(custom_reader));
    constexpr auto avio_ctx_buffer_size = int{4096};
    auto custom_io = luma_av::IOContext::make(avio_ctx_buffer_size, std::move(io_callbacks)).value();

    auto fctx = luma_av::format_context::open_input(std::move(custom_io)).value();
    fctx.FindStreamInfo().value();

    av_dump_format(fctx.get(), 0, input_filename.c_str(), 0);
}

// NOLINTBEGIN 
struct buffer_data {
    uint8_t *ptr;
    size_t size; ///< size left in the buffer
};
static int read_packet(void *opaque, uint8_t *buf, int buf_size)
{
    struct buffer_data *bd = (struct buffer_data *)opaque;
    buf_size = FFMIN(buf_size, bd->size);
    if (!buf_size)
        return AVERROR_EOF;
    printf("ptr:%p size:%zu\n", bd->ptr, bd->size);
    /* copy internal buffer data to buf */
    memcpy(buf, bd->ptr, buf_size);
    bd->ptr  += buf_size;
    bd->size -= buf_size;
    return buf_size;
}
TEST(AvioReadingExample, FfmpegExample) {
    AVFormatContext *fmt_ctx = NULL;
    AVIOContext *avio_ctx = NULL;
    uint8_t *buffer = NULL, *avio_ctx_buffer = NULL;
    size_t buffer_size, avio_ctx_buffer_size = 4096;
    char *input_filename = NULL;
    int ret = 0;
    struct buffer_data bd = { 0 };
    // if (argc != 2) {
    //     fprintf(stderr, "usage: %s input_file\n"
    //             "API example program to show how to read from a custom buffer "
    //             "accessed through AVIOContext.\n", argv[0]);
    //     return 1;
    // }
    input_filename = const_cast<char*>(kFileName);
    /* slurp file content into buffer */
    ret = av_file_map(input_filename, &buffer, &buffer_size, 0, NULL);
    if (ret < 0)
        goto end;
    /* fill opaque structure used by the AVIOContext read callback */
    bd.ptr  = buffer;
    bd.size = buffer_size;
    if (!(fmt_ctx = avformat_alloc_context())) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    avio_ctx_buffer = (uint8_t*)av_malloc(avio_ctx_buffer_size);
    if (!avio_ctx_buffer) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    avio_ctx = avio_alloc_context(avio_ctx_buffer, avio_ctx_buffer_size,
                                  0, &bd, &read_packet, NULL, NULL);
    if (!avio_ctx) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    fmt_ctx->pb = avio_ctx;
    ret = avformat_open_input(&fmt_ctx, NULL, NULL, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not open input\n");
        goto end;
    }
    ret = avformat_find_stream_info(fmt_ctx, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not find stream information\n");
        goto end;
    }
    av_dump_format(fmt_ctx, 0, input_filename, 0);
end:
    avformat_close_input(&fmt_ctx);
    /* note: the internal buffer could have changed, and be != avio_ctx_buffer */
    if (avio_ctx)
        av_freep(&avio_ctx->buffer);
    avio_context_free(&avio_ctx);
    av_file_unmap(buffer, buffer_size);
    if (ret < 0) {
        fprintf(stderr, "Error occurred: %s\n", "av_err2str(ret)");
    }
}
// NOLINTEND
