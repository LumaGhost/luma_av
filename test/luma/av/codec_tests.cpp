

#include <array>
#include <vector>

#include <luma/av/codec.hpp>

#include <gtest/gtest.h>

using namespace luma::av;

static result<Encoder> DefaultEncoder(std::string const& codec_name) {
    LUMA_AV_OUTCOME_TRY(codec, luma::av::find_decoder(codec_name));
    auto ctx = codec_context{codec};
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