#include <cgfs/camera/viewport.h>
#include <cgfs/image/canvas.h>
#include <cgfs/image/color.h>
#include <cgfs/math/bounding_sphere.h>
#include <cgfs/math/mat4.h>
#include <cgfs/math/vec3.h>
#include <cgfs/rasterizer/model.h>
#include <cgfs/rasterizer/object.h>
#include <cgfs/rasterizer/point.h>
#include <cgfs/rasterizer/triangle.h>

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdint>
#include <vector>

#include "rasterizer/pixel_helpers.h"
#include "support/test_helpers.h"

using cgfs::BoundingSphere;
using cgfs::Canvas;
using cgfs::DrawWireframeTriangle;
using cgfs::kMatte;
using cgfs::Length;
using cgfs::MakeBoundingSphere;
using cgfs::MakeOYRotationMatrix;
using cgfs::MakeScalingMatrix;
using cgfs::MakeTranslationMatrix;
using cgfs::Mat4;
using cgfs::Model;
using cgfs::Point;
using cgfs::RenderModel;
using cgfs::RenderObject;
using cgfs::Rgb8;
using cgfs::TransformModel;
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

TEST_CASE("Model: the default bounding sphere contains everything", "[rasterizer]") {
    CHECK(std::isinf(kTriangle.bounding_sphere.radius));
    CHECK(kTriangle.bounding_sphere == BoundingSphere{});
}

TEST_CASE("TransformModel: every vertex is transformed and the triangles are kept",
          "[rasterizer]") {
    const Model moved = TransformModel(kTriangle, MakeTranslationMatrix({1.0, 2.0, 3.0}));
    CHECK(moved.vertices ==
          std::vector<Vec3>{{1.25, 2.25, 3.0}, {0.75, 2.0, 3.0}, {1.125, 1.75, 3.0}});
    CHECK(moved.triangles == kTriangle.triangles);

    // The identity changes nothing, and the model itself is never changed.
    CHECK(TransformModel(kTriangle, Mat4::Identity()) == kTriangle);
    CHECK(kTriangle.vertices[0] == Vec3{0.25, 0.25, 0.0});
}

TEST_CASE("TransformModel: the bounding sphere moves and grows with the vertices", "[rasterizer]") {
    Model model = kTriangle;
    model.bounding_sphere = {.center = {0.5, 0.0, 0.0}, .radius = 2.0};

    // A translation moves the center and leaves the radius alone.
    const Model moved = TransformModel(model, MakeTranslationMatrix({1.0, 2.0, 3.0}));
    CHECK(moved.bounding_sphere == BoundingSphere{.center = {1.5, 2.0, 3.0}, .radius = 2.0});

    // A scaling multiplies the radius, and the center's distance from the origin.
    const Model doubled = TransformModel(model, MakeScalingMatrix(2.0));
    CHECK(doubled.bounding_sphere == BoundingSphere{.center = {1.0, 0.0, 0.0}, .radius = 4.0});
    const Model shrunk = TransformModel(model, MakeScalingMatrix(0.75));
    CHECK(shrunk.bounding_sphere == BoundingSphere{.center = {0.375, 0.0, 0.0}, .radius = 1.5});

    // A rotation turns the center about the origin and keeps the radius.
    const Model turned = TransformModel(model, MakeOYRotationMatrix(90.0));
    CHECK(std::abs(turned.bounding_sphere.center.x) < 1e-12);
    CHECK(std::abs(turned.bounding_sphere.center.z - 0.5) < 1e-12);
    CHECK(std::abs(turned.bounding_sphere.radius - 2.0) < 1e-12);

    // All three at once, in the order of an instance's model matrix: the radius is
    // multiplied by the scale whatever the rotation.
    const Mat4 model_matrix = MakeTranslationMatrix({-3.0, 1.0, 7.0}) *
                              MakeOYRotationMatrix(195.0) * MakeScalingMatrix(0.75);
    const Model placed = TransformModel(model, model_matrix);
    CHECK(std::abs(placed.bounding_sphere.radius - 1.5) < 1e-12);
    const Vec3 center = model_matrix * Vec3{0.5, 0.0, 0.0};
    CHECK(placed.bounding_sphere.center == center);
}

TEST_CASE("TransformModel: the transformed sphere still contains the transformed vertices",
          "[rasterizer]") {
    Model model = kTriangle;
    model.bounding_sphere = MakeBoundingSphere(model.vertices);
    for (const Mat4& transform :
         {MakeTranslationMatrix({-3.0, 1.0, 7.0}) * MakeOYRotationMatrix(195.0) *
              MakeScalingMatrix(0.75),
          MakeOYRotationMatrix(-30.0) * MakeTranslationMatrix({3.0, -1.0, -2.0}) *
              MakeScalingMatrix(40.0),
          MakeScalingMatrix(-2.0)}) {
        const Model placed = TransformModel(model, transform);
        for (const Vec3 vertex : placed.vertices) {
            CHECK(Length(vertex - placed.bounding_sphere.center) <=
                  placed.bounding_sphere.radius * (1.0 + 1e-12));
        }
    }
}

TEST_CASE("TransformModel: the sphere that contains everything stays that", "[rasterizer]") {
    const Model placed = TransformModel(
        kTriangle, MakeTranslationMatrix({-3.0, 1.0, 7.0}) * MakeScalingMatrix(0.75));
    CHECK(std::isinf(placed.bounding_sphere.radius));
}

// --- What the drawing under lights reads of a model ------------------------------------

namespace {

// kTriangle with a normal at each corner, each along another axis.
Model TriangleWithNormals() {
    Model model = kTriangle;
    model.triangles[0].normals = {Vec3{1.0, 0.0, 0.0}, Vec3{0.0, 1.0, 0.0}, Vec3{0.0, 0.0, 1.0}};
    return model;
}

bool Near(Vec3 a, Vec3 b) {
    return std::abs(a.x - b.x) < 1e-12 && std::abs(a.y - b.y) < 1e-12 &&
           std::abs(a.z - b.z) < 1e-12;
}

}  // namespace

TEST_CASE("Model: matte unless it says otherwise", "[rasterizer]") {
    CHECK(kTriangle.specular == kMatte);
    Model shiny = kTriangle;
    shiny.specular = 50.0;
    CHECK_FALSE(shiny == kTriangle);
}

TEST_CASE("TransformModel: moving a model does not turn its normals", "[rasterizer]") {
    const Model model = TriangleWithNormals();
    const Model moved = TransformModel(model, MakeTranslationMatrix({1.0, 2.0, 3.0}));
    CHECK(moved.triangles == model.triangles);
    CHECK(moved.vertices[0] == Vec3{1.25, 2.25, 3.0});  // the vertices did move
}

TEST_CASE("TransformModel: the normals turn with the model", "[rasterizer]") {
    // A quarter turn about Y takes +X to +Z and +Z to -X, and leaves +Y.
    const Model model = TriangleWithNormals();
    const Model turned =
        TransformModel(model, MakeTranslationMatrix({1.0, 2.0, 3.0}) * MakeOYRotationMatrix(90.0));
    const auto& normals = turned.triangles[0].normals;
    CHECK(Near(normals[0], {0.0, 0.0, 1.0}));
    CHECK(Near(normals[1], {0.0, 1.0, 0.0}));
    CHECK(Near(normals[2], {-1.0, 0.0, 0.0}));
    // Everything else about the triangle is as it was.
    CHECK(turned.triangles[0].v == model.triangles[0].v);
    CHECK(turned.triangles[0].color == model.triangles[0].color);
    CHECK(turned.triangles[0].h == model.triangles[0].h);
}

TEST_CASE("TransformModel: the normals come out at unit length", "[rasterizer]") {
    // A scale stretches every direction and is taken out of the normals again.
    const Model model = TriangleWithNormals();
    const std::array<Vec3, 3> unit = {Vec3{1.0, 0.0, 0.0}, Vec3{0.0, 1.0, 0.0},
                                      Vec3{0.0, 0.0, 1.0}};
    CHECK(TransformModel(model, MakeScalingMatrix(0.5)).triangles[0].normals == unit);
    CHECK(TransformModel(model, MakeScalingMatrix(3.0)).triangles[0].normals == unit);

    // So is whatever length the model stated them at, even under the identity.
    Model stated = kTriangle;
    stated.triangles[0].normals = {Vec3{4.0, 0.0, 0.0}, Vec3{0.0, 0.25, 0.0}, Vec3{0.0, 3.0, -4.0}};
    const Model as_it_stands = TransformModel(stated, Mat4::Identity());
    const auto& normals = as_it_stands.triangles[0].normals;
    CHECK(normals[0] == Vec3{1.0, 0.0, 0.0});
    CHECK(normals[1] == Vec3{0.0, 1.0, 0.0});
    CHECK(Near(normals[2], {0.0, 0.6, -0.8}));

    // Turned, scaled and moved at once: still unit vectors.
    const Model placed =
        TransformModel(stated, MakeTranslationMatrix({1.0, 2.0, 3.0}) * MakeOYRotationMatrix(35.0) *
                                   MakeScalingMatrix(0.75));
    for (const Vec3 normal : placed.triangles[0].normals) {
        CHECK(std::abs(Length(normal) - 1.0) < 1e-12);
    }
}

TEST_CASE("TransformModel: a corner without a normal stays without one", "[rasterizer]") {
    const Model transformed =
        TransformModel(kTriangle, MakeTranslationMatrix({1.0, 2.0, 3.0}) *
                                      MakeOYRotationMatrix(35.0) * MakeScalingMatrix(0.75));
    CHECK(transformed.triangles[0].normals == std::array<Vec3, 3>{});
}

TEST_CASE("TransformModel: the specular exponent is the model's", "[rasterizer]") {
    Model shiny = kTriangle;
    shiny.specular = 50.0;
    CHECK(TransformModel(shiny, MakeTranslationMatrix({1.0, 2.0, 3.0})).specular == 50.0);
    CHECK(TransformModel(kTriangle, MakeScalingMatrix(2.0)).specular == kMatte);
}
