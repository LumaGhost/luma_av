// example based on https://ffmpeg.org/doxygen/trunk/decode_video_8c-example.html

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern "C" {
#include <libavcodec/avcodec.h>
}

#include <algorithm>
#include <fstream>
#include <gtest/gtest.h>
#include <luma_av/codec.hpp>
#include <luma_av/parser.hpp>
#include <luma_av/swscale.hpp>

static auto kFileName = "./test_vids/fortnite_mpeg1_cut.mp4";
static auto kOutputFile = "./test_vids/outputs/output_uwu";

// NOLINTBEGIN
#define INBUF_SIZE 4096
static void pgm_save(const uint8_t* buf, int wrap, int xsize, int ysize,
                     const char *filename)
{
    FILE *f;
    int i;
    f = fopen(filename,"wb");
    fprintf(f, "P5\n%d %d\n%d\n", xsize, ysize, 255);
    for (i = 0; i < ysize; i++)
        fwrite(buf + i * wrap, 1, xsize, f);
    fclose(f);
}
// NOLINTEND

TEST(DecodeVideoExample, MyExample) {
    auto parser = luma_av::Parser::make(AV_CODEC_ID_MPEG1VIDEO).value();
    auto decoder = luma_av::Decoder::make(AV_CODEC_ID_MPEG1VIDEO).value();
    auto filename    = kFileName;
    auto outfilename = kOutputFile;
    FILE* f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Could not open %s\n", filename);
        exit(1);
    }
    auto fc = luma_av::detail::finally([&](){
        fclose(f);
    });
    // NOLINTBEGIN
    uint8_t inbuf[INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
    /* set end of buffer to 0 (this ensures that no overreading happens for damaged MPEG streams) */
    memset(inbuf + INBUF_SIZE, 0, AV_INPUT_BUFFER_PADDING_SIZE);
    // NOLINTEND

    int frame_num = 0;
    auto save_frames = [&](const auto& res) {
        if (!res) {
            std::cout << "error!!! " << res.error().message() << std::endl;
        }
        auto&& frame = *res.value();
        // NOLINTBEGIN
        printf("saving frame %3d\n", frame_num);
        fflush(stdout);
        char buf[1024];
        /* the picture is allocated by the decoder. no need to
        free it */
        snprintf(buf, sizeof(buf), "%s-%d.pgm", outfilename, frame_num);
        pgm_save(frame.data()[0], frame.linesize()[0],
            frame.width(), frame.height(), buf);
        ++frame_num;
        // NOLINTEND
    };
    while (!feof(f)) {
        /* read raw data from the input file */
        auto data_size = fread(inbuf, 1, INBUF_SIZE, f);
        if (!data_size)
            break;
        std::span<const uint8_t> data(inbuf, data_size);
        // this is a bug idk why i need to make a vector
        //  a single view should work? i.e. std::views::single(data);
        std::vector<std::span<const uint8_t>> v{data};
        auto pipe = 
            v | luma_av::views::parse_packets(parser) | luma_av::views::decode(decoder);
        std::ranges::for_each(pipe, save_frames);
    }
    std::ranges::for_each(luma_av::views::drain(decoder), save_frames);
}
using namespace luma_av_literals;
TEST(DecodeVideoExample, MyExampleStdFileScaling) {
    auto parser = luma_av::Parser::make(AV_CODEC_ID_MPEG1VIDEO).value();
    auto decoder = luma_av::Decoder::make(AV_CODEC_ID_MPEG1VIDEO).value();
    auto sws = luma_av::ScaleSession::make(luma_av::ScaleOpts{640_w, 460_h, AV_PIX_FMT_RGB24}).value();
    auto filename    = kFileName;
    auto outfilename = kOutputFile;

    std::ifstream ifs(filename, std::ios::binary);
    LUMA_AV_ASSERT(ifs.is_open());
    uint8_t inbuf[INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
    /* set end of buffer to 0 (this ensures that no overreading happens for damaged MPEG streams) */
    memset(inbuf + INBUF_SIZE, 0, AV_INPUT_BUFFER_PADDING_SIZE);

    auto save_frames = [&, frame_num = 0](const auto& res) mutable {
        auto&& frame = *res.value();
        std::cout << "saving frame " << frame_num << std::endl;
        auto file_name = std::string{outfilename} + "-" + std::to_string(frame_num);
        pgm_save(frame.data()[0], frame.linesize()[0],
            frame.width(), frame.height(), file_name.c_str());
        ++frame_num;
    };
    while (true) {
        /* read raw data from the input file */
        auto data_size = ifs.readsome(reinterpret_cast<char*>(inbuf), INBUF_SIZE);
        if (data_size==0) {
            break;
        }
        std::span<const uint8_t> data(inbuf, data_size);
        // this is a bug idk why i need to make a vector
        //  a single view should work? i.e. std::views::single(data);
        std::vector<std::span<const uint8_t>> v{data};
        auto pipe = 
            v | luma_av::views::parse_packets(parser) 
                | luma_av::views::decode(decoder) | luma_av::views::scale(sws);
        std::ranges::for_each(pipe, save_frames);
    };
    std::ranges::for_each(luma_av::views::drain(decoder) 
                            | luma_av::views::scale(sws), save_frames);
}

// NOLINTBEGIN
static void decode(AVCodecContext *dec_ctx, AVFrame *frame, AVPacket *pkt,
                   const char *filename)
{
    char buf[1024];
    int ret;
    ret = avcodec_send_packet(dec_ctx, pkt);
    if (ret < 0) {
        fprintf(stderr, "Error sending a packet for decoding\n");
        exit(1);
    }
    while (ret >= 0) {
        ret = avcodec_receive_frame(dec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        else if (ret < 0) {
            fprintf(stderr, "Error during decoding\n");
            exit(1);
        }
        printf("saving frame %3d\n", dec_ctx->frame_number);
        fflush(stdout);
        /* the picture is allocated by the decoder. no need to
           free it */
        snprintf(buf, sizeof(buf), "%s-%d", filename, dec_ctx->frame_number);
        pgm_save(frame->data[0], frame->linesize[0],
                 frame->width, frame->height, buf);
    }
}
TEST(DecodeVideoExample, FullFFmpegExample)
{
    const char *filename, *outfilename;
    const AVCodec *codec;
    AVCodecParserContext *parser;
    AVCodecContext *c= NULL;
    FILE *f;
    AVFrame *frame;
    uint8_t inbuf[INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
    uint8_t *data;
    size_t   data_size;
    int ret;
    AVPacket *pkt;
    // if (argc <= 2) {
    //     fprintf(stderr, "Usage: %s <input file> <output file>\n"
    //             "And check your input file is encoded by mpeg1video please.\n", argv[0]);
    //     exit(0);
    // }
    filename    = kFileName;
    outfilename = kOutputFile;
    pkt = av_packet_alloc();
    if (!pkt)
        exit(1);
    /* set end of buffer to 0 (this ensures that no overreading happens for damaged MPEG streams) */
    memset(inbuf + INBUF_SIZE, 0, AV_INPUT_BUFFER_PADDING_SIZE);
    /* find the MPEG-1 video decoder */
    codec = avcodec_find_decoder(AV_CODEC_ID_MPEG1VIDEO);
    if (!codec) {
        fprintf(stderr, "Codec not found\n");
        exit(1);
    }
    parser = av_parser_init(codec->id);
    if (!parser) {
        fprintf(stderr, "parser not found\n");
        exit(1);
    }
    c = avcodec_alloc_context3(codec);
    if (!c) {
        fprintf(stderr, "Could not allocate video codec context\n");
        exit(1);
    }
    /* For some codecs, such as msmpeg4 and mpeg4, width and height
       MUST be initialized there because this information is not
       available in the bitstream. */
    /* open it */
    if (avcodec_open2(c, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        exit(1);
    }
    f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Could not open %s\n", filename);
        exit(1);
    }
    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate video frame\n");
        exit(1);
    }
    while (!feof(f)) {
        /* read raw data from the input file */
        data_size = fread(inbuf, 1, INBUF_SIZE, f);
        if (!data_size)
            break;
        /* use the parser to split the data into frames */
        data = inbuf;
        while (data_size > 0) {
            ret = av_parser_parse2(parser, c, &pkt->data, &pkt->size,
                                   data, data_size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
            if (ret < 0) {
                fprintf(stderr, "Error while parsing\n");
                exit(1);
            }
            data      += ret;
            data_size -= ret;
            if (pkt->size)
                decode(c, frame, pkt, outfilename);
        }
    }
    /* flush the decoder */
    decode(c, frame, NULL, outfilename);
    fclose(f);
    av_parser_close(parser);
    avcodec_free_context(&c);
    av_frame_free(&frame);
    av_packet_free(&pkt);
}
// NOLINTEND
