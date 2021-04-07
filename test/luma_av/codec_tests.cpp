

#include <array>
#include <future>
#include <queue>
#include <vector>

#include <luma_av/codec.hpp>

#include <gtest/gtest.h>

#include <luma_av/format.hpp>
#include <luma_av/swscale.hpp>
#include <luma_av/util.hpp>

using namespace luma_av;
using namespace luma_av::views;
using namespace luma_av_literals;

static result<Decoder> DefaultDecoder(std::string const& codec_name) {
    LUMA_AV_OUTCOME_TRY(ctx, CodecContext::make(codec_name));
    return Decoder::make(std::move(ctx));
}

static result<Encoder> DefaultEncoder(std::string const& codec_name) {
    LUMA_AV_OUTCOME_TRY(ctx, CodecContext::make(codec_name));
    return Encoder::make(std::move(ctx));
}

TEST(codec, encode_vector) {
    std::vector<AVFrame*> frames(5); 

    auto enc = DefaultEncoder("h264").value();

    std::vector<packet> packets;
    packets.reserve(5);

    Encode(enc, frames, std::back_inserter(packets)).value();
    Drain(enc, std::back_inserter(packets)).value();
}

TEST(codec, encode_array) {
    std::array<AVFrame*, 5> frames; 

    auto enc = DefaultEncoder("h264").value();

    std::vector<packet> packets;
    packets.reserve(5);

    Encode(enc, frames, std::back_inserter(packets)).value();
    Drain(enc, std::back_inserter(packets)).value();
}

TEST(codec, encode_single) {
    AVFrame* frame = nullptr;

    auto enc = DefaultEncoder("h264").value();

    std::vector<packet> packets;
    packets.reserve(5);

    Encode(enc, std::views::single(frame), std::back_inserter(packets)).value();
    Drain(enc, std::back_inserter(packets)).value();
}

TEST(codec, transcode_ranges) {
    std::array<AVPacket*, 5> pkts; 

    auto dec = DefaultDecoder("h264").value();
    auto enc = DefaultEncoder("h264").value();

    std::vector<packet> out_pkts;
    out_pkts.reserve(5);

    for (auto const& pkt : pkts | decode_view(dec) | encode_view(enc)) {
        if (pkt) {
            out_pkts.push_back(packet::make(pkt.value(), packet::shallow_copy).value());
        } else if (pkt.error().value() == AVERROR(EAGAIN)) {
            continue;
        } else {
            throw std::system_error{pkt.error()};
        }
    }
}

TEST(codec, transcode_functions) {
    std::array<AVPacket*, 5> pkts; 

    auto dec = DefaultDecoder("h264").value();
    auto enc = DefaultEncoder("h264").value();

    std::vector<Frame> out_frames;
    out_frames.reserve(5);
    Decode(dec, pkts, std::back_inserter(out_frames)).value();

    std::vector<packet> out_pkts;
    out_pkts.reserve(5);
    Encode(enc, out_frames, std::back_inserter(out_pkts)).value();
}


TEST(codec, read_transcode_ranges) {

    auto reader = Reader::make("input_url"_cv).value();

    auto dec = DefaultDecoder("h264").value();
    auto enc = DefaultEncoder("h264").value();

    std::vector<packet> out_pkts;
    out_pkts.reserve(5);

    for (auto const& pkt : read_input(reader) | decode(dec) | encode(enc)) {
        if (pkt) {
            out_pkts.push_back(packet::make(pkt.value(), packet::shallow_copy).value());
        } else if (pkt.error().value() == AVERROR(EAGAIN)) {
            continue;
        } else if (pkt.error().value() == AVERROR_EOF) {
            break;
        } else {
            throw std::system_error{pkt.error()};
        }
    }
}

TEST(codec, read_transcode_functions) {
    auto reader = Reader::make("input_url"_cv).value();

    auto dec = DefaultDecoder("h264").value();
    auto enc = DefaultEncoder("h264").value();

    std::vector<packet> pkts;
    while (true) {
        if (auto pkt = reader.ReadFrame()) {
            pkts.push_back(std::move(pkt).value());
        } else if (pkt.error().value() == AVERROR_EOF){
            break;
        } else {
            throw std::system_error{pkt.error()};
        }
    }

    std::vector<Frame> out_frames;
    out_frames.reserve(5);
    Decode(dec, pkts, std::back_inserter(out_frames)).value();

    std::vector<packet> out_pkts;
    out_pkts.reserve(5);
    Encode(enc, out_frames, std::back_inserter(out_pkts)).value();
}

namespace {
const auto queue_pop_view = [](auto& q){
    return std::views::iota(0) | std::views::take(std::numeric_limits<int>::max()) 
        | std::views::transform([&](auto){
        auto ele = std::move(q.front());
        q.pop();
        return ele;
    });
};
} // anon

TEST(codec, read_transcode_ranges2) {

    auto reader = Reader::make("input_url"_cv).value();

    auto dec = Decoder::make("h264"_cv).value();
    auto enc = Encoder::make("h264"_cv).value();

    std::vector<packet> out_pkts;
    out_pkts.reserve(5);

    std::queue<result<packet>> packets;
    std::queue<result<Frame>> frames;

    auto read_fut = std::async([&]() -> void {
        for (auto const& pkt : read_input(reader)) {
            if (pkt) {
                packets.emplace(std::move(pkt.value().get()));
            } else if (pkt.error().value() == AVERROR_EOF) {
                packets.emplace(luma_av::errc::end);
                return;
            } else {
                packets.emplace(pkt.error());
                return;
            }
        }
    });

    auto dec_fut = std::async([&]() -> void {
        for (const auto frame : queue_pop_view(packets) | decode(dec)) {
            if (frame) {
                frames.emplace(Frame::make(frame.value().get(), Frame::shallow_copy));
            } else if (frame.error().value() == AVERROR(EAGAIN)) {
                continue;
            } else {
                frames.emplace(frame.error());
                return;
            }
        }
    });

    auto enc_fut = std::async([&]() -> result<std::vector<packet>> {
        auto f = luma_av::detail::finally([&](){
            read_fut.get();
            dec_fut.get();
        });
        std::vector<packet> out_packets;
        for (const auto pkt : queue_pop_view(frames) | encode(enc)) {
            if (pkt) {
                out_packets.push_back(packet::make(pkt.value().get(), packet::shallow_copy).value());
            } else if (pkt.error().value() == AVERROR(EAGAIN)) {
                continue;
            } else if (pkt.error() == luma_av::errc::end) {
                return std::move(out_packets);
            } else {
                return luma_av::outcome::failure(pkt.error());
            }
        }
    });

    auto pkts = enc_fut.get().value();
}


TEST(codec, read_transcode_scale_ranges) {

    auto reader = Reader::make("input_url"_cv).value();

    auto dec = Decoder::make("h264"_cv).value();
    auto enc = Encoder::make("h264"_cv).value();
    auto sws = ScaleSession::make(ScaleOpts{1920_w, 1080_h, AV_PIX_FMT_RGB24}).value();

    std::vector<packet> out_pkts;
    out_pkts.reserve(5);

    for (auto const& pkt : read_input(reader) | decode(dec) | scale(sws) | encode(enc)) {
        if (pkt) {
            out_pkts.push_back(packet::make(pkt.value(), packet::shallow_copy).value());
        } else if (pkt.error().value() == AVERROR(EAGAIN)) {
            continue;
        } else if (pkt.error().value() == AVERROR_EOF) {
            break;
        } else {
            throw std::system_error{pkt.error()};
        }
    }
}

TEST(codec, decode_view_messsaround) {
    auto dec = Decoder::make("h264"_cv).value();

    std::vector<packet> out_pkts;
    auto dv = decode_view(std::views::all(out_pkts), dec);
    auto it = dv.begin();
    ++it;
    it++;
    for (auto frame : dv) {
        std::cout << "uwu" << std::endl;
    }

    auto fn = detail::encdec_view_impl_fn<detail::DecodeInterfaceImpl>{};
    auto dvf = fn(std::views::all(out_pkts), dec);
    auto dvfc = fn(dec);
    auto dv2 = dvfc(std::views::all(out_pkts));

    static_assert(std::ranges::viewable_range<decltype(dv)>);
    static_assert(std::ranges::view<decltype(dv)>);
    static_assert(std::ranges::range<decltype(dv)>);
    static_assert(std::ranges::input_range<decltype(dv)>);
    auto pipe_out = std::views::all(out_pkts) | decode(dec);
    static_assert(std::ranges::viewable_range<decltype(pipe_out)>);
    static_assert(std::ranges::view<decltype(pipe_out)>);
    static_assert(std::ranges::range<decltype(pipe_out)>);
    static_assert(std::ranges::input_range<decltype(pipe_out)>);
    auto take = std::views::all(out_pkts) | decode(dec) | std::views::take(5);

}

TEST(codec, enc_view_messsaround) {
    auto enc = Encoder::make("h264"_cv).value();

    std::vector<Frame> out_frames;
    auto dv = encode_view(std::views::all(out_frames), enc);
    auto it = dv.begin();
    ++it;
    it++;
    for (auto frame : dv) {
        std::cout << "uwu" << std::endl;
    }

    auto fn = detail::encdec_view_impl_fn<detail::EncodeInterfaceImpl>{};
    auto dvf = fn(std::views::all(out_frames), enc);
    auto dvfc = fn(enc);
    auto dv2 = dvfc(std::views::all(out_frames));

    static_assert(std::ranges::viewable_range<decltype(dv)>);
    static_assert(std::ranges::view<decltype(dv)>);
    static_assert(std::ranges::range<decltype(dv)>);
    static_assert(std::ranges::input_range<decltype(dv)>);
    auto pipe_out = std::views::all(out_frames) | encode(enc);
    static_assert(std::ranges::viewable_range<decltype(pipe_out)>);
    static_assert(std::ranges::view<decltype(pipe_out)>);
    static_assert(std::ranges::range<decltype(pipe_out)>);
    static_assert(std::ranges::input_range<decltype(pipe_out)>);
    auto take = std::views::all(out_frames) | encode(enc) | std::views::take(5);

}


TEST(codec, NewRangesUwU) {

    auto reader = Reader::make("input_url"_cv).value();

    auto dec = Decoder::make("h264"_cv).value();
    auto enc = Encoder::make("h264"_cv).value();
    auto sws = ScaleSession::make(ScaleOpts{1920_w, 1080_h, AV_PIX_FMT_RGB24}).value();

    std::vector<packet> out_pkts;
    out_pkts.reserve(5);

    auto pipe = read_input(reader) | decode(dec) | scale(sws) | encode(enc);

    for (const auto& packet : pipe) {
        std::cout << "uwu" << std::endl;
    }

}
