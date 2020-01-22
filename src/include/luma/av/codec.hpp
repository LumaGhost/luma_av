

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <memory>

#include <luma/av/frame.hpp>

/*
https://ffmpeg.org/doxygen/3.2/group__lavc__decoding.html#ga8f5b632a03ce83ac8e025894b1fc307a
https://ffmpeg.org/doxygen/3.2/structAVCodecContext.html
todo and next steps
solution for encoder vs decoder but they share a lot of the same apis
more convenient/expresive/type safe accessors for the codec members
codec context wrapper an decode/encode api. honstly theres so many fucking fields tho
    i think i have an initial direction for the class, need to look more into constructor
    options and invariants

def need to put together a statement on error handling
    looking like a combination of result, exceptions (from constructors)
    contracts for certain hard/logical errors or in constructors,
    along with user defined error policies
    the error handling space is definitely going through a change
    (proposals to completely change how exceptions work, contracts)
    so im down to try something new
    i feel like codec and frame errors are different. i think codec not found
    is an error that people will reasonably recover from (e.g. try a different codec)
    so i feel like exceptions and error codes together give the user what they want
    either way. i was thinking with frame allocation falures are different, more 
    controvercial, but i think you end up with the same ammount of options for erro
    handling without having to ask the user to make a policy.
    i def agree for throwing functions in a move you need a policy so that you
    can terminate by default without making people too angry. but honestly
    i think i can do better than a move that allocates... update nah we cant
    av frame is an allocating move or no move... at least everything i cant think of so far
    conracts imo are the optimal choice for allowing users to pass e.g. an AVCodec* or AVFrame*
    to one of the types here. we could signal with an exception, but i like contracts
    for hard logical errors since users can turn them off if theyre sure they wont get
    violated and they dont want the overhead
codecs are lighterweight i assume or at least not as performance sensitve as frame
    so i think policies are overkill
compatibility with the c api strucures is prob important for people
    that have existing custom code in ffmpeg, ideally we can
    make adopting the library possible in that situation without
    forcing someone in that position to completely redo stuff in this api
*/

namespace luma {
namespace av {

namespace detail {

// cray to wrap null error handling with result error handling?
//  maybe but i think its stilll slightly more expressive and its
//  way more friendly to exception users typing .value to get the exception
//  instead of basically writing this function
result<const AVCodec*> codec_error_handling(const AVCodec* codec) {
    if (codec) {
        return codec;
    } else {
        // think the error is always codec not found
        auto ec = int{};
        return return ec;
    }
}

// going to "force" the user not to modify the global?
//  they can const cast it if they really want to
//  honestly ive had bugs because of not realising the codecs and formats are global
//  and i think its something other people might not expect as well
//  so i think its helpful to discourage modification here
// also by c++ convention here this ptr is non owning
//  i think its prob just okay to use avcodec straight up?
//  maybe a lightweight wrapper that just stores the pointer?
// overload set since its c++ and we can do that
result<const AVCodec*> find_decoder(enum AVCodecID id) {
    const AVCodec* codec = avcodec_find_decoder(id);
    return codec_error_handling(codec);
}
result<const AVCodec*> find_decoder(const std::string_view name) {
    const AVCodec* codec = avcodec_find_decoder_by_name(name);
    return codec_error_handling(codec);
}
}// detail

// class wrapper helps because it makes the lifetime/ownership easier to understand
//  i def was confused at first about the ownershup of avcodec. the global
//  ownership is def hard for a new person to understand i think
//  part of the motivation is to make the use easier, so i 
//  think clarity could be a good motivation for making a type
class codec {
    public:
    // is the null state any use here?
    //  think im going with the non null invariant on this one
    //  since its just a non owning ptr
    // codec() noexcept = default;

    // automatically throws? use the free function to handle yourself
    explicit codec(enum AVCodecID id) : codec_{detail::find_decoder(id).value()} {

    }

    explicit codec(const std::string_view name) : codec_{detail::find_decoder(name).value()} {

    }

    // contract that codec isnt null
    explicit codec(const AVCodec* codec) : codec_{codec} {

    }

    auto get() const noexcept -> const AVCodec* {
        return codec_;
    }

    private:
        const AVCodec* codec_;
};

// if people want to use the error code api
result<codec> find_decoder(enum AVCodecID id) {
    // todo think outcome try can be used here or some other tool in the library
    auto codec_res = detail::find_decoder(id);
    if (codec_res) {
        return codec{codec_res.value()};
    } else {
        return codec_res.error();
    }
}
result<codec> find_decoder(const std::string_view name) {
    // todo think outcome try can be used here or some other tool in the library
    auto codec_res = detail::find_decoder(name);
    if (codec_res) {
        return codec{codec_res.value()};
    } else {
        return codec_res.error();
    }
}

// class for actually decoding, prob have
//  an invariant around keeping the codec open
class codec_context {

    // users that want to set tons of params can use the codec
    //  context api straight up
    explicit codec_context(const AVCodecContext*);

        /*
    This will be set to a reference-counted video or audio frame (depending on the decoder type)
        allocated by the decoder. Note that the function will always call 
        av_frame_unref(frame) before doing anything else.

    so i think the best practices buffer reuse is like this
    auto f = frame{}; // av_frame_alloc (just the frame no buffers)
    recieve_frame(f);
    // up to the user at this point
    // they can even just inspect f or write it to the disk or something.
    // whatever really. anything besides presisting f in memory and
    // they can just move back up to reusing f 
    // to persist the frame, one call to av_frame_alloc for a new frame
    //  and then copy props to the new frame and
    //  move the buffer ref from f to the new frame
    auto out_frame = frame{std::move(f)};
    // reuse f on the next call to recieve_frame
    // i think we can offer two apis, one with a void return
    //  for users that just want to read/save the frame and not
    //   make a new avframe
    //  and one with a frame for convenience for users that want the frames
    //  the user byob on the recieve frame call regardless
    */
    result<void> recieve_frame(frame& frame) {
        auto ec = avcodec_receive_frame(context_, frame.avframe_ptr());
        return ec;
    }

    private:
    AVCodecContext* context_; 
};




} // av
} // luma