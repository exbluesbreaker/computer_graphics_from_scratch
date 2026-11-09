#include <cgfs/camera/viewport.h>
#include <cgfs/image/canvas.h>
#include <cgfs/image/color.h>
#include <cgfs/math/mat4.h>
#include <cgfs/math/vec3.h>
#include <cgfs/rasterizer/instance.h>
#include <cgfs/rasterizer/model.h>
#include <cgfs/rasterizer/object.h>
#include <cgfs/rasterizer/point.h>
#include <cgfs/rasterizer/triangle.h>

#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdint>
#include <vector>

#include "rasterizer/pixel_helpers.h"
#include "support/test_helpers.h"

using cgfs::Canvas;
using cgfs::DrawWireframeTriangle;
using cgfs::Instance;
using cgfs::MakeModelMatrix;
using cgfs::MakeOYRotationMatrix;
using cgfs::MakeScalingMatrix;
using cgfs::MakeTranslationMatrix;
using cgfs::Mat4;
using cgfs::Model;
using cgfs::Point;
using cgfs::RenderInstance;
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

// A fresh black canvas with the instance rendered on it.
Canvas CanvasWithInstance(const Instance& instance, const std::vector<Model>& models) {
    Canvas canvas(kSize, kSize, kBlack);
    RenderInstance(canvas, TestViewport(), instance, models);
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

// A triangle in its own coordinates, in the model's plane z = 0. Placed at (0, 0, 1) it
// lies on the viewport plane and its corners land on the pixels (8, 8), (-8, 8), (0, -8).
const Model kTriangle{
    .vertices = {{0.25, 0.25, 0.0}, {-0.25, 0.25, 0.0}, {0.0, -0.25, 0.0}},
    .triangles = {{{0, 1, 2}, kWhite}},
};

// A lopsided triangle, so that its mirror image is a different picture: placed at
// (0, 0, 1) its corners land on (8, 8), (-8, 0) and (4, -8).
const Model kLopsided{
    .vertices = {{0.25, 0.25, 0.0}, {-0.25, 0.0, 0.0}, {0.125, -0.25, 0.0}},
    .triangles = {{{0, 1, 2}, kWhite}},
};

constexpr double kEpsilon = 1e-12;

bool Near(Vec3 a, Vec3 b) {
    return std::abs(a.x - b.x) < kEpsilon && std::abs(a.y - b.y) < kEpsilon &&
           std::abs(a.z - b.z) < kEpsilon;
}

}  // namespace

TEST_CASE("RenderInstance: the vertices are moved by the position and drawn like RenderObject",
          "[rasterizer]") {
    const std::vector<Model> models = {kTriangle};
    const Canvas canvas = CanvasWithInstance({.model = 0, .position = {0.0, 0.0, 1.0}}, models);
    CHECK(Bytes(canvas) == Bytes(CanvasWithOutlines({{{8, 8}, {-8, 8}, {0, -8}}})));

    // The same picture as RenderObject on the vertices moved by hand.
    const std::vector<Vec3> moved = {{0.25, 0.25, 1.0}, {-0.25, 0.25, 1.0}, {0.0, -0.25, 1.0}};
    Canvas by_hand(kSize, kSize, kBlack);
    RenderObject(by_hand, TestViewport(), moved, kTriangle.triangles);
    CHECK(Bytes(canvas) == Bytes(by_hand));
}

TEST_CASE("RenderInstance: a position along x or y shifts the picture, along z shrinks it",
          "[rasterizer]") {
    const std::vector<Model> models = {kTriangle};
    // Right by 0.125 on the viewport plane: 4 pixels.
    CHECK(Bytes(CanvasWithInstance({.model = 0, .position = {0.125, 0.0, 1.0}}, models)) ==
          Bytes(CanvasWithOutlines({{{12, 8}, {-4, 8}, {4, -8}}})));
    // Down by 0.125: 4 pixels.
    CHECK(Bytes(CanvasWithInstance({.model = 0, .position = {0.0, -0.125, 1.0}}, models)) ==
          Bytes(CanvasWithOutlines({{{8, 4}, {-8, 4}, {0, -12}}})));
    // Twice as far from the camera: half the size.
    CHECK(Bytes(CanvasWithInstance({.model = 0, .position = {0.0, 0.0, 2.0}}, models)) ==
          Bytes(CanvasWithOutlines({{{4, 4}, {-4, 4}, {0, -4}}})));
    // All at once: the corners move to (0.75, 0.5, 2), (0.25, 0.5, 2) and (0.5, 0, 2).
    CHECK(Bytes(CanvasWithInstance({.model = 0, .position = {0.5, 0.25, 2.0}}, models)) ==
          Bytes(CanvasWithOutlines({{{12, 8}, {4, 8}, {8, 0}}})));
}

TEST_CASE("RenderInstance: an instance names its model by index", "[rasterizer]") {
    // Two models: the triangle, and a red one half its size.
    const Model small{
        .vertices = {{0.125, 0.125, 0.0}, {-0.125, 0.125, 0.0}, {0.0, -0.125, 0.0}},
        .triangles = {{{0, 1, 2}, kRed}},
    };
    const std::vector<Model> models = {kTriangle, small};
    const Vec3 here{0.0, 0.0, 1.0};

    const Canvas first = CanvasWithInstance({.model = 0, .position = here}, models);
    CHECK(Bytes(first) == Bytes(CanvasWithOutlines({{{8, 8}, {-8, 8}, {0, -8}}})));
    CHECK(first.GetPixel(0, -8) == Rgb8{255, 255, 255});

    const Canvas second = CanvasWithInstance({.model = 1, .position = here}, models);
    CHECK(Bytes(second) == Bytes(CanvasWithOutlines({{{4, 4}, {-4, 4}, {0, -4}, kRed}})));
    CHECK(second.GetPixel(0, -4) == Rgb8{255, 0, 0});
    CHECK(second.GetPixel(0, -8) == Rgb8{0, 0, 0});
}

TEST_CASE("RenderInstance: the model is unchanged and can be drawn again elsewhere",
          "[rasterizer]") {
    const std::vector<Model> models = {kTriangle};
    Canvas canvas(kSize, kSize, kBlack);
    RenderInstance(canvas, TestViewport(), {.model = 0, .position = {-0.125, 0.0, 1.0}}, models);
    RenderInstance(canvas, TestViewport(), {.model = 0, .position = {0.125, 0.0, 1.0}}, models);
    CHECK(models[0].vertices == kTriangle.vertices);
    CHECK(Bytes(canvas) ==
          Bytes(CanvasWithOutlines({{{4, 8}, {-12, 8}, {-4, -8}}, {{12, 8}, {-4, 8}, {4, -8}}})));
}

TEST_CASE("RenderInstance: a model without triangles, or without vertices, draws nothing",
          "[rasterizer]") {
    const std::vector<Model> models = {Model{.vertices = kTriangle.vertices, .triangles = {}},
                                       Model{}};
    CHECK(LitPixels(CanvasWithInstance({.model = 0, .position = {0.0, 0.0, 1.0}}, models)).empty());
    CHECK(LitPixels(CanvasWithInstance({.model = 1, .position = {0.0, 0.0, 1.0}}, models)).empty());
}

TEST_CASE("MakeModelMatrix: with the default orientation and scale it is the translation",
          "[rasterizer]") {
    const Instance instance{.model = 0, .position = {1.0, 2.0, 3.0}};
    CHECK(MakeModelMatrix(instance) == MakeTranslationMatrix({1.0, 2.0, 3.0}));
    CHECK(MakeModelMatrix(instance) * Vec3{0.25, -0.25, 0.0} == Vec3{1.25, 1.75, 3.0});
}

TEST_CASE("MakeModelMatrix: scales first, then rotates, then translates", "[rasterizer]") {
    const Instance instance{.model = 0,
                            .position = {1.0, 2.0, 3.0},
                            .orientation = MakeOYRotationMatrix(90.0),
                            .scale = 2.0};
    const Mat4 model_matrix = MakeModelMatrix(instance);
    // (1, 0, 0) is scaled to (2, 0, 0), turned to (0, 0, 2) and moved to (1, 2, 5).
    CHECK(Near(model_matrix * Vec3{1.0, 0.0, 0.0}, {1.0, 2.0, 5.0}));
    // The model's origin is only moved: the scaling and the rotation happen about it.
    CHECK(Near(model_matrix * Vec3{}, {1.0, 2.0, 3.0}));
    CHECK(model_matrix == MakeTranslationMatrix(instance.position) * instance.orientation *
                              MakeScalingMatrix(instance.scale));
}

TEST_CASE("RenderInstance: the scale grows or shrinks the model about its own origin",
          "[rasterizer]") {
    const std::vector<Model> models = {kTriangle};
    // Half the size, in place.
    CHECK(Bytes(CanvasWithInstance({.model = 0, .position = {0.0, 0.0, 1.0}, .scale = 0.5},
                                   models)) ==
          Bytes(CanvasWithOutlines({{{4, 4}, {-4, 4}, {0, -4}}})));
    // Twice the size twice as far: on the same rays, the same picture.
    CHECK(Bytes(CanvasWithInstance({.model = 0, .position = {0.0, 0.0, 2.0}, .scale = 2.0},
                                   models)) ==
          Bytes(CanvasWithInstance({.model = 0, .position = {0.0, 0.0, 1.0}}, models)));
}

TEST_CASE("RenderInstance: the orientation turns the model about its own origin", "[rasterizer]") {
    const std::vector<Model> models = {kLopsided};
    const Canvas upright = CanvasWithInstance({.model = 0, .position = {0.0, 0.0, 1.0}}, models);
    CHECK(Bytes(upright) == Bytes(CanvasWithOutlines({{{8, 8}, {-8, 0}, {4, -8}}})));
    // A half turn about Y mirrors it in x; its origin, and so its place, does not move.
    const Canvas turned = CanvasWithInstance(
        {.model = 0, .position = {0.0, 0.0, 1.0}, .orientation = MakeOYRotationMatrix(180.0)},
        models);
    CHECK(Bytes(turned) == Bytes(CanvasWithOutlines({{{-8, 8}, {8, 0}, {-4, -8}}})));
}
