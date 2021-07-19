# luma_av

### Intro

this library is meant to provide the functionality of ffmpeg through a modern c++20 interface. the goal is to provide the features of the ffmpeg api and compatibility with existing code using the ffmpeg api, while also providing memory safe abstractions and other higher level features not available in c. in the end we hope to make coding with ffmpeg a lot easier and safer (: at the cost of requiring newer c++ features. in the examples directory you can see examples of our api compared side by side with ffmpeg (:

in its current state i see this library as a proof of concept. there are still a lot of quality of life improvements to make before other people can comfortable work in the project (in my opinion). and im still thinking through some of the library design and semantics. right now i am mainly looking for feedback as i work towards an actual 0.0 release of the project (:

note: i would ideally like all of the abstractions in this library to be "zero [performance] cost". though right now i am prioritizing features, expressiveness, usability, simplicity etc. over pure performance and leaving optimization and benchmarking as an exercise for later.

### Overview

this library attempts to mirror the structue and layout of ffmpeg. though it is not identacle, it should hopefully at least be familiar. while trying to at least provide the functionality available in ffmpeg, in some cases i also added additional abstractions as shortcuts for the sake of simplicity. for example in codec.hpp there is `CodecContext` which aims to mirror `AVCodecContext` from the ffmpeg library. but then there are also `Decoder` and `Encoder` objects that build on top of the codec context to further express its different capabiltiies and offer a simplified interface for those that want to get up and running quicker.

raii is used throughout along with other memory safety and modern c++ best practices (havily based on the CppCoreGuidelines). mainly being explicit abt ownership and object invariants and trying to use modern idiomatic language features where aplicable. we use assertions liberally (which can be disabled if desired) to enforce corectness. ideally we would like the use to be able to call our api and have peace of mind that there wont be any undefined behavior or memory safety issues and at worst an assertion or error will fire ( hard in c++ but we're trying (: ).

additionally, we use c++20 ranges to express a lot of the processing operations available in ffmpeg. a main motivation of using ranges is that a lot of the nuance of processing with ffmpeg (e.g. having to feed another frame/packet when your encoder/decoder returns EAGAIN) can be hidden behind a much more easy to use abstraction. thanks to ranges, building a media processing pipeline can be as easy as choosing which steps you want. and of course our ranges are compatible with standard ranges and other c++20 compatible ranges (:

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

this library does not use exceptions (: instead we use the Outcome library. 

the short answer for why is that i wanted easier integration with existing ffmpeg code that may not be exception safe. i also personally just dont like exceptions (and i plan to explain why in more detail on a different page... at some point).

additionally, since we pass errors through return values objects with failable initialization are initialized using static methods rather than constructors.

### Working with existing ffmpeg code

i tried to include support for users with existing c or c++ code that uses ffmpeg. in addtion to not using exceptions, there are a lot of different options for (hopefully) easy and flexible conversion between our types and ffmpeg types. hopefully the compatibility is to the point where one could integrate this library as just a small part of a larger ffmpeg c project if need be.


### Dependencies/Build

right now the dependencies and build are just the minimum to get working on our machines. there is still a lot of accessiblity improvements and generel cleanup to be done (:

that said, the following conan commans `should` get you up and running (:

- ```conan install . -if build --build missing```
- ```conan build . -bf build```
- see build/test and build/examples for the output binaries (:


### Contributing

not quite ready for other developers but thats something i want in the future (: in the meantime id appreciate any discussion or feedback. mainly looking for feedback in terms of if you would use this library and why or why not. but i'll hear any type feedback. feedback doesnt have to be constructive just in good faith (:


### Closing

hopefully this readme at least answered some questions about what this library aims to accomplish, and how it plans to accomplish that. and hopefully enough abt how the libary works to get a feel for whether or not you are interested in using it (: feedback on the readme is especially appreciated!