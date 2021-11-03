/*
 * Copyright (c) 2001 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
/**
 * @file
 * video decoding with libavcodec API example
 *
 * @example decode_video.c
 */
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

static constexpr auto kFileName = "./test_vids/fortnite_mpeg1_cut.mp4";
static constexpr auto kFrameCompCount = int{10};

#define INBUF_SIZE 4096

static std::vector<std::vector<uint8_t>> LumaAvDecodeVideo() {
    auto parser = luma_av::Parser::make(AV_CODEC_ID_MPEG1VIDEO).value();
    auto decoder = luma_av::Decoder::make(AV_CODEC_ID_MPEG1VIDEO).value();
    auto filename    = kFileName;
    FILE* f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Could not open %s\n", filename);
        exit(1);
    }
    auto fc = luma_av::detail::finally([&](){
        fclose(f);
    });
    uint8_t inbuf[INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
    /* set end of buffer to 0 (this ensures that no overreading happens for damaged MPEG streams) */
    memset(inbuf + INBUF_SIZE, 0, AV_INPUT_BUFFER_PADDING_SIZE);

    std::vector<std::vector<uint8_t>> frame_data;
    auto save_frames = [&](const auto& res) mutable {
        if (!res) {
            std::cout << "error!!! " << res.error().message() << std::endl;
        }
        auto&& frame = *res.value();
        const auto buff_size = frame.width() * frame.height();
        const auto gray_buff = frame.data()[0];
        frame_data.emplace_back(gray_buff, gray_buff+buff_size);
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
        if (frame_data.size() >= kFrameCompCount) {
            break;
        }
    }
    return frame_data;
}


static std::vector<std::vector<uint8_t>> FFmpegDecodeVideoExample()
{
    const auto decode = [](AVCodecContext *dec_ctx, AVFrame *frame, AVPacket *pkt,
                            std::vector<std::vector<uint8_t>>& frame_data) -> void
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
            const auto gray_buff = frame->data[0];
            const auto buff_size = frame->width * frame->height;
            frame_data.emplace_back(gray_buff, gray_buff+buff_size);
            if (frame_data.size() >= kFrameCompCount) {
                    break;
            }
        }
    };
    const char *filename;
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
    filename    = kFileName;
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
    std::vector<std::vector<uint8_t>> frame_data;
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
            if (pkt->size) {
                decode(c, frame, pkt, frame_data);
                if (frame_data.size() >= kFrameCompCount) {
                    break;
                }
            }
        }
        if (frame_data.size() >= kFrameCompCount) {
                    break;
        }
    }
    fclose(f);
    av_parser_close(parser);
    avcodec_free_context(&c);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    return frame_data;
}


TEST(DecodeVideoIntegration, FfmpegComparison) {
    const auto luma_frames = LumaAvDecodeVideo();
    const auto ffmpeg_frames = FFmpegDecodeVideoExample();
    ASSERT_EQ(luma_frames.size(), ffmpeg_frames.size());
    ASSERT_EQ(luma_frames, ffmpeg_frames);
}


TEST(DecodeVideoIntegration, ParserConstructDestruct) {
    auto parser = luma_av::Parser::make(AV_CODEC_ID_MPEG1VIDEO).value();
}


/**
note: packet free'd first
*/
TEST(DecodeVideoIntegration, ParserParseOne) {
    auto parser = luma_av::Parser::make(AV_CODEC_ID_MPEG1VIDEO).value();
    auto filename    = kFileName;
    FILE* f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Could not open %s\n", filename);
        exit(1);
    }
    auto fc = luma_av::detail::finally([&](){
        fclose(f);
    });
    uint8_t inbuf[INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
    /* set end of buffer to 0 (this ensures that no overreading happens for damaged MPEG streams) */
    memset(inbuf + INBUF_SIZE, 0, AV_INPUT_BUFFER_PADDING_SIZE);
    
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
            v | luma_av::views::parse_packets(parser);
        std::vector<luma_av::Packet> packets;
        std::ranges::transform(pipe, std::back_inserter(packets), [](auto&& pkt_ref){
            return luma_av::Packet::make(*pkt_ref.value()).value();
        });
        if (!packets.empty()) {
            break;
        }
    }
}

/**
note: parser free'd first
*/
TEST(DecodeVideoIntegration, ParserFullParse) {
    auto parser = luma_av::Parser::make(AV_CODEC_ID_MPEG1VIDEO).value();
    auto filename    = kFileName;
    FILE* f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Could not open %s\n", filename);
        exit(1);
    }
    auto fc = luma_av::detail::finally([&](){
        fclose(f);
    });
    uint8_t inbuf[INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
    /* set end of buffer to 0 (this ensures that no overreading happens for damaged MPEG streams) */
    memset(inbuf + INBUF_SIZE, 0, AV_INPUT_BUFFER_PADDING_SIZE);
    
    std::vector<luma_av::Packet> parsed;
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
            v | luma_av::views::parse_packets(parser);
        std::ranges::transform(pipe, std::back_inserter(parsed), [](auto&& pkt_ref){
                return luma_av::Packet::make(*pkt_ref.value()).value();
        });
    }
    ASSERT_FALSE(parsed.empty());
}