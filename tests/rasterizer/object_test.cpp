#include <cgfs/camera/viewport.h>
#include <cgfs/image/canvas.h>
#include <cgfs/image/color.h>
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
