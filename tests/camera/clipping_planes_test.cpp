#include <cgfs/camera/clipping_planes.h>
#include <cgfs/camera/viewport.h>
#include <cgfs/math/plane.h>
#include <cgfs/math/vec3.h>

#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <vector>

#include "support/test_helpers.h"

using cgfs::Length;
using cgfs::MakeClippingPlanes;
using cgfs::Plane;
using cgfs::SignedDistance;
using cgfs::Vec3;
using cgfs::Viewport;

namespace {

constexpr double kEpsilon = 1e-12;

bool Near(double a, double b) { return std::abs(a - b) < kEpsilon; }

bool Near(Vec3 a, Vec3 b) { return Near(a.x, b.x) && Near(a.y, b.y) && Near(a.z, b.z); }

// The order MakeClippingPlanes returns its planes in.
enum : std::size_t { kNear, kLeft, kRight, kBottom, kTop };

// In front of every plane: inside the clipping volume.
bool InVolume(const std::vector<Plane>& planes, Vec3 point) {
    for (const Plane& plane : planes) {
        if (SignedDistance(plane, point) < 0.0) {
            return false;
        }
    }
    return true;
}

// The viewport of the demo pictures on a 320 x 180 canvas: 16/9 wide, 1 high, at 1.
Viewport WideViewport() { return Viewport(320, 180, 16.0 / 9.0, 1.0, 1.0); }

}  // namespace

TEST_CASE("MakeClippingPlanes: five planes, the near one first, at the viewport's distance",
          "[clipping_planes]") {
    const std::vector<Plane> planes = MakeClippingPlanes(Viewport(320, 180));
    REQUIRE(planes.size() == 5);
    CHECK(planes[kNear] == Plane{.normal = {0.0, 0.0, 1.0}, .d = -1.0});

    const std::vector<Plane> farther = MakeClippingPlanes(Viewport(320, 180, 1.0, 1.0, 2.5));
    CHECK(farther[kNear] == Plane{.normal = {0.0, 0.0, 1.0}, .d = -2.5});
}

TEST_CASE("MakeClippingPlanes: a field of view of 90 degrees gives the book's planes",
          "[clipping_planes]") {
    // A viewport twice as wide and as high as it is far away, on a canvas of so many
    // pixels that the half pixel added on every side does not show in six decimals.
    const std::vector<Plane> planes =
        MakeClippingPlanes(Viewport(1'000'000'000, 1'000'000'000, 2.0, 2.0, 1.0));
    const double s = 1.0 / std::sqrt(2.0);
    const auto near_enough = [](Vec3 a, Vec3 b) { return Length(a - b) < 1e-6; };
    CHECK(near_enough(planes[kLeft].normal, {s, 0.0, s}));
    CHECK(near_enough(planes[kRight].normal, {-s, 0.0, s}));
    CHECK(near_enough(planes[kBottom].normal, {0.0, s, s}));
    CHECK(near_enough(planes[kTop].normal, {0.0, -s, s}));
    for (const std::size_t side : {kLeft, kRight, kBottom, kTop}) {
        CHECK(planes[side].d == 0.0);  // they pass through the eye
    }
}

TEST_CASE("MakeClippingPlanes: the side planes are the viewport's sides and half a pixel more",
          "[clipping_planes]") {
    // 2 x 1 at distance 2 on 64 x 32 pixels: a pixel is 1/32 of the viewport's units
    // each way, so the planes pass through x = ±(1 + 1/64) and y = ±(1/2 + 1/64) on the
    // viewport plane. The normals are those vectors turned a quarter.
    const std::vector<Plane> planes = MakeClippingPlanes(Viewport(64, 32, 2.0, 1.0, 2.0));
    const double half_width = 1.0 + 1.0 / 64.0;
    const double half_height = 0.5 + 1.0 / 64.0;
    CHECK(Near(planes[kLeft].normal, cgfs::Normalized(Vec3{2.0, 0.0, half_width})));
    CHECK(Near(planes[kRight].normal, cgfs::Normalized(Vec3{-2.0, 0.0, half_width})));
    CHECK(Near(planes[kBottom].normal, cgfs::Normalized(Vec3{0.0, 2.0, half_height})));
    CHECK(Near(planes[kTop].normal, cgfs::Normalized(Vec3{0.0, -2.0, half_height})));
}

TEST_CASE("MakeClippingPlanes: every normal has length 1", "[clipping_planes]") {
    for (const Viewport& viewport :
         {Viewport(320, 180), WideViewport(), Viewport(64, 32, 2.0, 1.0, 2.0),
          Viewport(100, 300, 0.3, 0.9, 7.0)}) {
        for (const Plane& plane : MakeClippingPlanes(viewport)) {
            CHECK(Near(Length(plane.normal), 1.0));
        }
    }
}

TEST_CASE(
    "MakeClippingPlanes: the viewport's rectangle, half a pixel larger, lies on the "
    "planes",
    "[clipping_planes]") {
    const Viewport viewport = WideViewport();
    const std::vector<Plane> planes = MakeClippingPlanes(viewport);
    const double half_w = viewport.width() / 2.0 + viewport.width() / 640.0;
    const double half_h = viewport.height() / 2.0 + viewport.height() / 360.0;
    const double d = viewport.distance();

    // Its center is on the near plane and in front of the other four.
    const Vec3 center{0.0, 0.0, d};
    CHECK(SignedDistance(planes[kNear], center) == 0.0);
    for (const std::size_t side : {kLeft, kRight, kBottom, kTop}) {
        CHECK(SignedDistance(planes[side], center) > 0.0);
    }

    // Each edge is on its own plane, all along, and so is every point on the ray from the
    // eye through it.
    for (const double along : {-1.0, -0.3, 0.0, 0.5, 1.0}) {
        for (const double depth : {1.0, 2.0, 37.5}) {
            CHECK(
                Near(SignedDistance(planes[kLeft], Vec3{-half_w, along * half_h, d} * depth), 0.0));
            CHECK(
                Near(SignedDistance(planes[kRight], Vec3{half_w, along * half_h, d} * depth), 0.0));
            CHECK(Near(SignedDistance(planes[kBottom], Vec3{along * half_w, -half_h, d} * depth),
                       0.0));
            CHECK(Near(SignedDistance(planes[kTop], Vec3{along * half_w, half_h, d} * depth), 0.0));
        }
    }

    // The viewport's own sides are inside the volume, by half a pixel, and the points
    // one pixel beyond them outside it, by the other half.
    const double pixel = viewport.width() / 320.0;
    const Vec3 on_left_side = viewport.CanvasToViewport(-160, 17);
    const Vec3 on_bottom_side = viewport.CanvasToViewport(33, -90);
    for (const Vec3 on_a_side : {on_left_side, viewport.CanvasToViewport(160, 17), on_bottom_side,
                                 viewport.CanvasToViewport(33, 90)}) {
        for (const Plane& plane : planes) {
            CHECK(SignedDistance(plane, on_a_side) >= 0.0);
        }
    }
    CHECK(SignedDistance(planes[kLeft], on_left_side) > 0.0);
    CHECK(SignedDistance(planes[kLeft], on_left_side) < pixel / 2.0);
    CHECK(SignedDistance(planes[kBottom], on_bottom_side) > 0.0);
    CHECK(SignedDistance(planes[kBottom], on_bottom_side) < pixel / 2.0);
    CHECK(SignedDistance(planes[kLeft], viewport.CanvasToViewport(-161, 17)) < 0.0);
    CHECK(SignedDistance(planes[kRight], viewport.CanvasToViewport(161, 17)) < 0.0);
    CHECK(SignedDistance(planes[kBottom], viewport.CanvasToViewport(33, -91)) < 0.0);
    CHECK(SignedDistance(planes[kTop], viewport.CanvasToViewport(33, 91)) < 0.0);
}

TEST_CASE("MakeClippingPlanes: a point on a side plane projects into the middle of a pixel",
          "[clipping_planes]") {
    // Where clipping makes new corners: on the left plane they project half a pixel
    // beyond the viewport's left side, x = -(160 + 1/2) on a canvas 320 wide, which
    // ToPixel turns into the leftmost column whatever the last bit says; and so on
    // around the canvas.
    const Viewport viewport = WideViewport();
    const std::vector<Plane> planes = MakeClippingPlanes(viewport);
    const double to_pixels = 320.0 / viewport.width();  // and 180 / the height: the same
    for (const double depth : {1.0, 2.5, 40.0}) {
        // A point of each plane at that depth: the plane's normal turned a quarter.
        const Vec3 on_left = Vec3{-planes[kLeft].normal.z, 0.0, planes[kLeft].normal.x};
        const Vec3 on_right = Vec3{planes[kRight].normal.z, 0.0, -planes[kRight].normal.x};
        const Vec3 on_bottom = Vec3{0.0, -planes[kBottom].normal.z, planes[kBottom].normal.y};
        const Vec3 on_top = Vec3{0.0, planes[kTop].normal.z, -planes[kTop].normal.y};
        CHECK(Near(on_left.x / on_left.z * to_pixels, -160.5));
        CHECK(Near(on_right.x / on_right.z * to_pixels, 160.5));
        CHECK(Near(on_bottom.y / on_bottom.z * to_pixels, -90.5));
        CHECK(Near(on_top.y / on_top.z * to_pixels, 90.5));
        CHECK(Near(SignedDistance(planes[kLeft], on_left * depth), 0.0));
        CHECK(Near(SignedDistance(planes[kTop], on_top * depth), 0.0));
    }
}

TEST_CASE(
    "MakeClippingPlanes: a point is in the volume when it projects inside the viewport "
    "and is not nearer than it",
    "[clipping_planes]") {
    // The viewport is 16/9 x 1 at distance 1: in view means |x / z| <= 8/9, |y / z| <= 1/2
    // and z >= 1, give or take the half pixel at the sides.
    const std::vector<Plane> planes = MakeClippingPlanes(WideViewport());
    CHECK(InVolume(planes, {0.0, 0.0, 5.0}));
    CHECK(InVolume(planes, {4.0, 2.0, 5.0}));  // x / z = 0.8, y / z = 0.4
    CHECK(InVolume(planes, {-4.0, -2.0, 5.0}));
    CHECK(InVolume(planes, {0.0, 0.0, 1.0}));         // on the near plane
    CHECK_FALSE(InVolume(planes, {4.6, 0.0, 5.0}));   // x / z = 0.92: off to the right
    CHECK_FALSE(InVolume(planes, {-4.6, 0.0, 5.0}));  // off to the left
    CHECK_FALSE(InVolume(planes, {0.0, 2.6, 5.0}));   // y / z = 0.52: above
    CHECK_FALSE(InVolume(planes, {0.0, -2.6, 5.0}));  // below
    CHECK_FALSE(InVolume(planes, {0.0, 0.0, 0.5}));   // between the eye and the viewport
    CHECK_FALSE(InVolume(planes, {0.0, 0.0, -5.0}));  // behind the camera
    CHECK_FALSE(InVolume(planes, {4.0, 2.0, -5.0}));  // behind it, on the rays of a point in view

    // Each of the four side planes rejects only its own side.
    const Vec3 right{4.6, 0.0, 5.0};
    CHECK(SignedDistance(planes[kRight], right) < 0.0);
    CHECK(SignedDistance(planes[kLeft], right) > 0.0);
    CHECK(SignedDistance(planes[kTop], right) > 0.0);
    CHECK(SignedDistance(planes[kBottom], right) > 0.0);
    const Vec3 above{0.0, 2.6, 5.0};
    CHECK(SignedDistance(planes[kTop], above) < 0.0);
    CHECK(SignedDistance(planes[kBottom], above) > 0.0);
    CHECK(SignedDistance(planes[kLeft], above) > 0.0);
    CHECK(SignedDistance(planes[kRight], above) > 0.0);
}

TEST_CASE(
    "MakeClippingPlanes: a viewport farther away narrows the volume and moves the near "
    "plane",
    "[clipping_planes]") {
    const std::vector<Plane> planes = MakeClippingPlanes(Viewport(320, 180, 16.0 / 9.0, 1.0, 2.0));
    CHECK(InVolume(planes, {0.0, 0.0, 5.0}));
    CHECK(InVolume(planes, {2.0, 1.0, 5.0}));        // x / z = 0.4 <= 4/9, y / z = 0.2 <= 1/4
    CHECK_FALSE(InVolume(planes, {4.0, 0.0, 5.0}));  // in view at distance 1, not at 2
    CHECK_FALSE(InVolume(planes, {0.0, 2.0, 5.0}));
    CHECK_FALSE(InVolume(planes, {0.0, 0.0, 1.5}));  // nearer than the viewport now
}
