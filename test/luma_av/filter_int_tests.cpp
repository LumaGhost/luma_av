// example based on https://ffmpeg.org/doxygen/trunk/filtering_video_8c-example.html

#define _XOPEN_SOURCE 600 /* for usleep */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
}

#include <fstream>
#include <gtest/gtest.h>
#include <luma_av/codec.hpp>
#include <luma_av/format.hpp>
#include <luma_av/filter.hpp>
#include <luma_av/util.hpp>

using namespace luma_av_literals;

static auto kFileName = "./test_vids/fortnite_mpeg1_cut.mp4";

static void my_display_frame(const AVFrame *frame, AVRational time_base, int64_t& last_pts, std::stringstream& disp) {
    int x, y;
    uint8_t *p0, *p;
    int64_t delay;
    if (frame->pts != AV_NOPTS_VALUE) {
        if (last_pts != AV_NOPTS_VALUE) {
            /* sleep roughly the right amount of time;
            * usleep is in microseconds, just like AV_TIME_BASE. */
            delay = av_rescale_q(frame->pts - last_pts,
                                time_base, AV_TIME_BASE_Q);
            if (delay > 0 && delay < 1000000)
                usleep(delay);
        }
        last_pts = frame->pts;
    }
    /* Trivial ASCII grayscale display. */
    p0 = frame->data[0];
    disp << "\033c";
    for (y = 0; y < frame->height; y++) {
        p = p0;
        for (x = 0; x < frame->width; x++)
            disp << (" .-+#"[*(p++) / 52]);
        disp << '\n';
        p0 += frame->linesize[0];
    }
    disp << std::endl;
};

static std::string LumaAVFilterVideoEx() {
    const auto input_filename = luma_av::cstr_view{kFileName};
    auto fctx = luma_av::format_context::open_input(input_filename).value();
    fctx.FindStreamInfo().value();
    fctx.FindBestStream(AVMEDIA_TYPE_VIDEO).value();
    const auto vid_idx = fctx.stream_index(AVMEDIA_TYPE_VIDEO);
    const auto vid_codec = fctx.codec(AVMEDIA_TYPE_VIDEO);
    auto dec_ctx = luma_av::CodecContext::make(vid_codec, fctx.stream(vid_idx)->codecpar).value();
    const auto time_base = fctx.stream(vid_idx)->time_base;

    auto filter_graph = luma_av::FilterGraph::make().value();
    auto filter_args = luma_av::FilterGraphArgs{}
            .VideoSize(dec_ctx.get()->width, dec_ctx.get()->height)
            .PixFormat(dec_ctx.get()->pix_fmt)
            .AspectRatio(dec_ctx.get()->sample_aspect_ratio)
            .TimeBase(time_base);
    const auto src_filt = luma_av::FindFilter("buffer"_cv).value();
    filter_graph.CreateSrcFilter(src_filt, "in"_cv, filter_args).value();

    const auto sink_filt = luma_av::FindFilter("buffersink"_cv).value();
    filter_graph.CreateSinkFilter(sink_filt, "out"_cv).value();
    
    std::vector<AVPixelFormat> pix_fmts{AV_PIX_FMT_GRAY8, AV_PIX_FMT_NONE};
    filter_graph.SetSinkFilterFormats(pix_fmts).value();
    
    filter_graph.FinalizeConfig("scale=78:24,transpose=cclock"_cv).value();
    const auto sink_timebase = filter_graph.sink_context()->inputs[0]->time_base;


    auto reader = luma_av::Reader::make(std::move(fctx)).value();
    auto decoder = luma_av::Decoder::make(std::move(dec_ctx)).value();
    auto filter = luma_av::FilterSession::make(std::move(filter_graph)).value();

    const auto is_video = [&](auto const& pkt_res){
        if(pkt_res.has_value()) {
            return pkt_res.value().get().get()->stream_index == vid_idx;
        } else {
            return true;
        }
    };
    const auto set_frame_pts = [](const auto& frame_res) -> std::decay_t<decltype(frame_res)> {
        LUMA_AV_OUTCOME_TRY(frame, frame_res);
        // frame.get().get()->pts = frame.get().get()->best_effort_timestamp;
        return std::move(frame);
    };

    auto pipe = luma_av::views::read_input(reader) 
        | std::views::filter(is_video) | luma_av::views::decode_drain(decoder)
        | std::views::transform(set_frame_pts) | luma_av::views::filter_graph(filter);
    
    
    std::stringstream disp;
    const auto display = [&, last_pts = int64_t{0}](const auto& frame_res) mutable {
        auto const& frame = *frame_res.value();
        my_display_frame(frame.get(), sink_timebase, last_pts, disp);
    };
    std::ranges::for_each(pipe, display);

    return std::move(disp).str();
}

static std::string ffmpegFilterVideoEx() {

    const char *filter_descr = "scale=78:24,transpose=cclock";
    /* other way:
    scale=78:24 [scl]; [scl] transpose=cclock // assumes "[in]" and "[out]" to be input output pads respectively
    */
    AVFormatContext *fmt_ctx = nullptr;
    AVCodecContext *dec_ctx = nullptr;
    AVFilterContext *buffersink_ctx = nullptr;
    AVFilterContext *buffersrc_ctx = nullptr;
    AVFilterGraph *filter_graph = nullptr;
    int video_stream_index = -1;
    int64_t last_pts = AV_NOPTS_VALUE;
    auto open_input_file = [&](const char *filename) -> int
        {
            int ret;
            AVCodec *dec;
            if ((ret = avformat_open_input(&fmt_ctx, filename, NULL, NULL)) < 0) {
                av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
                return ret;
            }
            if ((ret = avformat_find_stream_info(fmt_ctx, NULL)) < 0) {
                av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
                return ret;
            }
            /* select the video stream */
            ret = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &dec, 0);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Cannot find a video stream in the input file\n");
                return ret;
            }
            video_stream_index = ret;
            /* create decoding context */
            dec_ctx = avcodec_alloc_context3(dec);
            if (!dec_ctx)
                return AVERROR(ENOMEM);
            avcodec_parameters_to_context(dec_ctx, fmt_ctx->streams[video_stream_index]->codecpar);
            /* init the video decoder */
            if ((ret = avcodec_open2(dec_ctx, dec, NULL)) < 0) {
                av_log(NULL, AV_LOG_ERROR, "Cannot open video decoder\n");
                return ret;
            }
            return 0;
        };
    auto init_filters = [&](const char *filters_descr) -> int
        {
            char args[512];
            int ret = 0;
            const AVFilter *buffersrc  = avfilter_get_by_name("buffer");
            const AVFilter *buffersink = avfilter_get_by_name("buffersink");
            AVFilterInOut *outputs = avfilter_inout_alloc();
            AVFilterInOut *inputs  = avfilter_inout_alloc();
            AVRational time_base = fmt_ctx->streams[video_stream_index]->time_base;
            enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_GRAY8, AV_PIX_FMT_NONE };
            filter_graph = avfilter_graph_alloc();
            if (!outputs || !inputs || !filter_graph) {
                ret = AVERROR(ENOMEM);
                goto end;
            }
            /* buffer video source: the decoded frames from the decoder will be inserted here. */
            snprintf(args, sizeof(args),
                    "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
                    dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt,
                    time_base.num, time_base.den,
                    dec_ctx->sample_aspect_ratio.num, dec_ctx->sample_aspect_ratio.den);
            ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
                                            args, NULL, filter_graph);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
                goto end;
            }
            /* buffer video sink: to terminate the filter chain. */
            ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                                            NULL, NULL, filter_graph);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
                goto end;
            }
            ret = av_opt_set_int_list(buffersink_ctx, "pix_fmts", pix_fmts,
                                    AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Cannot set output pixel format\n");
                goto end;
            }
            /*
            * Set the endpoints for the filter graph. The filter_graph will
            * be linked to the graph described by filters_descr.
            */
            /*
            * The buffer source output must be connected to the input pad of
            * the first filter described by filters_descr; since the first
            * filter input label is not specified, it is set to "in" by
            * default.
            */
            outputs->name       = av_strdup("in");
            outputs->filter_ctx = buffersrc_ctx;
            outputs->pad_idx    = 0;
            outputs->next       = NULL;
            /*
            * The buffer sink input must be connected to the output pad of
            * the last filter described by filters_descr; since the last
            * filter output label is not specified, it is set to "out" by
            * default.
            */
            inputs->name       = av_strdup("out");
            inputs->filter_ctx = buffersink_ctx;
            inputs->pad_idx    = 0;
            inputs->next       = NULL;
            if ((ret = avfilter_graph_parse_ptr(filter_graph, filters_descr,
                                            &inputs, &outputs, NULL)) < 0)
                goto end;
            if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
                goto end;
        end:
            avfilter_inout_free(&inputs);
            avfilter_inout_free(&outputs);
            return ret;
        };
    std::stringstream disp;
    auto display_frame = [&](const AVFrame *frame, AVRational time_base)
        {
            my_display_frame(frame, time_base, last_pts, disp);
        };

    int ret;
    AVPacket packet;
    AVFrame *frame;
    AVFrame *filt_frame;
    frame = av_frame_alloc();
    filt_frame = av_frame_alloc();
    if (!frame || !filt_frame) {
        perror("Could not allocate frame");
        exit(1);
    }
    if ((ret = open_input_file(kFileName)) < 0)
        goto end;
    if ((ret = init_filters(filter_descr)) < 0)
        goto end;
    /* read all packets */
    while (1) {
        if ((ret = av_read_frame(fmt_ctx, &packet)) < 0)
            break;
        if (packet.stream_index == video_stream_index) {
            ret = avcodec_send_packet(dec_ctx, &packet);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Error while sending a packet to the decoder\n");
                break;
            }
            while (ret >= 0) {
                ret = avcodec_receive_frame(dec_ctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if (ret < 0) {
                    av_log(NULL, AV_LOG_ERROR, "Error while receiving a frame from the decoder\n");
                    goto end;
                }
                frame->pts = frame->best_effort_timestamp;
                /* push the decoded frame into the filtergraph */
                if (av_buffersrc_add_frame_flags(buffersrc_ctx, frame, AV_BUFFERSRC_FLAG_KEEP_REF) < 0) {
                    av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
                    break;
                }
                /* pull filtered frames from the filtergraph */
                while (1) {
                    ret = av_buffersink_get_frame(buffersink_ctx, filt_frame);
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                        break;
                    if (ret < 0)
                        goto end;
                    display_frame(filt_frame, buffersink_ctx->inputs[0]->time_base);
                    av_frame_unref(filt_frame);
                }
                av_frame_unref(frame);
            }
        }
        av_packet_unref(&packet);
    }
    end:
        avfilter_graph_free(&filter_graph);
        avcodec_free_context(&dec_ctx);
        avformat_close_input(&fmt_ctx);
        av_frame_free(&frame);
        av_frame_free(&filt_frame);
        if (ret < 0 && ret != AVERROR_EOF) {
            fprintf(stderr, "Error occurred: %s\n", "av_err2str(ret)");
            exit(1);
        }
    return std::move(disp).str();
}


TEST(FilterIntTests, FFmpegComparison) {
    const auto our_results = LumaAVFilterVideoEx();
    const auto ffmpeg_results = ffmpegFilterVideoEx();
    ASSERT_EQ(our_results, ffmpeg_results);
}