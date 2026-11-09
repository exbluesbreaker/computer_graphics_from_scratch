#include <cgfs/camera/viewport.h>
#include <cgfs/image/canvas.h>
#include <cgfs/image/color.h>
#include <cgfs/math/mat4.h>
#include <cgfs/math/vec3.h>
#include <cgfs/rasterizer/model.h>
#include <cgfs/rasterizer/object.h>
#include <cgfs/rasterizer/point.h>
#include <cgfs/rasterizer/triangle.h>

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <vector>

#include "rasterizer/pixel_helpers.h"
#include "support/test_helpers.h"

using cgfs::Canvas;
using cgfs::DrawWireframeTriangle;
using cgfs::MakeOYRotationMatrix;
using cgfs::MakeScalingMatrix;
using cgfs::MakeTranslationMatrix;
using cgfs::Mat4;
using cgfs::Model;
using cgfs::Point;
using cgfs::RenderModel;
using cgfs::RenderObject;
using cgfs::Rgb8;
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

// A fresh black canvas with the model rendered on it under `transform`.
Canvas CanvasWithModel(const Model& model, const Mat4& transform) {
    Canvas canvas(kSize, kSize, kBlack);
    RenderModel(canvas, TestViewport(), model, transform);
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

// A lopsided triangle in its own plane z = 0, so that a mirror image is a different
// picture. Moved to z = 1 its corners land on the pixels (8, 8), (-8, 0) and (4, -8).
const Model kTriangle{
    .vertices = {{0.25, 0.25, 0.0}, {-0.25, 0.0, 0.0}, {0.125, -0.25, 0.0}},
    .triangles = {{{0, 1, 2}, kWhite}},
};
const Mat4 kToViewportPlane = MakeTranslationMatrix({0.0, 0.0, 1.0});

}  // namespace

TEST_CASE("RenderModel: a translation gives RenderObject on the moved vertices", "[rasterizer]") {
    const Canvas canvas = CanvasWithModel(kTriangle, kToViewportPlane);
    CHECK(Bytes(canvas) == Bytes(CanvasWithOutlines({{{8, 8}, {-8, 0}, {4, -8}}})));

    const std::vector<Vec3> moved = {{0.25, 0.25, 1.0}, {-0.25, 0.0, 1.0}, {0.125, -0.25, 1.0}};
    Canvas by_hand(kSize, kSize, kBlack);
    RenderObject(by_hand, TestViewport(), moved, kTriangle.triangles);
    CHECK(Bytes(canvas) == Bytes(by_hand));
}

TEST_CASE("RenderModel: the transform is applied before the projection", "[rasterizer]") {
    // Twice as far without scaling: half the size.
    CHECK(Bytes(CanvasWithModel(kTriangle, MakeTranslationMatrix({0.0, 0.0, 2.0}))) ==
          Bytes(CanvasWithOutlines({{{4, 4}, {-4, 0}, {2, -4}}})));
    // Scaled by 2 and then moved twice as far: on the same rays, the same picture.
    CHECK(Bytes(CanvasWithModel(kTriangle,
                                MakeTranslationMatrix({0.0, 0.0, 2.0}) * MakeScalingMatrix(2.0))) ==
          Bytes(CanvasWithModel(kTriangle, kToViewportPlane)));
}

TEST_CASE("RenderModel: a half turn about Y mirrors the model in x", "[rasterizer]") {
    // cos 180 is exactly -1 and the model has z = 0, so the mirror image is exact.
    const Canvas canvas =
        CanvasWithModel(kTriangle, kToViewportPlane * MakeOYRotationMatrix(180.0));
    CHECK(Bytes(canvas) == Bytes(CanvasWithOutlines({{{-8, 8}, {8, 0}, {-4, -8}}})));
}

TEST_CASE("RenderModel: a quarter turn about Y shows the model edge-on", "[rasterizer]") {
    // The triangle's plane turns into the plane x = 0: every vertex projects onto the
    // column x = 0, at y = 0.25 / 1.25, 0 / 0.75 and -0.25 / 1.125 of the viewport.
    const Canvas canvas = CanvasWithModel(kTriangle, kToViewportPlane * MakeOYRotationMatrix(90.0));
    const std::vector<Point> lit = LitPixels(canvas);
    REQUIRE(lit.size() == 14);
    for (const Point p : lit) {
        CHECK(p.x == 0);
    }
    CHECK(lit.front().y == -7);
    CHECK(lit.back().y == 6);
}

TEST_CASE("RenderModel: a product of transforms is the chain of them", "[rasterizer]") {
    const Mat4 chained = MakeTranslationMatrix({0.125, 0.0, 0.0}) * kToViewportPlane;
    CHECK(Bytes(CanvasWithModel(kTriangle, chained)) ==
          Bytes(CanvasWithModel(kTriangle, MakeTranslationMatrix({0.125, 0.0, 1.0}))));
    CHECK(Bytes(CanvasWithModel(kTriangle, chained)) ==
          Bytes(CanvasWithOutlines({{{12, 8}, {-4, 0}, {8, -8}}})));
}

TEST_CASE("RenderModel: a model without triangles, or without vertices, draws nothing",
          "[rasterizer]") {
    CHECK(LitPixels(CanvasWithModel(Model{.vertices = kTriangle.vertices, .triangles = {}},
                                    kToViewportPlane))
              .empty());
    CHECK(LitPixels(CanvasWithModel(Model{}, kToViewportPlane)).empty());
}
