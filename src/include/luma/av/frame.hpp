

/**
* purpose:
*  allow people to access the sweet power of ffmpeg without dealing with
*  the immense mental overhead and boilerplate that comes with manual
*  memory management and manual error handling
*  ideally flexibility and customization isnt sacrificed comapred to ffmpeg
*    and the api is easy to integrate with existing ffmpeg code
*    by supporting conversions/access to the underlying c structures
* next steps/todo:
*  trello and project board
*  design for buffer invariant, considering using a std allocator interface
*    do descrive the users choice in how the buffers should be allocated
*  more research/experiments into avframe memory api and the users
*   responsibilites for memory management. unsafe/safe secnarios etc.
*   better undestranding of avframe ref/unref. is unref the only
*   function you need to clear to properly clear the buffers?
*   ffmpeg documentaiton mentions "refernce counted" frames
*   as in some frames arent ref counted so i just want to make sure 
*  error handling desing: error codes? result? exceptions?
*  inital first draft and tests for frame design 
*  deside on align customization point
*/

/**
thoughts and ideas

how to deal with apis like av_recieve_frame without exposing AVFrame*?
  exposing avframe forces the user to know exactly how the ffmpeg api
  theyre passing the frame to affects the invariant
  not exposing avframe forces us to expose pretty much all ffmpeg functionality
  update: i think we allow for avframe* access but discourage it
    by providing enough functionality to where most users dont need it
    and i think it should be considered advanced functionality
    so its okay to expect the user to know how the
    underlying api modifies the frame and deal with any consequences
    from invariant violation
maybe a copyable policy. just a function object that returns true
    if copies are allowed. just write the copy constructor
    then add static_assert(copy_policy{}());
    or static_assert(copy_policy::value)
general utility for wrapping void pointer customization points into something
    more type safe and experessive?
holly shit av buffer padding size? https://ffmpeg.org/doxygen/3.2/group__lavc__decoding.html#ga8f5b632a03ce83ac8e025894b1fc307a
    the part about how mjpeg can segfault in a specific case
can i get a better move constructor by just swapping the frame ptrs?
    thats a reasonable cost for a move. maybe still unref the old buffer
    now its noexcept and maybe you dont need policies. just take the approach
    in codec and have detail result free functions that wrapp ffmpeg
    then use exceptions in the constructor and provide free functions
    that return a result of the regular type so the user can 
    opt into error handling
    actually i think i need a call to avframe alloc no matter what
    i just really really dont like how expensive the move is right now
    maybe we should let the user opt into a fucked up move state that breaks the invariant?
    so that they can get a cheap move if they want to? this move ctor is hard as fuck to design
any easier ways for the user to check if the frame has buffers?
    currently i think just if(frame.data(0)) will work 
    but maybe something more expressive? esp since we're making
    the whole buffer invariant about not just the data but linesize
    and the buffer sentitive params and ownership too
    we could maybe do bool conversion but idk if thats expressive
*/

// https://ffmpeg.org/doxygen/3.3/group__lavu__frame.html
// https://ffmpeg.org/doxygen/3.2/pixdesc_8h.html
// https://github.com/FFmpeg/FFmpeg/blob/master/libavutil/imgutils.c
// https://ffmpeg.org/doxygen/2.4/group__lavu__picture.html#ga8eddd074d5eb6a235591013675ac1055

#ifndef LUMA_AV_FRAME_HPP
#define LUMA_AV_FRAME_HPP

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
}

#include <memory>

#include <luma/av/result.hpp>

namespace luma {
namespace av {

// user gets to decide what happens when the frame fails to allocate
//  default is just terminate
//  alternatively deal with possibly null frame state
//  or throw if you really want to?
// right now its the users responsibility to check if the frame is null
//  maybe the policy is only called if the frame is null
//  i think its better if the user can do the check since it gives them more room to optimize?
struct termiante_on_frame_alloc_failure {
    void operator()(const AVFrame* frame) noexcept {
        if (!frame) {
            std::terminate();
        }
    }
};

struct throw_on_frame_alloc_failure {
    void operator()(const AVFrame* frame) {
        if (!frame) {
            throw std::bad_alloc{};
        }
    }
};

struct ignore_frame_alloc_failure {
    void operator()(const AVFrame* frame) noexcept {
        
    }
};

// av copy props calls are necessary for the classes invariant in copy/move operations
//  copy props can fail in places where theres no easy way to let the user decide
//  how to handle the failure e.g. in constructors... so we allow the user
//  to write a function that communicates how to handle the error
// maybe forcing the result api on the user is too costly but i doubt it
// i would consider an error code as something more light weight
struct termiante_on_av_copy_props_failure {
    void operator()(result<void> result) noexcept {
        if (!result) {
            std::terminate();
        }
    }
};

// priorities: as regular as possible (at least movable)
//  stronger invariant compared to av frame
//  minimize flexibility penalty compared to ffmpeg c api
// tentaitve invariant:
//  av frame ptr is not null (unless explicitly requested with a non default policy)
//  if there are buffers there are correct buffer params
// alignment as a template parameter?
template <class av_frame_alloc_failure_policy, class av_copy_props_failure_policy>
class basic_frame {
    public:

    // although its a bit more opinioninated i think?
    //  going to force noexcept default construction.
    // i think its easier to work with rvalues instead of members
    //  for the function objects so ideally they should be free to constructr
    // worst case this assert can be disabled with macros 
    static_assert(noexcept(av_frame_alloc_failure_policy{}));
    static_assert(noexcept(av_copy_props_failure_policy{}));
    // review noexcept syntax and rules
    // pretty sure its just noexcept(bool) and bool noexcept(expr)
    static constexpr bool noexcept_frame_failure = 
        std::is_nothrow_invocable_v<av_frame_alloc_failure_policy, AVFrame*>;
    static constexpr bool noexcept_default = noexcept_frame_failure;

    static constexpr bool noexcept_copy_props_failure = 
        std::is_nothrow_invocable_v<av_copy_props_failure_policy, AVFrame*>;

    basic_frame() noexcept(noexcept_default) {}

    // does a deep copy of the frame, validates the invariant with a contract
    // helpful for integrating with code that already uses avframe
    explicit basic_frame(const AVFrame*);
    // move ref isntead of deep copy?
    //  idk if this overload is surprising. one steals the buffer one doesnt
    //  and theres  no std::move like signal
    explicit basic_frame(AVFrame*);

    // constructors from buffer params
    // thinking types with invariants for the buffer and auto params
    // e.g. a frame params struct with width height and format
    // explicit basic_frame(video_buffer_params);
    // explicit basic_frame(audio_buffer_params);
    // just a rogue idea at this point but maybe
    //  a variant of video and audio buffer params
    //  could be useful in some way to enforce
    //  that its either an audio or video frame
    //  when it comes to managing the parameters that affect the buffer


    // we can write them but i think implict copy is too expensive to be implicit
    basic_frame(const basic_frame&) = delete;
    basic_frame& operator=(const basic_frame&) = delete;

    basic_frame(basic_frame&& other) noexcept(noexcept_default && noexcept_copy_props_failure) {
        av_copy_props_failure_policy{}(copy_props(other));
        av_frame_move_ref(frame_.get(), other.frame_.get());
    }
    basic_frame& operator=(basic_frame&& other) noexcept(noexcept_copy_props_failure) {
        av_copy_props_failure_policy{}(copy_props(other));
        // https://ffmpeg.org/doxygen/3.3/group__lavu__frame.html#ga709e62bc2917ffd84c5c0f4e1dfc48f7
        av_frame_unref(frame_.get());
        av_frame_move_ref(frame_.get(), other.frame_.get());
    }

    // decision
    // update: deciding that at least move smeantics are critical
    //  for being a usability improvement over avframe
    // // instead of move semantics?
    // void move_ref(frame&& other) noexcept {
    //     av_frame_unref(frame_.get());
    //     av_frame_move_ref(frame_.get(), other.frame_.get());
    // }

    // // then we can have shortcuts to combinations of operations?
    // friend result<frame> move(frame&& other) noexcept {
    //     auto f = frame{};
    //     auto ec = int{};
    //     ec = f.copy_props(std::move(other));
    //     if (ec) return ec;
    //     f.move(std::move(other));
    //     return f;
    // }
    // honestly for a type to be used normally (i.e. not wrapped in a pointer)
    //  at all it needs at least some copy/move semantics.
    //  i think the above are as good as i can think of for default move semantics
    //  the only problem is theyre pretty expensive and can fail
    //  maybe just bite the bullet on another policy for copy_props?
    //  another policy just feels kindof verbose and maybe like
    //  its asking the user for too much work? like they could
    //  just make their own frame class at that point?
    //  there will be defaults so the average user shouldnt have to do anything
    //  not really sure the best approach but i think having moves at least
    //  is going to be really important in order to make this frame
    //  have an actual convenience improvement over avframe



    // example getters and setters
    //  could do this for all of the fields
    int64_t best_effort_timestamp() const noexcept {
        return av_frame_get_best_effort_timestamp(frame_.get());
    }

    basic_frame& best_effort_timestamp(int64_t val) const noexcept {
        av_frame_set_best_effort_timestamp(frame_.get(), val);
        return *this;
    }

    // todo tons of other getters/setters that dont already have free functions

    // just getters for the width height and format (and audio buffer params)
    int width() const noexcept {
        return frame_->width;
    }
    int height() const noexcept {
        return frame_->height;
    }
    int nb_samples() const noexcept {
        return frame_->width;
    }
    int channel_layout() const noexcept {
        return frame_->height;
    }
    int format() const noexcept {
        return frame_->format;
    }


    // if we go with the invariant that if there are buffers
    //  always match the buffer params then a copy props call
    //  is needed on all copy/move operations

    // error handling? naked error code? void result? can i do result<this&>?
    // cant be a free function unless i expose AVFrame* which i think is prob too error prone?
    //  sometimes you do need the AVFrame* i think so idk
    // idk if im sold on the frame.copy(other) style api
    //  as it compares to the original copy(dst, other) style
    //  idk why it just doesnt read right to me?
    //  this is the only way afaik to do it without exposing AVFrame* so
    //  its going to have to stay for now i think
    result<void> copy_props(const basic_frame& other) noexcept {
        return errc{av_frame_copy_props(frame_.get(), other.frame_.get())};
    }
    // in thinking shared ownership wont be allowed?
    // which means the make writable functions wont be necessary?
    // maybe shared ownership is actually a good choice for copy now?
    //  i guess it will still require a copy_props call to maintain the invariant
    //  so it wont be better than the move operations
    // is shared ownership of the frame considered dangerous/error prone?
    //  or an essential option for managing the frame?
    // i feel like its def too dangerous as a copy constructor/assignment
    //  but i guess its okay to let the user opt into it if they want
    // what additional posibilities does the user/us have in terms of maintaing the 
    //  invariant if this funciton is allowed? maybe we can wait until this 
    //  functionality is requested?
    // i feel like we shouldnt offer shallow copies without synchronization
    //   since the api is almost misleadingly unsafe at that point
    //  i can see maybe someone wants to add their own concurrency on top
    //   and not have to deal with a second heap allocation from a shared prt?
    //  i guess i have to see the effort to support it before deciding
    result<void> ref(const basic_frame& other) noexcept {
        LUMA_AV_OUTCOME_TRY(this->copy_props_props(other));
        // safe even if buffers are allocated and not reference counted?
        av_frame_unref(frame_.get());
        return errc{av_frame_ref(frame_.get(), other.frame_.get())};
    }
    
    // this api is questionable 
    //  the dst can have buffer params that dont reflect the buffer
    //  theres also the chance that the dst buffer isnt initialized right
    //  i think this one should probably be abstracted away
    // int copy_buffers(const frame& other) noexcept {
    //     av_frame_copy(frame_.get(), other.frame_.get());
    // }

    // i think its prob better just to do a full copy
    result<void> copy(const basic_frame& other) {
        // todo weird at all that this doesnt use the users policy?
        //  could make it more explicit that its just for the moves
        //  since theres no way to handle the error
        LUMA_AV_OUTCOME_TRY(this->copy_props_props(other));
        // todo align?
        // i think its based on the cpu so can it be known at compile time?
        //  but its only needed for buffers so maybe its misleading as a tp?
        //  other issues from mismatching alignments and stuff?
        //  i think it can change the size of the buffer so it would be bad
        //  to mix frames of different algnment so 
        const auto align = int{32};
        // todo ontract violation if buffer params arent set
        auto ec = av_frame_get_buffer(frame_.get(), align);
        return errc{ec};
    }

    // prob not going to have a frame alloc wrapper?
    //  i hope its not too opinionated to try to make
    //  the invariant around a non null frame (i think that makes the class way easier)
    //  you can still get null frames with non default customization of the class
    //  but thats mostly for error handling and the user can try again by making a new object
    //  they dont need to have an alloc method
    //   
    // frame& frame_alloc();

    // only audio or video buffers can be allocated at a time
    //  so i want the api to try to express that. i think
    //  just one overloaded "alloc buffers" with audio vs video params
    //  signals that its one or the other
    result<void> alloc_buffers(int width, int height, int format) {
        // may need a buffer unref? anything else in case the buffers are
        //   already allocated? 
        av_frame_unref(frame_.get());
        frame_->width = width;
        frame_->height = height;
        frame_->format = format;
        // todo again deal with align
        constexpr int align = 32;
        // maybe there can be a better exception guarentee here
        //  like the old buffers stay if the new ones fail to allocate
        //  actually idk the buffers need to be unrefed before the call for sure
        // todo there should also be the option to use a custom allocator here
        return errc{av_frame_get_buffer(frame_.get(), align)};
    }

    // overload for audio params
    // result<void> alloc_buffers() {

    // }

    //format needs to be correct per the invariant
    //  otherwise this bounds check isnt actually safe
    int num_planes() const {
        auto result = int{av_pix_fmt_count_planes(frame_->format)};
        // contrat that result is positive?
        // maybe let the user deal with an error code if its negative?
        return result;
    }

    // todo not null?
    //  other types besides uint8_t that avframe supports?
    uint8_t* data(int idx) const {
        // contract that data plane index is in range
        assert(idx > 0 && idx < num_planes());
        //  i think the avimage or avpicture api is used for that
        return frame_->data[idx];
    }


    int linesize(int idx) const {
        // contract that linesize is in range
        assert(idx > 0 && idx < num_planes());
        //  i think the avimage or avpicture api is used for that
        return frame_->linesize[idx];
    }

    // maybe this can be a codec context member function?
    // friend here seems kindof lazy and scary
    //  maybe i can just bite the bullet and allow for an avframe* member
    //  it pretty much needs to be there anyway since otherwise 
    //  the user cant integrate with any ffmpeg code that isnt wrapped here
    // friend result<void> receive_frame(AVCodecContext *avctx, AVFrame *frame) {

    // }

    // yea its not so bad i guess if we discourage use and offer alternatives
    AVFrame* avframe_ptr() noexcept {
        return frame_.get();
    }
    const AVFrame* avframe_ptr() const noexcept {
        return frame_.get();
    }

    // todo extended data?

    // todo buf and extended buf?

    // todo get plane buffer api?

    // todo side data api?


private:    
    struct frame_deleter {
        void operator()(AVFrame* frame) const noexcept {
            av_frame_free(&frame);
        }
    };

    // probably needs to be more than justa private function
    //  noexcept if check frame alloc is noexcept
    AVFrame* checked_frame_alloc() {
        auto* frame = av_frame_alloc();
        av_frame_alloc_failure_policy{}(frame);
        return frame;
    }
    std::unique_ptr<AVFrame, frame_deleter> frame_{checked_frame_alloc(), frame_deleter{}};

};

using frame = basic_frame<termiante_on_frame_alloc_failure,
                          termiante_on_av_copy_props_failure>;


// unsure if these should be detail
// on one hand the frame invariant should always be writable
//  so only advanced users messing with the underlying AVFrame
//  will need theses. at the same time advanced users
//  will prob need these
// should they take a ptr or the frame class?
// the frame is more flexible but its not expressive that it cant be null
inline result<void> is_writable(frame& f) {
    // why isnt this const?
    auto ec = av_frame_is_writable(f.avframe_ptr());
    return errc{ec};
}
inline result<void> make_writable(frame& f) {
    auto ec = av_frame_make_writable(f.avframe_ptr());
    return errc{ec};
}

} // av
} // luma

#endif // LUMA_AV_FRAME_HPP