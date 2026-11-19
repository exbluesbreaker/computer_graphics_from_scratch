#include <cgfs/camera/camera.h>
#include <cgfs/camera/clipping_planes.h>
#include <cgfs/camera/viewport.h>
#include <cgfs/image/canvas.h>
#include <cgfs/image/color.h>
#include <cgfs/image/depth_buffer.h>
#include <cgfs/math/mat4.h>
#include <cgfs/math/plane.h>
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
using cgfs::DepthBuffer;
using cgfs::DrawFilledTriangle;
using cgfs::DrawShadedTriangle;
using cgfs::DrawTriangleFunction;
using cgfs::DrawWireframeTriangle;
using cgfs::Instance;
using cgfs::MakeClippingPlanes;
using cgfs::MakeOYRotationMatrix;
using cgfs::Model;
using cgfs::Plane;
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
                       const Camera& camera = Camera{},
                       DrawTriangleFunction draw_triangle = DrawWireframeTriangle) {
    Canvas canvas(kSize, kSize, kBlack);
    RenderScene(canvas, TestViewport(), models, instances, camera, draw_triangle);
    return canvas;
}

// The near plane of TestViewport(), z = 1 in the camera's coordinates, and the plane
// x = 0 with +X in front. Their normals are axes, so cutting a triangle whose corners
// are small binary fractions is exact, and the crossing points land on whole pixels.
constexpr Plane kNear{.normal = {0.0, 0.0, 1.0}, .d = -1.0};
constexpr Plane kRightHalf{.normal = {1.0, 0.0, 0.0}, .d = 0.0};

// A right triangle in its own plane z = 0 whose intensity grows from 0 at its left
// corner to 1 on its right side. Placed at (0, 0, 1) its corners land on the pixels
// (-8, -8), (8, -8) and (8, 0), and the plane x = 0 cuts it through (0, -8) and (0, -4).
Model RightTriangle() {
    return {
        .vertices = {{-0.25, -0.25, 0.0}, {0.25, -0.25, 0.0}, {0.25, 0.0, 0.0}},
        .triangles = {{{0, 1, 2}, kWhite, {0.0, 1.0, 1.0}}},
    };
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

TEST_CASE("RenderScene: draws with the primitive it is given", "[rasterizer]") {
    const std::vector<Model> models = {SmallTriangle(kRed)};
    const std::vector<Instance> instances = {{.model = 0, .position = {0.25, 0.0, 1.0}}};
    const Outline o = SmallOutline(8, 0, kRed);

    // An outline unless it is told otherwise.
    CHECK(Bytes(CanvasWithScene(models, instances, Camera{}, DrawWireframeTriangle)) ==
          Bytes(CanvasWithScene(models, instances)));

    Canvas filled(kSize, kSize, kBlack);
    DrawFilledTriangle(filled, o.a, o.b, o.c, o.color);
    CHECK(Bytes(CanvasWithScene(models, instances, Camera{}, DrawFilledTriangle)) == Bytes(filled));

    // Shaded, the intensities come from the corners of the model's triangles.
    const std::vector<Model> shaded_models = {RightTriangle()};
    const std::vector<Instance> at_the_viewport = {{.model = 0, .position = {0.0, 0.0, 1.0}}};
    Canvas shaded(kSize, kSize, kBlack);
    DrawShadedTriangle(shaded, {-8, -8, 0.0}, {8, -8, 1.0}, {8, 0, 1.0}, kWhite);
    CHECK(Bytes(CanvasWithScene(shaded_models, at_the_viewport, Camera{}, DrawShadedTriangle)) ==
          Bytes(shaded));
}

TEST_CASE("RenderScene: an instance in front of every clipping plane is drawn as without them",
          "[rasterizer]") {
    const std::vector<Model> models = {SmallTriangle(kWhite)};
    const std::vector<Instance> instances = {
        {.model = 0, .position = {-0.25, 0.25, 2.0}},
        {.model = 0, .position = {0.5, -0.25, 2.0}},
    };
    const Camera clipping{.clipping_planes = MakeClippingPlanes(TestViewport())};
    const Canvas canvas = CanvasWithScene(models, instances, clipping);
    CHECK(Bytes(canvas) == Bytes(CanvasWithScene(models, instances)));
    CHECK(LitPixels(canvas).size() > 20);
}

TEST_CASE("RenderScene: an instance outside the clipping volume is not drawn", "[rasterizer]") {
    // Behind the camera, where ProjectVertex cannot go at all, and off to each side,
    // where it would only draw beyond the canvas.
    const std::vector<Model> models = {SmallTriangle(kWhite)};
    const std::vector<Instance> instances = {
        {.model = 0, .position = {0.0, 0.0, -5.0}}, {.model = 0, .position = {0.0, 0.0, 0.0}},
        {.model = 0, .position = {-9.0, 0.0, 4.0}}, {.model = 0, .position = {9.0, 0.0, 4.0}},
        {.model = 0, .position = {0.0, 9.0, 4.0}},  {.model = 0, .position = {0.0, -9.0, 4.0}},
    };
    const Camera clipping{.clipping_planes = MakeClippingPlanes(TestViewport())};
    CHECK(LitPixels(CanvasWithScene(models, instances, clipping)).empty());

    // With one more instance, in view, the picture is that instance alone.
    std::vector<Instance> with_one_in_view = instances;
    with_one_in_view.push_back({.model = 0, .position = {0.0, 0.0, 2.0}});
    CHECK(Bytes(CanvasWithScene(models, with_one_in_view, clipping)) ==
          Bytes(CanvasWithOutlines({{{2, 2}, {-2, 2}, {0, -2}}})));
}

TEST_CASE("RenderScene: an instance that reaches behind the camera is cut at the near plane",
          "[rasterizer]") {
    // One corner 3 in front of the camera and two 1 behind it: the sides cross the plane
    // z = 1 half way, at (0.125, 0, 1) and (-0.125, 0, 1). What is drawn is the tip in
    // front: its corner projects to (0, 0.25 / 3) of the viewport, the pixel (0, 2).
    const std::vector<Model> models = {{
        .vertices = {{0.0, 0.25, 3.0}, {0.25, -0.25, -1.0}, {-0.25, -0.25, -1.0}},
        .triangles = {{{0, 1, 2}, kWhite}},
    }};
    const std::vector<Instance> instances = {{.model = 0}};
    const Camera clipping{.clipping_planes = {kNear}};
    CHECK(Bytes(CanvasWithScene(models, instances, clipping)) ==
          Bytes(CanvasWithOutlines({{{0, 2}, {4, 0}, {-4, 0}}})));
}

TEST_CASE("RenderScene: a wireframe cut by a plane is the outline of the pieces", "[rasterizer]") {
    // The plane x = 0 keeps the right part of the triangle, a quadrilateral, which
    // ClipTriangle returns as two triangles: the cut shows as a new edge along the
    // plane, and the diagonal between the two pieces shows too.
    const std::vector<Model> models = {RightTriangle()};
    const std::vector<Instance> instances = {{.model = 0, .position = {0.0, 0.0, 1.0}}};
    const Camera clipping{.clipping_planes = {kRightHalf}};
    const Canvas canvas = CanvasWithScene(models, instances, clipping);
    CHECK(Bytes(canvas) ==
          Bytes(CanvasWithOutlines({{{8, -8}, {8, 0}, {0, -8}}, {{0, -8}, {8, 0}, {0, -4}}})));
    for (const Point p : LitPixels(canvas)) {
        CHECK(p.x >= 0);
    }
}

TEST_CASE(
    "RenderScene: a shaded triangle cut by a plane is the part of the whole one in front "
    "of it",
    "[rasterizer]") {
    // The crossing points take their intensity from the same fraction of the side as
    // their position, so the shading of what is left does not change: pixel for pixel
    // the cut picture is the whole one with everything left of x = 0 removed. (Exactly
    // so here, where every edge of the pieces passes through whole pixels.)
    const std::vector<Model> models = {RightTriangle()};
    const std::vector<Instance> instances = {{.model = 0, .position = {0.0, 0.0, 1.0}}};
    const Canvas whole = CanvasWithScene(models, instances, Camera{}, DrawShadedTriangle);
    const Canvas cut = CanvasWithScene(models, instances, Camera{.clipping_planes = {kRightHalf}},
                                       DrawShadedTriangle);

    std::size_t kept = 0;
    std::size_t removed = 0;
    for (int x = -kSize / 2; x < kSize / 2; ++x) {
        for (int y = -kSize / 2; y < kSize / 2; ++y) {
            if (x >= 0) {
                CHECK(cut.GetPixel(x, y) == whole.GetPixel(x, y));
                if (cut.GetPixel(x, y) != Rgb8{0, 0, 0}) {
                    ++kept;
                }
            } else {
                CHECK(cut.GetPixel(x, y) == Rgb8{0, 0, 0});
                if (whole.GetPixel(x, y) != Rgb8{0, 0, 0}) {
                    ++removed;
                }
            }
        }
    }
    CHECK(kept > 30);     // something is left,
    CHECK(removed > 10);  // and something was cut away
    // The intensity at the cut is the one the whole triangle has there: half way from 0
    // at x = -8 to 1 at x = 8.
    CHECK(cut.GetPixel(0, -8) == Rgb8{127, 127, 127});
    CHECK(cut.GetPixel(8, -8) == Rgb8{255, 255, 255});
}

TEST_CASE("RenderScene: the clipping planes are in the camera's coordinates", "[rasterizer]") {
    // Two instances 2 away from the origin, one along -X and one along +X, each turned
    // to face a camera that looks along -X. To that camera, turned 90 degrees to its
    // left, the first is 2 ahead and the second 2 behind, whatever the world's axes say:
    // the near plane goes with the camera, and only the first is drawn, half its size.
    const std::vector<Model> models = {OffGridTriangle(kWhite)};
    const std::vector<Instance> instances = {
        {.model = 0, .position = {-2.0, 0.0, 0.0}, .orientation = MakeOYRotationMatrix(90.0)},
        {.model = 0, .position = {2.0, 0.0, 0.0}, .orientation = MakeOYRotationMatrix(90.0)},
    };
    const Camera turned{.orientation = MakeOYRotationMatrix(90.0), .clipping_planes = {kNear}};
    CHECK(Bytes(CanvasWithScene(models, instances, turned)) ==
          Bytes(CanvasWithOutlines({{{2, 2}, {-2, 2}, {0, -2}}})));

    // A camera that steps forward leaves an instance behind its near plane.
    const std::vector<Model> plain_models = {SmallTriangle(kWhite)};
    const std::vector<Instance> ahead = {{.model = 0, .position = {0.0, 0.0, 2.0}}};
    CHECK_FALSE(LitPixels(CanvasWithScene(plain_models, ahead, Camera{.clipping_planes = {kNear}}))
                    .empty());
    CHECK(
        LitPixels(CanvasWithScene(plain_models, ahead,
                                  Camera{.position = {0.0, 0.0, 1.5}, .clipping_planes = {kNear}}))
            .empty());
}

// --- With a depth buffer ---------------------------------------------------------------

namespace {

// A fresh black canvas with the scene drawn filled, through a depth buffer of its own.
Canvas CanvasWithDepthBufferedScene(const std::vector<Model>& models,
                                    const std::vector<Instance>& instances,
                                    const Camera& camera = Camera{}) {
    Canvas canvas(kSize, kSize, kBlack);
    DepthBuffer depth_buffer(canvas);
    RenderScene(canvas, TestViewport(), models, instances, camera, DrawFilledTriangle,
                &depth_buffer);
    return canvas;
}

}  // namespace

TEST_CASE("RenderScene: with a depth buffer the nearer instance shows, whatever the list order",
          "[rasterizer]") {
    // The same triangle in two colors, one behind the other on the camera's axis: the
    // red one on the viewport plane, the blue one twice as far, half the size on the
    // canvas and wholly behind the red one.
    const std::vector<Model> models = {SmallTriangle(kRed), SmallTriangle(kBlue)};
    const Instance near{.model = 0, .position = {0.0, 0.0, 1.0}};
    const Instance far{.model = 1, .position = {0.0, 0.0, 2.0}};

    const Canvas near_first = CanvasWithDepthBufferedScene(models, {near, far});
    CHECK(Bytes(near_first) == Bytes(CanvasWithDepthBufferedScene(models, {far, near})));
    CHECK(near_first.GetPixel(0, 0) == Rgb8{255, 0, 0});
    // Nothing of the blue one is seen: the picture is that of the red one alone.
    CHECK(Bytes(near_first) == Bytes(CanvasWithDepthBufferedScene(models, {near})));

    // Without a depth buffer the later instance owns the pixels, as before.
    const Canvas painted = CanvasWithScene(models, {near, far}, Camera{}, DrawFilledTriangle);
    CHECK(painted.GetPixel(0, 0) == Rgb8{0, 0, 255});
}

TEST_CASE(
    "RenderScene: the depth is the distance in front of the camera, not a place in the "
    "world",
    "[rasterizer]") {
    // A camera turned to look back along -Z: of two triangles at z = -1 and z = -2 of
    // the world the first is the nearer, though its z is the larger.
    const std::vector<Model> models = {OffGridTriangle(kRed), OffGridTriangle(kBlue)};
    const Instance near{.model = 0, .position = {0.0, 0.0, -1.0}};
    const Instance far{.model = 1, .position = {0.0, 0.0, -2.0}};
    const Camera looking_back{.orientation = MakeOYRotationMatrix(180.0)};

    const Canvas near_first = CanvasWithDepthBufferedScene(models, {near, far}, looking_back);
    CHECK(Bytes(near_first) ==
          Bytes(CanvasWithDepthBufferedScene(models, {far, near}, looking_back)));
    CHECK(near_first.GetPixel(0, 0) == Rgb8{255, 0, 0});
    CHECK(Bytes(near_first) == Bytes(CanvasWithDepthBufferedScene(models, {near}, looking_back)));
}

TEST_CASE("RenderScene: what a clipping plane leaves of a triangle keeps its depths",
          "[rasterizer]") {
    // A white triangle on the viewport plane, cut by the plane x = 0, in front of a red
    // one at twice the distance that the plane leaves whole on its right. Where the
    // white one is left it hides the red one, in either order.
    const std::vector<Model> models = {RightTriangle(), SmallTriangle(kRed)};
    const Instance cut{.model = 0, .position = {0.0, 0.0, 1.0}};
    const Instance behind{.model = 1, .position = {0.5, -0.375, 2.0}};
    const Camera camera{.clipping_planes = {kRightHalf}};

    const Canvas cut_first = CanvasWithDepthBufferedScene(models, {cut, behind}, camera);
    CHECK(Bytes(cut_first) == Bytes(CanvasWithDepthBufferedScene(models, {behind, cut}, camera)));
    CHECK(cut_first.GetPixel(7, -7) == Rgb8{255, 255, 255});  // inside both: the nearer, white
    CHECK(cut_first.GetPixel(-4, -7) == Rgb8{0, 0, 0});       // cut away, and nothing behind it
}
