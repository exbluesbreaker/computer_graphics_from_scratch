#include <cgfs/camera/camera.h>
#include <cgfs/camera/viewport.h>
#include <cgfs/image/canvas.h>
#include <cgfs/image/color.h>
#include <cgfs/math/mat4.h>
#include <cgfs/math/vec3.h>
#include <cgfs/rasterizer/instance.h>
#include <cgfs/rasterizer/model.h>
#include <cgfs/rasterizer/point.h>
#include <cgfs/rasterizer/scene.h>
#include <cgfs/rasterizer/triangle.h>

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "rasterizer/pixel_helpers.h"
#include "support/test_helpers.h"

using cgfs::Camera;
using cgfs::Canvas;
using cgfs::DrawWireframeTriangle;
using cgfs::Instance;
using cgfs::MakeOYRotationMatrix;
using cgfs::Model;
using cgfs::Point;
using cgfs::RenderInstance;
using cgfs::RenderScene;
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

// A fresh black canvas with the scene rendered on it, as `camera` sees it.
Canvas CanvasWithScene(const std::vector<Model>& models, const std::vector<Instance>& instances,
                       const Camera& camera = Camera{}) {
    Canvas canvas(kSize, kSize, kBlack);
    RenderScene(canvas, TestViewport(), models, instances, camera);
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

// A small triangle in its own coordinates: placed at (x, y, 1) its corners land 4 pixels
// from the pixel of (x, y), at (+4, +4), (-4, +4) and (0, -4) from it.
Model SmallTriangle(cgfs::Color color) {
    return {
        .vertices = {{0.125, 0.125, 0.0}, {-0.125, 0.125, 0.0}, {0.0, -0.125, 0.0}},
        .triangles = {{{0, 1, 2}, color}},
    };
}

// The same triangle with corners 0.14 from its center instead of 0.125: they land on the
// same pixels (0.14 * 32 = 4.48 truncates to 4) but not on a pixel boundary, so a turned
// camera's cos and sin, exact only to a few ulps, cannot move them by a pixel.
Model OffGridTriangle(cgfs::Color color) {
    return {
        .vertices = {{0.14, 0.14, 0.0}, {-0.14, 0.14, 0.0}, {0.0, -0.14, 0.0}},
        .triangles = {{{0, 1, 2}, color}},
    };
}

// The expected outline of SmallTriangle placed so that its center is the pixel (cx, cy).
Outline SmallOutline(int cx, int cy, cgfs::Color color = kWhite) {
    return {{cx + 4, cy + 4}, {cx - 4, cy + 4}, {cx, cy - 4}, color};
}

}  // namespace

TEST_CASE("RenderScene: every instance is drawn where its position puts it", "[rasterizer]") {
    // One model, three instances of it, far enough apart not to touch.
    const std::vector<Model> models = {SmallTriangle(kWhite)};
    const std::vector<Instance> instances = {
        {.model = 0, .position = {-0.25, 0.25, 1.0}},
        {.model = 0, .position = {0.0, -0.25, 1.0}},
        {.model = 0, .position = {0.25, 0.25, 1.0}},
    };
    const Canvas canvas = CanvasWithScene(models, instances);
    CHECK(Bytes(canvas) == Bytes(CanvasWithOutlines(
                               {SmallOutline(-8, 8), SmallOutline(0, -8), SmallOutline(8, 8)})));
    // Three separate triangles: as many pixels as the three drawn alone, one per canvas.
    std::size_t alone = 0;
    for (const Instance& instance : instances) {
        alone += LitPixels(CanvasWithScene(models, {instance})).size();
    }
    CHECK(LitPixels(canvas).size() == alone);

    // The same picture as the three RenderInstance calls.
    Canvas one_by_one(kSize, kSize, kBlack);
    for (const Instance& instance : instances) {
        RenderInstance(one_by_one, TestViewport(), instance, models);
    }
    CHECK(Bytes(canvas) == Bytes(one_by_one));
}

TEST_CASE("RenderScene: instances are drawn in order and the later one owns shared pixels",
          "[rasterizer]") {
    // Two models of the same shape in different colors, placed on top of each other.
    const std::vector<Model> models = {SmallTriangle(kRed), SmallTriangle(kBlue)};
    const Vec3 here{0.0, 0.0, 1.0};

    const Canvas red_then_blue =
        CanvasWithScene(models, {{.model = 0, .position = here}, {.model = 1, .position = here}});
    CHECK(Bytes(red_then_blue) == Bytes(CanvasWithOutlines({SmallOutline(0, 0, kBlue)})));
    CHECK(red_then_blue.GetPixel(0, -4) == Rgb8{0, 0, 255});

    const Canvas blue_then_red =
        CanvasWithScene(models, {{.model = 1, .position = here}, {.model = 0, .position = here}});
    CHECK(Bytes(blue_then_red) == Bytes(CanvasWithOutlines({SmallOutline(0, 0, kRed)})));
    CHECK(blue_then_red.GetPixel(0, -4) == Rgb8{255, 0, 0});
}

TEST_CASE("RenderScene: instances may use any of the models, in any order", "[rasterizer]") {
    const std::vector<Model> models = {SmallTriangle(kRed), SmallTriangle(kGreen)};
    const std::vector<Instance> instances = {
        {.model = 1, .position = {-0.25, 0.0, 1.0}},
        {.model = 0, .position = {0.25, 0.0, 1.0}},
        {.model = 1, .position = {0.0, 0.25, 1.0}},
    };
    CHECK(Bytes(CanvasWithScene(models, instances)) ==
          Bytes(CanvasWithOutlines({SmallOutline(-8, 0, kGreen), SmallOutline(8, 0, kRed),
                                    SmallOutline(0, 8, kGreen)})));
}

TEST_CASE("RenderScene: a scene without instances draws nothing", "[rasterizer]") {
    CHECK(LitPixels(CanvasWithScene({SmallTriangle(kWhite)}, {})).empty());
    CHECK(LitPixels(CanvasWithScene({}, {})).empty());
}

TEST_CASE("RenderScene: the model and instance lists may be any contiguous range", "[rasterizer]") {
    const std::array<Model, 1> models = {SmallTriangle(kWhite)};
    const Instance instances[] = {{.model = 0, .position = {0.0, 0.0, 1.0}}};
    Canvas canvas(kSize, kSize, kBlack);
    RenderScene(canvas, TestViewport(), models, instances);
    CHECK(Bytes(canvas) == Bytes(CanvasWithOutlines({SmallOutline(0, 0)})));
}

TEST_CASE("RenderScene: without a camera the scene is seen from the origin, the default camera",
          "[rasterizer]") {
    const std::vector<Model> models = {SmallTriangle(kWhite)};
    const std::vector<Instance> instances = {{.model = 0, .position = {0.25, 0.0, 1.0}}};
    CHECK(Bytes(CanvasWithScene(models, instances)) ==
          Bytes(CanvasWithScene(models, instances, Camera{})));
    CHECK(Bytes(CanvasWithScene(models, instances)) ==
          Bytes(CanvasWithOutlines({SmallOutline(8, 0)})));
}

TEST_CASE("RenderScene: moving the camera moves the picture the opposite way", "[rasterizer]") {
    const std::vector<Model> models = {SmallTriangle(kWhite)};
    const std::vector<Instance> instances = {{.model = 0, .position = {0.0, 0.0, 1.0}}};
    // The camera steps left: the triangle appears to the right.
    CHECK(Bytes(CanvasWithScene(models, instances, Camera{.position = {-0.25, 0.0, 0.0}})) ==
          Bytes(CanvasWithOutlines({SmallOutline(8, 0)})));
    // The camera rises: the triangle appears lower.
    CHECK(Bytes(CanvasWithScene(models, instances, Camera{.position = {0.0, 0.25, 0.0}})) ==
          Bytes(CanvasWithOutlines({SmallOutline(0, -8)})));
    // The camera steps back: the triangle is twice as far, half the size.
    CHECK(Bytes(CanvasWithScene(models, instances, Camera{.position = {0.0, 0.0, -1.0}})) ==
          Bytes(CanvasWithOutlines({{{2, 2}, {-2, 2}, {0, -2}}})));
}

TEST_CASE("RenderScene: a turned camera sees what stands in its new line of sight",
          "[rasterizer]") {
    // The triangle stands at (-1, 0, 0), beside the default camera rather than in front
    // of it (its vertices have z <= 0, which ProjectVertex rejects), turned to face a
    // camera that looks along -X. A camera turned 90 degrees to its left looks along -X
    // and sees it straight ahead, 1 away, as the default camera sees the same triangle
    // at (0, 0, 1).
    const std::vector<Model> models = {OffGridTriangle(kWhite)};
    const std::vector<Instance> instances = {
        {.model = 0, .position = {-1.0, 0.0, 0.0}, .orientation = MakeOYRotationMatrix(90.0)}};
    const Camera turned{.orientation = MakeOYRotationMatrix(90.0)};
    CHECK(Bytes(CanvasWithScene(models, instances, turned)) ==
          Bytes(CanvasWithOutlines({SmallOutline(0, 0)})));
}

TEST_CASE("RenderScene: the camera and the model transform combine into one matrix",
          "[rasterizer]") {
    // The book's demo camera, moved and turned. An instance turned the same way and
    // placed 1 ahead along the camera's line of sight looks exactly as the untransformed
    // instance at (0, 0, 1) looks to the default camera.
    const Camera camera{.position = {-3.0, 1.0, 2.0}, .orientation = MakeOYRotationMatrix(-30.0)};
    const std::vector<Model> models = {OffGridTriangle(kWhite)};
    const std::vector<Instance> ahead = {
        {.model = 0,
         .position = camera.position + camera.orientation * Vec3{0.0, 0.0, 1.0},
         .orientation = camera.orientation}};
    const std::vector<Instance> plain = {{.model = 0, .position = {0.0, 0.0, 1.0}}};
    CHECK(Bytes(CanvasWithScene(models, ahead, camera)) == Bytes(CanvasWithScene(models, plain)));
    CHECK(Bytes(CanvasWithScene(models, ahead, camera)) ==
          Bytes(CanvasWithOutlines({SmallOutline(0, 0)})));
}
