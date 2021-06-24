
#include <luma_av/frame.hpp>

#include <luma_av/codec.hpp>
#include <gtest/gtest.h>

using namespace luma_av;


/**
memory safe construct and destruct
*/
TEST(frame_unit, default_ctor) {
    auto f = Frame::make().value();
}

/**
memory safe move construct and destruct
*/
TEST(frame_unit, default_move) {
    auto f = Frame::make().value();
    auto f2 = Frame{std::move(f)};
}

// /**
// memory safe construct, buffer aloc, and destruct
// */
// TEST(frame_unit, buffer_alloc) {
//     frame f{};
//     f.alloc_buffers(1920, 1080, AV_PIX_FMT_RGB24).value();
//     ASSERT_EQ(f.width(), 1920);
//     ASSERT_EQ(f.height(), 1080);
//     ASSERT_EQ(f.format(), AV_PIX_FMT_RGB24);
//     ASSERT_NE(f.data(0), nullptr);
//     // rgb24 has only one plane
//     ASSERT_ANY_THROW(f.data(1));
//     ASSERT_ANY_THROW(f.linesize(1));
//     // todo im pretty sure this is only the min alignment since it doesnt
//     //  factor in the alignment
//     auto min_linesize = av_image_get_linesize(AV_PIX_FMT_RGB24, 1920, 1080);
//     ASSERT_EQ(f.linesize(0), min_linesize);
// }

// TEST(frame_unit, move_alloc) {
//     frame f{};
//     f.alloc_buffers(1920, 1080, AV_PIX_FMT_YUV420P).value();
//     auto f2 = frame{std::move(f)};
//     ASSERT_EQ(f2.width(), 1920);
//     ASSERT_EQ(f2.height(), 1080);
//     ASSERT_EQ(f2.format(), AV_PIX_FMT_YUV420P);
// }

// TEST(frame_unit, alloc_move) {
//     frame f{};
//     auto f2 = frame{std::move(f)};
//     f2.alloc_buffers(1920, 1080, AV_PIX_FMT_YUV420P).value();
//     ASSERT_EQ(f2.width(), 1920);
//     ASSERT_EQ(f2.height(), 1080);
//     ASSERT_EQ(f2.format(), AV_PIX_FMT_YUV420P);
// }
