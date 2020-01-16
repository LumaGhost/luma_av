

// https://ffmpeg.org/doxygen/3.3/group__lavu__frame.html

extern "C" {
#include <libavutil/frame.h>
}

#include <memory>

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
}

struct throw_on_frame_alloc_failure {
    void operator()(const AVFrame* frame) {
        if (!frame) {
            throw std::runtime_error{};
        }
    }
}

struct ignore_frame_alloc_failure {
    void operator()(const AVFrame* frame) noexcept {
        
    }
}

// tentaitve invariant:
//  av frame ptr is not null (unless explicitly requested with a non default policy)
//  if there are buffers there are correct buffer params
// alignment as a template parameter?
template <class av_frame_alloc_failure_policy>
class frame {

    // review noexcept syntax and rules
    // pretty sure its just noexcept(bool) and bool noexcept(expr)
    static constexpr bool noexcept_failure_policy = 
        std::is_nothrow_invocable_v<av_frame_alloc_failure_policy, AVFrame*>;
    static constexpr bool noexcept_default = 
        noexcept(av_frame_alloc_failure_policy{})
        && noexcept_failure_policy

    frame() noexcept(noexcept_default) = default;

    frame(const frame&) = delete;
    frame& operator=(const frame&) = delete;

    // fuck i realised i need a copy_props call in here which can also fail
    //  maybe i can just go with no implicit move/copy and let the user
    //  manage themselves
    frame(frame&& other) noexcept(noexcept_default) {
        av_frame_move_ref(frame_.get(), other.frame_.get());
    }
    frame& operator=(frame&& other) noexcept {
        // https://ffmpeg.org/doxygen/3.3/group__lavu__frame.html#ga709e62bc2917ffd84c5c0f4e1dfc48f7
        av_frame_unref(frame_.get());
        av_frame_move_ref(frame_.get(), other.frame_.get());
    }
    // instead of move semantics?
    void move_ref(frame&& other) noexcept {
        av_frame_unref(frame_.get());
        av_frame_move_ref(frame_.get(), other.frame_.get());
    }

    template <class T>
    using result = std::variant<T, int>;
    // then we can have shortcuts to combinations of operations?
    friend result<frame> move(frame&& other) noexcept {
        auto f = frame{};
        auto ec = int{};
        ec = f.copy_props(std::move(other));
        if (ec) return ec;
        f.move(std::move(other));
        return f;
    }
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

    frame& best_effort_timestamp(int64_t val) const noexcept {
        av_frame_set_best_effort_timestamp(frame_.get(), val);
        return *this;
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
    int copy_props(const frame& other) noexcept {
        return av_frame_copy_props(frame_.get(), other.frame_.get());
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
    int ref(const frame& other) noexcept {
        auto ec = this->copy_props_props(other);
        if (ec) return ec;
        // safe even if buffers are allocated and not reference counted?
        av_frame_unref(frame_.get());
        return av_frame_ref(frame_.get(), other.frame_.get())
    }
    
    // this api is questionable 
    //  the dst can have buffer params that dont reflect the buffer
    //  theres also the chance that the dst buffer isnt initialized right
    //  i think this one should probably be abstracted away
    // int copy_buffers(const frame& other) noexcept {
    //     av_frame_copy(frame_.get(), other.frame_.get());
    // }

    // i think its prob better just to do a full copy
    int copy(const frame& other) {
        auto ec = int{};
        ec = this->copy_props_props(other);
        if (ec) return ec;
        // align?
        // i think its based on the cpu so can it be known at compile time?
        //  but its only needed for buffers so maybe its misleading as a tp?
        //  other issues from mismatching alignments and stuff?
        //  i think it can change the size of the buffer so it would be bad
        //  to mix frames of different algnment so 
        const auto align = int{32};
        // contract violation if buffer params arent set
        ec = av_frame_get_buffer(frame_.get(), align);
        return ec;
    }

    // prob not going to have a frame alloc wrapper?
    //  i hope its not too opinionated to try to make
    //  the invariant around a non null frame (i think that makes the class way easier)
    //  you can still get null frames with non default customization of the class
    //  but thats mostly for error handling and the user can try again by making a new object
    //  they dont need to have an alloc method
    //   
    frame& frame_alloc();


private:    
    struct frame_deleter {
        void operator()(AVFrame* frame) const noexcept {
            av_frame_free(&frame);
        }
    }

    // probably needs to be more than justa private function
    //  noexcept if check frame alloc is noexcept
    AVFrame* checked_frame_alloc() {
        auto* frame = av_frame_alloc();
        check_frame_alloc(frame);
        return frame;
    }
    av_frame_alloc_failure_policy check_frame_alloc_;
    std::unique_ptr<AVFrame, frame_deleter> frame_{checked_frame_alloc(), frame_deleter{}};

};