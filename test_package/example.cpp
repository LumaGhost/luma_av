#include <iostream>

#include <luma_av/codec.hpp>

using namespace luma_av;

int main() {
    std::cout << "making codec context" << std::endl;
    const auto h264_codec = std::string{"h264"};
    auto codec_context = CodecContext::make(h264_codec).value();
    std::cout << "making decoder" << std::endl;
    const auto decoder = Decoder::make(std::move(codec_context));
    std::cout << "end of test_package example" << std::endl;
    return 0;
}
