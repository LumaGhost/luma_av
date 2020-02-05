
#ifndef LUMA_AV_FRAME_HPP
#define LUMA_AV_FRAME_HPP

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
}

#include <memory>

#include <luma/av/result.hpp>
#include <luma/av/detail/unique_or_null.hpp>

namespace luma {
namespace av {

namespace detail {

struct frame_deleter {
    void operator()(AVFrame* frame) const noexcept {
        av_frame_free(&frame);
    }
};

} // detail


template <int alignment>
class basic_frame : public detail::unique_or_null<AVFrame,
                                    detail::frame_deleter> {

    public:
    using base_type = detail::unique_or_null<AVFrame,
                                             detail::frame_deleter>;
    using base_type::get;
    public:

    basic_frame() : base_type{checked_frame_alloc().value()} {}

    // does a deep copy of the frame, validates the invariant with a contract
    // helpful for integrating with code that already uses avframe
    explicit basic_frame(const AVFrame*);
    // move ref isntead of deep copy?
    //  idk if this overload is surprising. one steals the buffer one doesnt
    //  and theres  no std::move like signal
    // explicit basic_frame(AVFrame*);

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

    basic_frame(basic_frame&& other) = default;
    basic_frame& operator=(basic_frame&& other) = default;



    // example getters and setters
    //  could do this for all of the fields
    int64_t best_effort_timestamp() const noexcept {
        return av_frame_get_best_effort_timestamp(this->get());
    }

    basic_frame& best_effort_timestamp(int64_t val) const noexcept {
        av_frame_set_best_effort_timestamp(this->get(), val);
        return *this;
    }

    // todo tons of other getters/setters that dont already have free functions

    // just getters for the width height and format (and audio buffer params)
    int width() const noexcept {
        return this->get()->width;
    }
    int height() const noexcept {
        return this->get()->height;
    }
    int nb_samples() const noexcept {
        return this->get()->width;
    }
    int channel_layout() const noexcept {
        return this->get()->height;
    }
    int format() const noexcept {
        return this->get()->format;
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
    // result<void> copy_props(const basic_frame& other) noexcept {
    //     return errc{av_frame_copy_props(frame_.get(), other.frame_.get())};
    // }

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
    // result<void> ref(const basic_frame& other) noexcept {
    //     LUMA_AV_OUTCOME_TRY(this->copy_props_props(other));
    //     // safe even if buffers are allocated and not reference counted?
    //     av_frame_unref(frame_.get());
    //     return errc{av_frame_ref(frame_.get(), other.frame_.get())};
    // }
    // todo will shallow copies be allowed?

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
        av_frame_unref(this->get());
        this->get()->width = width;
        this->get()->height = height;
        this->get()->format = format;
        // todo again deal with align
        constexpr int align = 32;
        // maybe there can be a better exception guarentee here
        //  like the old buffers stay if the new ones fail to allocate
        //  actually idk the buffers need to be unrefed before the call for sure
        // todo there should also be the option to use a custom allocator here
        return detail::ffmpeg_code_to_result(av_frame_get_buffer(this->get(), align));
    }

    // overload for audio params
    // result<void> alloc_buffers() {

    // }

    //format needs to be correct per the invariant
    //  otherwise this bounds check isnt actually safe
    int num_planes() const {
        auto result = int{av_pix_fmt_count_planes(this->get()->format)};
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
        return this->get()->data[idx];
    }


    int linesize(int idx) const {
        // contract that linesize is in range
        assert(idx > 0 && idx < num_planes());
        //  i think the avimage or avpicture api is used for that
        return this->get()->linesize[idx];
    }

    // maybe this can be a codec context member function?
    // friend here seems kindof lazy and scary
    //  maybe i can just bite the bullet and allow for an avframe* member
    //  it pretty much needs to be there anyway since otherwise 
    //  the user cant integrate with any ffmpeg code that isnt wrapped here
    // friend result<void> receive_frame(AVCodecContext *avctx, AVFrame *frame) {

    // }

    // todo extended data?

    // todo buf and extended buf?

    // todo get plane buffer api?

    // todo side data api?


private:    
    // probably needs to be more than justa private function
    //  noexcept if check frame alloc is noexcept
    result<base_type> checked_frame_alloc() {
        auto* frame = av_frame_alloc();
        if (frame) {
            return base_type{frame};
        } else {
            return luma::av::make_error_code(errc::alloc_failure);
        }
    }

};

using frame = basic_frame<32>;


// unsure if these should be detail
// on one hand the frame invariant should always be writable
//  so only advanced users messing with the underlying AVFrame
//  will need theses. at the same time advanced users
//  will prob need these
// should they take a ptr or the frame class?
// the frame is more flexible but its not expressive that it cant be null
inline result<void> is_writable(frame& f) {
    // why isnt this const?
    auto ec = av_frame_is_writable(f.get());
    return detail::ffmpeg_code_to_result(ec);
}
inline result<void> make_writable(frame& f) {
    auto ec = av_frame_make_writable(f.get());
    return detail::ffmpeg_code_to_result(ec);
}

inline result<void> copy_props(frame& dst, const frame& src) noexcept {
    return detail::ffmpeg_code_to_result(av_frame_copy_props(dst.get(), src.get()));
}

// this api is questionable 
//  the dst can have buffer params that dont reflect the buffer
//  theres also the chance that the dst buffer isnt initialized right
//  i think this one should probably be abstracted away
// int copy_buffers(const frame& other) noexcept {
//     av_frame_copy(frame_.get(), other.frame_.get());
// }

// i think its prob better just to do a full copy
// after some time i think free functions are better for this
//  sort of operation since its more consistent with the ffmpeg api style
// frame.copy(other) isnt as explicit about src/destination
//  is frame copied into other or other into frame?
//  after seeing it on packet i definitely prefer the
//  deep copy construction from AVFrame*/AVPacket*
//  over a member function like this
inline result<void> copy_frame(frame& dst, const frame& src) {
    // todo weird at all that this doesnt use the users policy?
    //  could make it more explicit that its just for the moves
    //  since theres no way to handle the error
    LUMA_AV_OUTCOME_TRY(luma::av::copy_props(dst, src));
    // todo align?
    // i think its based on the cpu so can it be known at compile time?
    //  but its only needed for buffers so maybe its misleading as a tp?
    //  other issues from mismatching alignments and stuff?
    //  i think it can change the size of the buffer so it would be bad
    //  to mix frames of different algnment so 
    const auto align = int{32};
    // todo ontract violation if buffer params arent set
    auto ec = av_frame_get_buffer(dst.get(), align);
    /*
    i think a copy implementation is actually more complicated than this
        (this isnt even finished because we still need to copy the buffer data)
        but there are also more cases to handle, like if there are no buffers
        and copy props is enough to do a full deep copy. maybe the dst
        frame already has buffers allocated so we can just copy the data
    */
    return detail::ffmpeg_code_to_result(ec);
}

} // av
} // luma

#endif // LUMA_AV_FRAME_HPP