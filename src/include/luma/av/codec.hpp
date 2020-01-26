

#ifndef LUMA_AV_CODEC_HPP
#define LUMA_AV_CODEC_HPP

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <memory>

#include <luma/av/result.hpp>
#include <luma/av/frame.hpp>
#include <luma/av/packet.hpp>

/*
https://ffmpeg.org/doxygen/3.2/group__lavc__decoding.html#ga8f5b632a03ce83ac8e025894b1fc307a
https://ffmpeg.org/doxygen/3.2/structAVCodecContext.html
todo and next steps
solution for encoder vs decoder but they share a lot of the same apis
more convenient/expresive/type safe accessors for the codec members
codec context wrapper an decode/encode api. honstly theres so many fucking fields tho
    i think i have an initial direction for the class, need to look more into constructor
    options and invariants
solution for global registration functions like avcodec_register_all
    my first thought is to do it for the user by default,
    just call them during static initialization and you can have a build
    option for the user to disable that if they want
transcoder class for conveinence so the user doesnt have to 
    deal with an encoder and decoder themselves. also i think you can save
    some allocations if you know the user wants to transcode
consistent library wide stance on how/if underlying "unsafe"
    ffmpeg is accessed/exposed

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
        return errc::codec_not_found;
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
// think ffmpeg is expecting null terminated, so no sv here :/
result<const AVCodec*> find_decoder(const std::string& name) {
    const AVCodec* codec = avcodec_find_decoder_by_name(name.c_str());
    return codec_error_handling(codec);
}
}// detail

// https://ffmpeg.org/doxygen/3.4/structAVCodec.html#a16e4be8873bd93ac84c7b7d86455d518
// class wrapper helps because it makes the lifetime/ownership easier to understand
//  i def was confused at first about the ownershup of avcodec. the global
//  ownership is def hard for a new person to understand i think
//  part of the motivation is to make the use easier, so i 
//  think clarity could be a good motivation for making a type
class codec {
    public:
    // is the null state any use here?
    //  think im going with the non null invariant on this one
    //  since its just a non owning ptr theres no after move state
    //  and at that point a default state feels less important
    // i think the null state just isnt useful
    //  it would basically just be two step initialization
    //  the codec functionality is basically just a global map
    //  you either find one or you dont and i think the "not found"
    //  state is better left for the free functions since we can
    //  still express the not found state (and arguably more clearly)
    //  and we also get to make this invariant stronger and
    //  easier to work with
    // codec() noexcept = default;

    // automatically throws? use the free function to handle yourself
    explicit codec(enum AVCodecID id) : codec_{detail::find_decoder(id).value()} {

    }

    explicit codec(const std::string& name) : codec_{detail::find_decoder(name).value()} {

    }

    // contract that codec isnt null
    explicit codec(const AVCodec* codec) : codec_{codec} {

    }

    auto get() const noexcept -> const AVCodec* {
        return codec_;
    }

    // no ownership so shalow/pointer copy is safe
    // no need to move since there is no way to optimize
    //  and no ownership transfer happening
    codec(const codec&) = default;
    codec& operator=(const codec&) = default;

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
result<codec> find_decoder(const std::string& name) {
    // todo think outcome try can be used here or some other tool in the library
    auto codec_res = detail::find_decoder(name);
    if (codec_res) {
        return codec{codec_res.value()};
    } else {
        return codec_res.error();
    }
}

// struct allocate_t {};
// inline constexpr auto allocate = allocate_t{};

// https://ffmpeg.org/doxygen/3.2/structAVCodecParameters.html
class codec_parameters {
    public:

    // nullptr functionality

    // to distinguish from the default constructor?
    //   or should allocating be the defaut?
    //   and then you explicitly construct with nullptr?
    // im against the default being null
    //   i think to get null you should have to explicitly initialize with null
    //   its a bit more user friendly if the default obj is actually useful
    //    and the codec par has a default state already
    //  this does make the after move different from the default
    //   but at least after move still meets the class invariant
    codec_parameters() : codec_par_{alloc_codec_par().value()} {

    }

    codec_parameters(const AVCodecParameters* par) : codec_par_{alloc_codec_par().value()} {
        copy_par(codec_par_.get(), par).value();
    }
    // the user will prob have .reset available if they want
    //   to construct from an owning AVCodecPar
    //   i dont think the par has any ownership transfer semantics
    //   besides the ptr from allocating the par

    // think the codecpar is small enough to allow implicit copy by default
    codec_parameters(const codec_parameters& other) : codec_par_{alloc_codec_par().value()} {
        copy_par(codec_par_.get(), other.codec_par_.get()).value();
    }
    codec_parameters& operator=(const codec_parameters& other) {
        // think this is all you need. it should completely overwrite
        //  the first codec par with the second
        copy_par(codec_par_.get(), other.codec_par_.get()).value();
        return *this;
    }

    // default move semantics with null after move state?
    codec_parameters(codec_parameters&&) = default;
    codec_parameters& operator=(codec_parameters&&) = default;

    AVCodecParameters* get() noexcept {
        return codec_par_.get();
    }

    const AVCodecParameters* get() const noexcept {
        return codec_par_.get();
    }


    private:
    struct codec_par_deleter {
        void operator()(AVCodecParameters* par) const noexcept {
            avcodec_parameters_free(&par);
        }
    };
    // prob all static in the cpp eventually?
    //  something less visible than private functions
    using codec_par_ptr = std::unique_ptr<AVCodecParameters, codec_par_deleter>;
    result<codec_par_ptr> alloc_codec_par() {
        auto par =  avcodec_parameters_alloc();
        if (par) {
            return codec_par_ptr{par};
        } else {
            return errc::alloc_failure;
        }
    }
    result<void> copy_par(AVCodecParameters* out_par, const AVCodecParameters* par) {
        return errc{avcodec_parameters_copy(out_par, par)};
    }
    codec_par_ptr codec_par_ = nullptr;
};

// class for actually decoding, prob have
//  an invariant around keeping the codec open
// https://ffmpeg.org/doxygen/3.2/group__lavc__core.html#gae80afec6f26df6607eaacf39b561c315
//  "It is illegal to then call avcodec_open2() with a different codec."
//  lets make this explicit
// for the move and copy semantics, since the codec context is less
//  complicated than the frame (the frame has extra invaraints because of the buffer and linesize)
//  so maybe we can go for a null state here? that way we can have a cheap move
//  that doesnt allocate
class codec_context {

    // todo friendly solution to plug all of this nullptr functionality
    //   into a type that wants those semantics?
    //   something along the lines of crtp deriving from a detail type?s
    //   just looking for a shortcut to help luma_av devs
    //   and help enforce consistency
    // for consitency with the likes of codec par i dont think
    //  we're going to have a default ctor that sets us to null.
    //  instead the user must explicitly ask for null
    // constexpr codec_context() noexcept = default;

    codec_context(std::nullptr_t) : context_{nullptr} {}

    codec_context& operator=(std::nullptr_t) {
        context_ = nullptr;
        return *this;
    }

    explicit operator bool() const noexcept {
        return bool{context_};
    }

    friend bool operator==(const codec_context& ctx, std::nullptr_t) noexcept {
        return ctx.context_ == nullptr;
    }
    friend bool operator==(std::nullptr_t, const codec_context& ctx) noexcept {
        return ctx.context_ == nullptr;
    }
    friend bool operator!=(const codec_context& ctx, std::nullptr_t) noexcept {
        return ctx.context_ != nullptr;
    }
    friend bool operator!=(std::nullptr_t, const codec_context& ctx) noexcept {
        return ctx.context_ != nullptr;
    }

    // other functionality that interacts with the null state
    void reset(AVCodecContext* ctx = nullptr) noexcept {
        context_.reset(ctx);
    }
    AVCodecContext* release() noexcept {
        return context_.release();
    }

    // deref operator? i dont think any api uses AVCodecContext&
    //  so there would be no need for operator()*
    // operator-> could be considered i think since people definitely
    //  want to access codec context params
    //  feel like our own type safe accessors are prob a better approach
    //  than just letting the user deref since its way less encapsulaton.
    //  we have the get method but i think that will hopefully get branded
    //  as the one "youre on your own" accecss point and its geared towards
    //  advanced users. i feel like its harder to communicate that a deref
    //  operator is only for advanced users. on that note we may want to
    //  consider a name other than "get" if its only for advanced users
    //  i liked the slightly verbose naming on the frame one. pros of naming
    //  "get" is that we have a consistent interface that would allow us to
    //   use crtp to plug in the null ptr semantics on each type
    //   thats going to behave that way
    //   a big factor for me is how complex the invariant is.
    //   if there isnt much to mess up than a friendly "get"
    //   and maybe even a deref is probably fine
    //  i think the ideall is that the user doesnt really have
    //  to deal with the AVCodecContext*, but if they do i think get is enough
    //  i also dont like having operator-> without operator*

    // do we want to support initializing from a null codec?
    //  we already have our nullptr constructor used up
    //  so to avoid being ambiguous i think this can just take a codec
    // worst case the user can call avcodec alloc context on their own 
    //  and use the context constructor
    // explicit codec_context(const AVCodec*);
    explicit codec_context(const codec& codec) 
        : codec_{codec.get()}, context_{alloc_context(codec).value()} {

    }

    // users that want to set tons of params can use the codec
    //  context api straight up
    // will this constructor mess with the invariant?
    //  i think it will. we need to keep the codec and context together
    // explicit codec_context(const AVCodecContext*);

    // need to be consistent about this throught the lib
    //  do we take AVCodecParameters* or just luma::av::codec_par
    //  as long as luma codec par offers easy conversions
    //  i think taking the luma version is better since
    //  it expresses that null isnt valid
    explicit codec_context(const codec& codec, const codec_parameters& par) : codec_context{codec} {
        codec_ctx_from_par(context_.get(), par.get()).value();
    }

    // looks like the context itself cannot be deeply copied
    //  just the stream settings via codec par
    // https://ffmpeg.org/doxygen/3.2/group__lavc__core.html#gae381631ba4fb14f4124575d9ceacb87e
    codec_context(const codec_context&) = delete;
    codec_context& operator=(const codec_context&) = delete;

    // think we can always allocate the par for the user
    //  the ffmpeg call unconditionally resets the par
    //  so i dont think thers a benefit of the user
    //  allocating the par themselves
    // name thats a bit more explicit about the fact that
    //  this is an allocation and a copy of the current parameters?
    result<codec_parameters> codec_par() const {
        auto par = codec_parameters{};
        LUMA_AV_OUTCOME_TRY(codec_par_from_ctx(par.get(), context_.get()));
        return par;
    }

    // what does ffmpeg mean by this function isnt thread safe?
    // two threads cant call this function at the same time
    //  even if its on completely different objects?
    result<void> open(AVDictionary**  options) {
        auto ec = avcodec_open2(get(), codec_, options);
        return errc{ec};
    }

    bool is_open() const noexcept {
        // this is ffmpegs implementation
        //  idk why this function isnt const
        // https://ffmpeg.org/doxygen/3.2/group__lavc__misc.html#ga906dda732e79eac12067c6d7ea19b630
        return !!context_->internal;
    }

    /*
    warning:
    "The input buffer, avpkt->data must be AV_INPUT_BUFFER_PADDING_SIZE 
        larger than the actual read bytes because some optimized bitstream readers
        read 32 or 64 bits at once and could read over the end.
    "Do not mix this API with the legacy API (like avcodec_decode_video2())
        on the same AVCodecContext. It will return unexpected results
         now or in future libavcodec versions."
    feel like our api should ideally avoid all of the warnings and pitfalls of ffmpeg
        unless the user goes out of their way to access those
    */
    // think we can take the raw ptr here since its const
    //  no safety penality but more felxibility?
    //  well it could be null, in that case we say its on the user?
    result<void> send_packet(const AVPacket* p) {
        auto ec = avcodec_send_packet(get(), p);
        return errc{ec};
    }

    // convenience overload for our own packet
    result<void> send_packet(const packet& p) {
        return this->send_packet(p.get());
    }

    /*
    " The encoder may create a reference to the frame data 
        (or copy it if the frame is not reference-counted)"
      more hints at what ref coutning means. i think we will want to make sure
        that the luma frame is ref counted so we dont force the user
        into a useless expensive copy here, or maybe the opposite.
        i think the encoder may need to hold onto that buffer for a bit
        because not every frame produces a packet, so yea we would
        want to make sure luma frame isnt ref counted so that it maintains
        its unique ownership even while its being encoded
    */
    // think the frame overloadsd can follow the same spirit as send_packet
    // still need to decide if we want the encoder and decoder to be different types
    // maybe the codec context memory management is one detail class
    //  so that we dont repeat as much similar code when making decoder and encoder
    //  as a separate class. I like the type safe distinction between encoder and decoder
    //  since they have different capabilties and cant really be interchanged
    result<void> send_frame(const AVFrame* f) {
        auto ec = avcodec_send_frame(context_.get(), f);
        return errc{ec};
    }



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
        auto ec = avcodec_receive_frame(context_.get(), frame.avframe_ptr());
        return errc{ec};
    }

    // i guess the recieve functions should also have AVFrame/AVpacket overloads?
    //  i guess but a non const ptr is less expressive imo but if the user
    //  wants it i dont see it being harmful if we offer a safer alternative
    //  it def has to be "Advanced usage" unlike the send functions and decode/encode
    //  the user can actually mess this up if they modify the frame before making it writable
    //  so theres def more risk
    result<void> recieve_packet(packet& p) {
        auto ec = avcodec_receive_packet(context_.get(), p.get());
        return errc{ec};
    }
    result<void> recieve_packet(AVPacket* p) {
        auto ec = avcodec_receive_packet(context_.get(), p);
        return errc{ec};
    }
    // both of these functions are risky i guess since in either case
    //  the packet and the encoder share ownership of the pkt buffer

    // same packet overloads from send_packet would
    //  be supported here
    result<frame> decode(const AVPacket* p) {
        LUMA_AV_OUTCOME_TRY(this->send_packet(p));
        LUMA_AV_OUTCOME_TRY(this->recieve_frame(decoder_frame_));
        // the decoded frame f reference the frame buffers inside
        //  of the decoder, we need to copy those out so f has ownership
        // LUMA_AV_OUTCOME_TRY(luma::av::make_writable(f));
        auto f = frame{};
        LUMA_AV_OUTCOME_TRY(f.copy(decoder_frame_));
        return f;
    }

    /*
    https://ffmpeg.org/doxygen/3.2/group__lavc__decoding.html#ga5b8eff59cf259747cf0b31563e38ded6
    This will be set to a reference-counted packet allocated by the encoder. 
        Note that the function will always call av_frame_unref(frame) before doing anything else.
    i just realised "will be set" is a bit vauge. it could be taken to mean the reseat the ptr
        but they dont take a ** so they couldnt, that would also leak memory. so i take it
        to mean they unref and reseat just the buffers of the packet 
    */
    // same frame overloads from send_frame would
    //  be supported here
    result<packet> encode(const AVFrame* f) {
        LUMA_AV_OUTCOME_TRY(this->send_frame(f));
        LUMA_AV_OUTCOME_TRY(this->recieve_packet(encoder_packet_));
        // the encoded packet p references the buffers inside
        //  of the encoder, we need to copy those out so p has ownership
        return packet{encoder_packet_.get()};
    }
    // this function (like decode) assumes the user wants a uniquely
    //  owned frame/packet. otherwise this is inefficient comapred
    //  to doing manual send/recieve calls due to us creating
    //  a new packet or frame each time here isntead of reusing
    // in both cases (encode/decode) i dont think there is a more
    //  efficient way to eccode/decode and produce a uniquesly owned packet/frame?
    //  actually with the packet since we do a deep copy every time we can reuse
    //  the pkt each time even if the user wants a uniquely owned packet.
    //  the frame one i dont think we can get more efficient, sizeof(AVFrame)
    //  isnt part of the public abi (afaik) so we cant avoid an extra call
    //  to avframe alloc without using make_writable. we could keep a frame
    //  as a member and reuse it but that would still result in decode
    //  costing the same (one call to frame alloc, copy props, and alloc + copy of the buffers)
    // note: keeping the frame/packet as a member does help us avoid wasting allocations
    //  in the case where the frame/packet isnt ready from the decoder/encoder

    // this isnt global (unlike the codec) 
    //  so its okay for the user to modify it on a non const object
    AVCodecContext* get() noexcept {
        return context_.get();
    }

    const AVCodecContext* get() const noexcept {
        return context_.get();
    }


    private:
    struct codec_context_deleter {
        void operator()(AVCodecContext* ctx) const noexcept {
            avcodec_free_context(&ctx);
        }
    };
    using context_ptr = std::unique_ptr<AVCodecContext, codec_context_deleter>;
    // again i think the result error handling is more expressive than null error handling
    //  and signficiantly more friendly to excepton users
    // could make all of this public including the deleter possibly
    // im not sure at the moment which is better
    //  minimize scope/visibility by default, but dont hide useful functionality
    result<context_ptr> alloc_context(const codec& codec) {
        auto ctx = avcodec_alloc_context3(codec.get());
        if (ctx) {
            return context_ptr{ctx};
        } else {
            // i think this is always an alloc failure?
            return errc::alloc_failure;
        }
    }
    result<void> codec_par_from_ctx(AVCodecParameters* par, const AVCodecContext* ctx) const {
        return errc{avcodec_parameters_from_context(par, ctx)};
    }
    result<void> codec_ctx_from_par(AVCodecContext* ctx, const AVCodecParameters* par) const {
        return errc{avcodec_parameters_to_context(ctx, par)};
    }
    const AVCodec* codec_ = nullptr;
    context_ptr context_ = nullptr; 
    // all of these would need a null state though
    // also the members are pretty awkward for 
    //  users that what to use the send/recieve functions themselves
    frame decoder_frame_;
    packet encoder_packet_;
};




} // av
} // luma

#endif // LUMA_AV_CODEC_HPP