# luma_av

### Intro

This library is meant to provide the functionality of ffmpeg through a modern c++20 interface (:

The goal is to provide the features and semantics of the ffmpeg api and compatibility with existing code using the ffmpeg api. while also providing memory safe abstractions and other higher level features not available in c. in the end we hope to make coding with ffmpeg a lot easier and safer (: at the cost of requiring newer c++ features.

The examples directory contains side by side examples of our api and the C ffmpeg api (:

#### Library Status

In its current state the library should be seen as a proof of concept. There are still a lot of quality of life improvements to make before other people can comfortably work on the project , and the library design and semantics are still subject to revision and redesign. 

### Overview

This library attempts to mirror the structue and layout of ffmpeg. Although not identical, it is intended to be familiar. While striving to provide the functionality available in ffmpeg, in some cases additional abstractions and shortcuts have been added for the sake of simplicity.
 
For example in codec.hpp there is `CodecContext` which aims to mirror `AVCodecContext` from the ffmpeg library. but then there are also `Decoder` and `Encoder` objects that build on top of the codec context to further express its different capabilities and offer a simplified interface for those that want to get up and running quicker.

RAII is used throughout along with other memory safety and modern c++ best practices (partially based on the [CppCoreGuidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines)). There has been a focus on ownership and lifetime semantics as well as object invariants. Additionally an attempt has been made to use modern idiomatic language features where aplicable. 

Assertions are used liberally (which can be disabled if desired) to enforce corectness. Ideally we would like the user to be able to call our api and have peace of mind that there wont be any undefined behavior or memory safety issues and at worst an assertion or error will fire ( hard in c++ but we're trying (: ). Ideally all of the abstractions in this library to be "zero [performance] cost". Currently we are prioritizing features, expressiveness, usability, simplicity etc. over pure performance and leaving optimization and benchmarking for future work.

Additionally, we use c++20 ranges to express a lot of the processing operations available in ffmpeg. a main motivation of using ranges is that a lot of the nuance of processing with ffmpeg (e.g. having to feed another frame/packet when your encoder/decoder returns EAGAIN) can be hidden behind easier abstractions. With ranges, building a media processing pipeline can be as easy as choosing which steps you want. and of course our ranges are compatible with standard ranges and other c++20 compatible ranges (:

### Examples

this example shows a simple transcoding processing pipeline where we read a video file, decode, scale, and encode (:
saving the output video is still a work in progress so im just leaving the packets in a vector (:
```
auto reader = luma_av::Reader::make("input_url"_cv).value();

auto dec = luma_av::Decoder::make("h264"_cv).value();
auto enc = luma_av::Encoder::make("h264"_cv).value();
auto sws = luma_av::ScaleSession::make(luma_av::ScaleOpts{1920_w, 1080_h, AV_PIX_FMT_RGB24}).value();

auto pipe = luma_av::views::read_input(reader) 
            | luma_av::views::decode(dec) | luma_av::views::scale(sws) 
            | luma_av::views::encode(enc) 
            | std::views::transform([](auto const& res){
                return luma_av::Packet::make(res.value()).value();
            });

std::vector<luma_av::Packet> out_pkts;
for (auto&& pkt : pipe) {
    out_pkts.push_back(std::move(pkt));
}
```

note: examples assume `using namespace luma_av_literals;`

###  Error Handling

This library does not use exceptions (: instead we use the [Outcome library](https://ned14.github.io/outcome/). Mainly for easier integration with existing ffmpeg code that may not be exception safe. As well as personal preference against exceptions.

Additionally, since we pass errors through return values objects with failable initialization are initialized using static methods rather than constructors.

### Working with existing ffmpeg code

There have been efforts made to include support for users with existing c or c++ code that uses ffmpeg. In addition to not using exceptions, there are several options for assisting conversion between our library types and ffmpeg types. Hopefully the compatibility is to the point where one could integrate this library as just a small part of a larger ffmpeg c project if need be.


### Dependencies/Build

Right now the dependencies and build are just the minimum to get working on our machines. There is still a lot of accessiblity improvements and generel cleanup to be done (:

that said, the following conan commans `should` get you up and running (:

- ```conan install . -if build --build missing```
- ```conan build . -bf build```
- see build/test and build/examples for the output binaries (:


### Contributing

not quite ready for other developers but thats something i want in the future (: in the meantime id appreciate any discussion or feedback. mainly looking for feedback in terms of if you would use this library and why or why not. but i'll hear any type feedback. feedback doesnt have to be constructive just in good faith (:


### Closing

hopefully this readme at least answered some questions about what this library aims to accomplish, and how it plans to accomplish that. and hopefully enough abt how the libary works to get a feel for whether or not you are interested in using it (: feedback on the readme is especially appreciated!