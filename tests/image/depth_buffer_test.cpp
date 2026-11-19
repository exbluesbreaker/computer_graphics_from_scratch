#include <cgfs/image/canvas.h>
#include <cgfs/image/depth_buffer.h>

#include <catch2/catch_test_macros.hpp>
#include <stdexcept>

using cgfs::Canvas;
using cgfs::DepthBuffer;

TEST_CASE("DepthBuffer rejects non-positive dimensions", "[depth_buffer]") {
    CHECK_THROWS_AS(DepthBuffer(0, 10), std::invalid_argument);
    CHECK_THROWS_AS(DepthBuffer(10, -1), std::invalid_argument);
}

TEST_CASE("DepthBuffer takes its size from a canvas", "[depth_buffer]") {
    const Canvas canvas(6, 4);
    const DepthBuffer depth_buffer(canvas);
    CHECK(depth_buffer.width() == 6);
    CHECK(depth_buffer.height() == 4);
}

TEST_CASE("DepthBuffer: an empty pixel holds 0, infinitely far", "[depth_buffer]") {
    const DepthBuffer depth_buffer(4, 4);
    for (int x = -2; x < 2; ++x) {
        for (int y = -2; y < 2; ++y) {
            INFO("pixel (" << x << ", " << y << ")");
            CHECK(depth_buffer.InvZ(x, y) == 0.0);
        }
    }
}

TEST_CASE("DepthBuffer: anything in front of the camera is nearer than nothing", "[depth_buffer]") {
    DepthBuffer depth_buffer(4, 4);
    CHECK(depth_buffer.UpdateIfCloser(0, 0, 1e-9));  // z = a thousand million
    CHECK(depth_buffer.InvZ(0, 0) == 1e-9);
}

TEST_CASE("DepthBuffer: a nearer surface replaces a farther one and not the other way around",
          "[depth_buffer]") {
    DepthBuffer depth_buffer(4, 4);
    CHECK(depth_buffer.UpdateIfCloser(0, 0, 0.25));  // z = 4
    CHECK(depth_buffer.UpdateIfCloser(0, 0, 0.5));   // z = 2: nearer, so 1/z is larger
    CHECK(depth_buffer.InvZ(0, 0) == 0.5);
    CHECK_FALSE(depth_buffer.UpdateIfCloser(0, 0, 0.25));  // z = 4 again: behind
    CHECK(depth_buffer.InvZ(0, 0) == 0.5);
    CHECK(depth_buffer.UpdateIfCloser(0, 0, 1.0));  // z = 1
    CHECK(depth_buffer.InvZ(0, 0) == 1.0);
}

TEST_CASE("DepthBuffer: of two surfaces equally deep the first stays", "[depth_buffer]") {
    DepthBuffer depth_buffer(4, 4);
    CHECK(depth_buffer.UpdateIfCloser(1, -1, 0.5));
    CHECK_FALSE(depth_buffer.UpdateIfCloser(1, -1, 0.5));
    CHECK(depth_buffer.InvZ(1, -1) == 0.5);
}

TEST_CASE("DepthBuffer: infinitely far and behind the camera are never the nearest",
          "[depth_buffer]") {
    DepthBuffer depth_buffer(4, 4);
    CHECK_FALSE(depth_buffer.UpdateIfCloser(0, 0, 0.0));
    CHECK_FALSE(depth_buffer.UpdateIfCloser(0, 0, -1.0));
    CHECK(depth_buffer.InvZ(0, 0) == 0.0);
}

TEST_CASE("DepthBuffer: every pixel has a depth of its own", "[depth_buffer]") {
    DepthBuffer depth_buffer(4, 4);
    CHECK(depth_buffer.UpdateIfCloser(-2, 1, 0.5));   // the top-left corner
    CHECK(depth_buffer.UpdateIfCloser(1, -2, 0.25));  // the bottom-right corner
    CHECK(depth_buffer.InvZ(-2, 1) == 0.5);
    CHECK(depth_buffer.InvZ(1, -2) == 0.25);
    CHECK(depth_buffer.InvZ(0, 0) == 0.0);
    // What is behind one pixel's surface is in front of the other's.
    CHECK_FALSE(depth_buffer.UpdateIfCloser(-2, 1, 0.3));
    CHECK(depth_buffer.UpdateIfCloser(1, -2, 0.3));
}

TEST_CASE("DepthBuffer: a pixel outside the buffer is never the nearest", "[depth_buffer]") {
    DepthBuffer depth_buffer(4, 4);
    CHECK_FALSE(depth_buffer.UpdateIfCloser(2, 0, 1.0));   // x must be < width/2
    CHECK_FALSE(depth_buffer.UpdateIfCloser(-3, 0, 1.0));  // x must be >= -width/2
    CHECK_FALSE(depth_buffer.UpdateIfCloser(0, 2, 1.0));   // y must be < height/2
    CHECK_FALSE(depth_buffer.UpdateIfCloser(0, -3, 1.0));  // y must be >= -height/2
    CHECK_FALSE(depth_buffer.InvZ(2, 0).has_value());
    CHECK_FALSE(depth_buffer.InvZ(0, -3).has_value());
}

TEST_CASE("DepthBuffer has exactly the pixels of a canvas of its size", "[depth_buffer]") {
    // An odd size, where the range of the coordinates is not symmetric.
    const Canvas canvas(5, 3);
    DepthBuffer depth_buffer(canvas);
    for (int x = -4; x <= 4; ++x) {
        for (int y = -4; y <= 4; ++y) {
            INFO("pixel (" << x << ", " << y << ")");
            const bool on_canvas = canvas.GetPixel(x, y).has_value();
            CHECK(depth_buffer.InvZ(x, y).has_value() == on_canvas);
            CHECK(depth_buffer.UpdateIfCloser(x, y, 1.0) == on_canvas);
        }
    }
}
