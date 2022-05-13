

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


// we're gona redirect the ffmpeg logger to these (:
//  dont think theres any other way to pass these to ffmpeg besides global
static std::stringstream luma_output;
static std::stringstream ffmpeg_output;


static void log_callback_null(void *ptr, int level, const char *fmt, va_list vl){}

static void log_callback_luma_av(void *ptr, int level, const char *fmt, va_list vl)
{
    char buff[1024];
    const auto num_read = vsprintf(buff, fmt, vl);
    luma_output << std::string(buff, num_read);
}

static void log_callback_ffmpeg(void *ptr, int level, const char *fmt, va_list vl)
{
    char buff[1024];
    const auto num_read = vsprintf(buff, fmt, vl);
    ffmpeg_output << std::string(buff, num_read);
}


static auto kFileName = "./test_vids/fortnite_uwu.mp4";

struct BufferData {
    uint8_t *ptr;
    size_t size; ///< size left in the buffer
};
static void LumaAVReadExample() {
    const auto input_filename = luma_av::cstr_view{kFileName};
    auto map_buff = luma_av::MappedFileBuff::make(input_filename).value();

    auto custom_reader = 
            [bd = BufferData{map_buff.data(), map_buff.size()}] 
            (uint8_t *buf, int buf_size) mutable -> int {
        buf_size = FFMIN(buf_size, bd.size);
        if (!buf_size)
            return AVERROR_EOF;
        luma_output << bd.size << "\n";
        /* copy internal buffer data to buf */
        memcpy(buf, bd.ptr, buf_size);
        bd.ptr  += buf_size;
        bd.size -= buf_size;
        return buf_size;
    };

    auto io_callbacks = luma_av::CustomIOFunctions{}.CustomRead(std::move(custom_reader));
    constexpr auto avio_ctx_buffer_size = int{4096};
    auto custom_io = luma_av::IOContext::make(avio_ctx_buffer_size, std::move(io_callbacks)).value();

    auto fctx = luma_av::format_context::open_input(std::move(custom_io)).value();
    fctx.FindStreamInfo().value();

    av_log_set_callback(log_callback_luma_av);
    av_dump_format(fctx.get(), 0, input_filename.c_str(), 0);
    av_log_set_callback(av_log_default_callback);
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
    ffmpeg_output << bd->size << "\n";
    /* copy internal buffer data to buf */
    memcpy(buf, bd->ptr, buf_size);
    bd->ptr  += buf_size;
    bd->size -= buf_size;
    return buf_size;
}
static void FfmpegReaderExample() {
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


    av_log_set_callback(log_callback_ffmpeg);
    av_dump_format(fmt_ctx, 0, input_filename, 0);
    av_log_set_callback(av_log_default_callback);


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

TEST(AVIOreadTests, FfmpegCompare) {
    LumaAVReadExample();
    FfmpegReaderExample();
    auto luma_str = std::string{std::move(luma_output).str()};
    auto ffmpeg_str = std::string{std::move(ffmpeg_output).str()};
    ASSERT_EQ(luma_str, ffmpeg_str);
}



/**
verify that our MappedFileBuff aggrees with av_file_map on the size of the file
*/
TEST(AVIOreadTests, file_map) {
    uint8_t *buffer = NULL;
    size_t buffer_size;
    const auto input_filename = luma_av::cstr_view{kFileName};
    ASSERT_EQ(av_file_map(input_filename.c_str(), &buffer, &buffer_size, 0, NULL), 0);
    av_file_unmap(buffer, buffer_size);
    
    const auto map_buff = luma_av::MappedFileBuff::make(input_filename).value();
    ASSERT_EQ(map_buff.size(), buffer_size);
}

