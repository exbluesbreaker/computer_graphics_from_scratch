#include <cgfs/camera/viewport.h>
#include <cgfs/image/canvas.h>
#include <cgfs/image/color.h>
#include <cgfs/image/depth_buffer.h>
#include <cgfs/math/vec3.h>
#include <cgfs/rasterizer/object.h>
#include <cgfs/rasterizer/point.h>
#include <cgfs/rasterizer/triangle.h>

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <vector>

#include "rasterizer/pixel_helpers.h"
#include "support/test_helpers.h"

using cgfs::Canvas;
using cgfs::DepthBuffer;
using cgfs::DrawFilledTriangle;
using cgfs::DrawShadedTriangle;
using cgfs::DrawWireframeTriangle;
using cgfs::Point;
using cgfs::RenderObject;
using cgfs::Rgb8;
using cgfs::Triangle;
using cgfs::Vec3;
using cgfs::Viewport;
using cgfs_test::Bytes;
using cgfs_test::LitPixels;
using namespace cgfs::colors;

namespace {

// A 32 x 32 canvas seen through a 1 x 1 viewport at distance 1: a vertex (x, y, 1) on
// the viewport plane lands on the pixel (32 x, 32 y).
constexpr int kSize = 32;

Viewport TestViewport() { return Viewport(kSize, kSize); }

// A fresh black canvas with the object rendered on it.
Canvas CanvasWithObject(const std::vector<Vec3>& vertices, const std::vector<Triangle>& triangles) {
    Canvas canvas(kSize, kSize, kBlack);
    RenderObject(canvas, TestViewport(), vertices, triangles);
    return canvas;
}

// A fresh black canvas with some wireframe triangles on it, for the expected picture.
struct Outline {
    Point a;
    Point b;
    Point c;
    cgfs::Color color = kWhite;
};
Canvas CanvasWithOutlines(const std::vector<Outline>& outlines) {
    Canvas canvas(kSize, kSize, kBlack);
    for (const Outline& o : outlines) {
        DrawWireframeTriangle(canvas, o.a, o.b, o.c, o.color);
    }
    return canvas;
}

// The corners of the square |x|, |y| <= 1/4 on the viewport plane, counterclockwise from
// the bottom left: the pixels (-8, -8), (8, -8), (8, 8), (-8, 8).
const std::vector<Vec3> kSquare = {
    {-0.25, -0.25, 1.0}, {0.25, -0.25, 1.0}, {0.25, 0.25, 1.0}, {-0.25, 0.25, 1.0}};

}  // namespace

TEST_CASE("RenderObject: one triangle is the wireframe of its projected corners", "[rasterizer]") {
    const std::vector<Vec3> vertices = {{0.25, 0.25, 1.0}, {-0.25, 0.0, 1.0}, {0.125, -0.25, 1.0}};
    const std::vector<Triangle> triangles = {{{0, 1, 2}, kWhite}};
    const Canvas canvas = CanvasWithObject(vertices, triangles);
    CHECK(Bytes(canvas) == Bytes(CanvasWithOutlines({{{8, 8}, {-8, 0}, {4, -8}}})));
    CHECK(LitPixels(canvas).size() > 20);  // and something was drawn
}

TEST_CASE("RenderObject: a triangle names its corners by index, in any order", "[rasterizer]") {
    // The triangle uses vertices 3, 1 and 0; vertex 2, in the empty top-left corner,
    // leaves no pixel.
    const std::vector<Vec3> vertices = {
        {0.25, 0.25, 1.0}, {-0.25, 0.0, 1.0}, {-0.4, 0.4, 1.0}, {0.125, -0.25, 1.0}};
    const std::vector<Triangle> triangles = {{{3, 1, 0}, kWhite}};
    const Canvas canvas = CanvasWithObject(vertices, triangles);
    CHECK(Bytes(canvas) == Bytes(CanvasWithOutlines({{{4, -8}, {-8, 0}, {8, 8}}})));
    CHECK(canvas.GetPixel(-12, 12) == Rgb8{0, 0, 0});
}

TEST_CASE("RenderObject: shared vertices are projected once and the triangles meet",
          "[rasterizer]") {
    // The square cut along its diagonal: two triangles sharing the corners 0 and 2.
    const std::vector<Triangle> triangles = {{{0, 1, 2}, kWhite}, {{0, 2, 3}, kWhite}};
    const Canvas canvas = CanvasWithObject(kSquare, triangles);
    CHECK(Bytes(canvas) ==
          Bytes(CanvasWithOutlines({{{-8, -8}, {8, -8}, {8, 8}}, {{-8, -8}, {8, 8}, {-8, 8}}})));
    // The square's outline (17 pixels a side, corners counted once) and the 15 interior
    // pixels of its diagonal, which runs through (k, k) exactly.
    CHECK(LitPixels(canvas).size() == 4 * 17 - 4 + 15);
}

TEST_CASE("RenderObject: every triangle keeps its color and later ones draw over earlier ones",
          "[rasterizer]") {
    // The first triangle owns the bottom and right sides, the second the top and left
    // side; they share the diagonal, which the one drawn last colors.
    const std::vector<Triangle> red_then_blue = {{{0, 1, 2}, kRed}, {{0, 2, 3}, kBlue}};
    const Canvas canvas = CanvasWithObject(kSquare, red_then_blue);
    CHECK(canvas.GetPixel(0, -8) == Rgb8{255, 0, 0});  // bottom
    CHECK(canvas.GetPixel(8, 0) == Rgb8{255, 0, 0});   // right
    CHECK(canvas.GetPixel(0, 8) == Rgb8{0, 0, 255});   // top
    CHECK(canvas.GetPixel(-8, 0) == Rgb8{0, 0, 255});  // left
    CHECK(canvas.GetPixel(0, 0) == Rgb8{0, 0, 255});   // the diagonal, drawn second

    const std::vector<Triangle> blue_then_red = {{{0, 2, 3}, kBlue}, {{0, 1, 2}, kRed}};
    const Canvas reversed = CanvasWithObject(kSquare, blue_then_red);
    CHECK(reversed.GetPixel(0, 0) == Rgb8{255, 0, 0});
    CHECK(reversed.GetPixel(0, 8) == Rgb8{0, 0, 255});
}

TEST_CASE("RenderObject: the vertices are projected in perspective", "[rasterizer]") {
    // The same triangle twice as far from the camera with twice the size lies on the
    // same rays, so it makes the same picture.
    const std::vector<Vec3> near = {{0.25, 0.25, 1.0}, {-0.25, 0.25, 1.0}, {0.0, -0.25, 1.0}};
    const std::vector<Vec3> far = {{0.5, 0.5, 2.0}, {-0.5, 0.5, 2.0}, {0.0, -0.5, 2.0}};
    const std::vector<Triangle> triangles = {{{0, 1, 2}, kWhite}};
    const Canvas canvas = CanvasWithObject(far, triangles);
    CHECK(Bytes(canvas) == Bytes(CanvasWithObject(near, triangles)));
    CHECK(Bytes(canvas) == Bytes(CanvasWithOutlines({{{8, 8}, {-8, 8}, {0, -8}}})));

    // Twice as far without the scaling: half the size.
    const std::vector<Vec3> far_small = {{0.25, 0.25, 2.0}, {-0.25, 0.25, 2.0}, {0.0, -0.25, 2.0}};
    CHECK(Bytes(CanvasWithObject(far_small, triangles)) ==
          Bytes(CanvasWithOutlines({{{4, 4}, {-4, 4}, {0, -4}}})));
}

TEST_CASE("RenderObject: the vertex and triangle lists may be any contiguous range",
          "[rasterizer]") {
    const std::array<Vec3, 3> vertices = {Vec3{0.25, 0.25, 1.0}, Vec3{-0.25, 0.25, 1.0},
                                          Vec3{0.0, -0.25, 1.0}};
    const Triangle triangles[] = {{{0, 1, 2}, kWhite}};
    Canvas canvas(kSize, kSize, kBlack);
    RenderObject(canvas, TestViewport(), vertices, triangles);
    CHECK(Bytes(canvas) == Bytes(CanvasWithOutlines({{{8, 8}, {-8, 8}, {0, -8}}})));
}

TEST_CASE("RenderObject: an object without triangles draws nothing", "[rasterizer]") {
    CHECK(LitPixels(CanvasWithObject(kSquare, {})).empty());
    CHECK(LitPixels(CanvasWithObject({}, {})).empty());
}

TEST_CASE("Triangle: the intensity is 1 at every corner unless it says otherwise", "[rasterizer]") {
    const Triangle triangle{{0, 1, 2}, kRed};
    CHECK(triangle.h == std::array<double, 3>{1.0, 1.0, 1.0});
    CHECK(triangle == Triangle{{0, 1, 2}, kRed, {1.0, 1.0, 1.0}});
    CHECK_FALSE(triangle == Triangle{{0, 1, 2}, kRed, {1.0, 0.5, 1.0}});
}

TEST_CASE("RenderObject: the triangles are drawn with the primitive it is given", "[rasterizer]") {
    const std::vector<Vec3> vertices = {{0.25, 0.25, 1.0}, {-0.25, 0.0, 1.0}, {0.125, -0.25, 1.0}};
    const std::vector<Triangle> triangles = {{{0, 1, 2}, kRed}};
    const Point a{8, 8};
    const Point b{-8, 0};
    const Point c{4, -8};

    // An outline when nothing is said, which is saying DrawWireframeTriangle.
    Canvas outlined(kSize, kSize, kBlack);
    RenderObject(outlined, TestViewport(), vertices, triangles, DrawWireframeTriangle);
    CHECK(Bytes(outlined) == Bytes(CanvasWithObject(vertices, triangles)));

    // Filled: the canvas of DrawFilledTriangle on the projected corners.
    Canvas filled(kSize, kSize, kBlack);
    RenderObject(filled, TestViewport(), vertices, triangles, DrawFilledTriangle);
    Canvas filled_by_hand(kSize, kSize, kBlack);
    DrawFilledTriangle(filled_by_hand, a, b, c, kRed);
    CHECK(Bytes(filled) == Bytes(filled_by_hand));
    CHECK(LitPixels(filled).size() > LitPixels(outlined).size());

    // Shaded, with the default intensities: the full color everywhere, the filled one.
    Canvas shaded(kSize, kSize, kBlack);
    RenderObject(shaded, TestViewport(), vertices, triangles, DrawShadedTriangle);
    CHECK(Bytes(shaded) == Bytes(filled));
}

TEST_CASE("RenderObject: a shaded triangle takes its intensities from its corners",
          "[rasterizer]") {
    const std::vector<Vec3> vertices = {{0.25, 0.25, 1.0}, {-0.25, 0.0, 1.0}, {0.125, -0.25, 1.0}};
    const std::vector<Triangle> triangles = {{{0, 1, 2}, kGreen, {1.0, 0.25, 0.5}}};
    Canvas canvas(kSize, kSize, kBlack);
    RenderObject(canvas, TestViewport(), vertices, triangles, DrawShadedTriangle);

    Canvas by_hand(kSize, kSize, kBlack);
    DrawShadedTriangle(by_hand, {8, 8, 1.0}, {-8, 0, 0.25}, {4, -8, 0.5}, kGreen);
    CHECK(Bytes(canvas) == Bytes(by_hand));
    CHECK(canvas.GetPixel(8, 8) == Rgb8{0, 255, 0});   // the full color at the first corner
    CHECK(canvas.GetPixel(-8, 0) == Rgb8{0, 63, 0});   // a quarter of it at the second
    CHECK(canvas.GetPixel(4, -8) == Rgb8{0, 127, 0});  // half at the third

    // The intensity goes with the corner, whichever vertex the corner names.
    const std::vector<Triangle> other_order = {{{2, 0, 1}, kGreen, {0.5, 1.0, 0.25}}};
    Canvas same(kSize, kSize, kBlack);
    RenderObject(same, TestViewport(), vertices, other_order, DrawShadedTriangle);
    CHECK(Bytes(same) == Bytes(by_hand));
}

TEST_CASE("RenderObject: the intensity belongs to the triangle's corner, not to the vertex",
          "[rasterizer]") {
    // The square cut along its diagonal: the two triangles share the vertices 0 and 2,
    // and each has its own shade there, as two faces of a cube would.
    const std::vector<Triangle> triangles = {
        {{0, 1, 2}, kWhite, {1.0, 1.0, 1.0}},     // bottom right: bright
        {{0, 2, 3}, kWhite, {0.25, 0.25, 0.25}},  // top left: dim
    };
    Canvas canvas(kSize, kSize, kBlack);
    RenderObject(canvas, TestViewport(), kSquare, triangles, DrawShadedTriangle);
    CHECK(canvas.GetPixel(4, -4) == Rgb8{255, 255, 255});
    CHECK(canvas.GetPixel(-4, 4) == Rgb8{63, 63, 63});
    CHECK(canvas.GetPixel(8, -8) == Rgb8{255, 255, 255});  // a corner only the first one has
    CHECK(canvas.GetPixel(-8, 8) == Rgb8{63, 63, 63});     // and one only the second has
}

TEST_CASE("RenderObject: outlines and fills do not look at the intensities", "[rasterizer]") {
    const std::vector<Triangle> dim = {{{0, 1, 2}, kWhite, {0.0, 0.25, 0.5}}};
    const std::vector<Triangle> plain = {{{0, 1, 2}, kWhite}};
    CHECK(Bytes(CanvasWithObject(kSquare, dim)) == Bytes(CanvasWithObject(kSquare, plain)));

    Canvas filled_dim(kSize, kSize, kBlack);
    RenderObject(filled_dim, TestViewport(), kSquare, dim, DrawFilledTriangle);
    Canvas filled_plain(kSize, kSize, kBlack);
    RenderObject(filled_plain, TestViewport(), kSquare, plain, DrawFilledTriangle);
    CHECK(Bytes(filled_dim) == Bytes(filled_plain));
}

// --- With a depth buffer ---------------------------------------------------------------

namespace {

// Two triangles that both cover the middle of the canvas: a red one on the viewport
// plane, z = 1, with corners at the pixels (-8, -8), (8, -8) and (0, 8), and a blue one
// twice as far and twice as large in 3D, so that it lands on the same kind of pixels
// turned upside down: (-8, 8), (8, 8) and (0, -8).
const std::vector<Vec3> kNearAndFar = {
    {-0.25, -0.25, 1.0}, {0.25, -0.25, 1.0}, {0.0, 0.25, 1.0},  // 0, 1, 2: the near one
    {-0.5, 0.5, 2.0},    {0.5, 0.5, 2.0},    {0.0, -0.5, 2.0},  // 3, 4, 5: the far one
};
const Triangle kNearTriangle{{0, 1, 2}, kRed};
const Triangle kFarTriangle{{3, 4, 5}, kBlue};

}  // namespace

TEST_CASE("RenderObject: with a depth buffer the nearer triangle shows, whatever the list order",
          "[rasterizer]") {
    const auto render = [](const std::vector<Triangle>& triangles, bool depth_buffered) {
        Canvas canvas(kSize, kSize, kBlack);
        DepthBuffer depth_buffer(canvas);
        RenderObject(canvas, TestViewport(), kNearAndFar, triangles, DrawFilledTriangle,
                     depth_buffered ? &depth_buffer : nullptr);
        return canvas;
    };

    const Canvas near_first = render({kNearTriangle, kFarTriangle}, true);
    CHECK(Bytes(near_first) == Bytes(render({kFarTriangle, kNearTriangle}, true)));
    CHECK(near_first.GetPixel(0, 0) == Rgb8{255, 0, 0});   // both cover it: the near one
    CHECK(near_first.GetPixel(-6, 7) == Rgb8{0, 0, 255});  // only the far one is here

    // Without one the list order decides, as it always did.
    CHECK(render({kNearTriangle, kFarTriangle}, false).GetPixel(0, 0) == Rgb8{0, 0, 255});
    CHECK(Bytes(near_first) == Bytes(render({kFarTriangle, kNearTriangle}, false)));
}

TEST_CASE("RenderObject: the depths are those of the vertices, 1/z", "[rasterizer]") {
    Canvas canvas(kSize, kSize, kBlack);
    DepthBuffer depth_buffer(canvas);
    RenderObject(canvas, TestViewport(), kNearAndFar, std::vector<Triangle>{kFarTriangle},
                 DrawFilledTriangle, &depth_buffer);
    CHECK(depth_buffer.InvZ(0, 0) == 0.5);      // z = 2
    CHECK(depth_buffer.InvZ(-15, -15) == 0.0);  // nothing was drawn here
}

TEST_CASE("RenderObject: objects hide each other through the depth buffer they share",
          "[rasterizer]") {
    const std::vector<Triangle> near_object = {kNearTriangle};
    const std::vector<Triangle> far_object = {kFarTriangle};

    // One buffer for both calls: the far object, drawn second, stays behind the near one.
    Canvas shared(kSize, kSize, kBlack);
    DepthBuffer depth_buffer(shared);
    RenderObject(shared, TestViewport(), kNearAndFar, near_object, DrawFilledTriangle,
                 &depth_buffer);
    RenderObject(shared, TestViewport(), kNearAndFar, far_object, DrawFilledTriangle,
                 &depth_buffer);
    CHECK(shared.GetPixel(0, 0) == Rgb8{255, 0, 0});

    // A buffer each: neither object knows how deep the other is, and the later one shows.
    Canvas separate(kSize, kSize, kBlack);
    DepthBuffer first(separate);
    DepthBuffer second(separate);
    RenderObject(separate, TestViewport(), kNearAndFar, near_object, DrawFilledTriangle, &first);
    RenderObject(separate, TestViewport(), kNearAndFar, far_object, DrawFilledTriangle, &second);
    CHECK(separate.GetPixel(0, 0) == Rgb8{0, 0, 255});
}

TEST_CASE("RenderObject: a wireframe is drawn whole through a depth buffer", "[rasterizer]") {
    Canvas canvas(kSize, kSize, kBlack);
    DepthBuffer depth_buffer(canvas);
    RenderObject(canvas, TestViewport(), kSquare, std::vector<Triangle>{{{0, 1, 2}, kRed}},
                 DrawWireframeTriangle, &depth_buffer);
    CHECK(Bytes(canvas) == Bytes(CanvasWithObject(kSquare, {{{0, 1, 2}, kRed}})));
}

TEST_CASE("Triangle: a corner has no normal of its own unless the triangle states one",
          "[rasterizer]") {
    const Triangle triangle{{0, 1, 2}, kRed};
    CHECK(triangle.normals == std::array<Vec3, 3>{});
    const Triangle smooth{
        .v = {0, 1, 2},
        .color = kRed,
        .normals = {Vec3{1.0, 0.0, 0.0}, Vec3{0.0, 1.0, 0.0}, Vec3{0.0, 0.0, 1.0}}};
    CHECK_FALSE(triangle == smooth);
    CHECK(smooth.h == std::array<double, 3>{1.0, 1.0, 1.0});
}

TEST_CASE("RenderObject: a triangle primitive does not look at the normals", "[rasterizer]") {
    // The normals are for the drawing under lights (see shading_test.cpp); with a
    // primitive the picture is the same whatever they are.
    const std::vector<Vec3> vertices = {{0.25, 0.25, 1.0}, {-0.25, 0.0, 1.0}, {0.125, -0.25, 1.0}};
    const std::vector<Triangle> plain = {{{0, 1, 2}, kRed, {1.0, 0.25, 0.5}}};
    std::vector<Triangle> with_normals = plain;
    with_normals[0].normals = {Vec3{1.0, 0.0, 0.0}, Vec3{0.0, 1.0, 0.0}, Vec3{0.0, 0.0, 1.0}};
    for (const cgfs::DrawTriangleFunction draw :
         {DrawWireframeTriangle, DrawFilledTriangle, DrawShadedTriangle}) {
        Canvas expected(kSize, kSize, kBlack);
        RenderObject(expected, TestViewport(), vertices, plain, draw);
        Canvas canvas(kSize, kSize, kBlack);
        RenderObject(canvas, TestViewport(), vertices, with_normals, draw);
        CHECK(Bytes(canvas) == Bytes(expected));
        CHECK_FALSE(LitPixels(canvas).empty());
    }
}
