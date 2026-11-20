#include <cgfs/camera/camera.h>
#include <cgfs/camera/clipping_planes.h>
#include <cgfs/camera/viewport.h>
#include <cgfs/image/canvas.h>
#include <cgfs/image/color.h>
#include <cgfs/image/depth_buffer.h>
#include <cgfs/math/bounding_sphere.h>
#include <cgfs/math/mat4.h>
#include <cgfs/math/plane.h>
#include <cgfs/math/vec3.h>
#include <cgfs/rasterizer/culling.h>
#include <cgfs/rasterizer/instance.h>
#include <cgfs/rasterizer/model.h>
#include <cgfs/rasterizer/object.h>
#include <cgfs/rasterizer/point.h>
#include <cgfs/rasterizer/scene.h>
#include <cgfs/rasterizer/triangle.h>

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "rasterizer/pixel_helpers.h"
#include "support/test_helpers.h"

using cgfs::BackFaceCulling;
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
//
// Without back-face culling unless a test asks for it. The models of most tests here are
// single triangles, which are not closed objects and are looked at from whichever side
// the test puts its camera on; the tests of the culling itself come last and turn it on.
Canvas CanvasWithScene(const std::vector<Model>& models, const std::vector<Instance>& instances,
                       const Camera& camera = Camera{},
                       DrawTriangleFunction draw_triangle = DrawWireframeTriangle,
                       BackFaceCulling back_face_culling = BackFaceCulling::kOff) {
    Canvas canvas(kSize, kSize, kBlack);
    RenderScene(canvas, TestViewport(), models, instances, camera, draw_triangle, nullptr,
                back_face_culling);
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
    RenderScene(canvas, TestViewport(), models, instances, Camera{}, DrawWireframeTriangle, nullptr,
                BackFaceCulling::kOff);
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

// A fresh black canvas with the scene drawn filled, through a depth buffer of its own;
// without back-face culling unless a test asks for it, like CanvasWithScene.
Canvas CanvasWithDepthBufferedScene(const std::vector<Model>& models,
                                    const std::vector<Instance>& instances,
                                    const Camera& camera = Camera{},
                                    BackFaceCulling back_face_culling = BackFaceCulling::kOff) {
    Canvas canvas(kSize, kSize, kBlack);
    DepthBuffer depth_buffer(canvas);
    RenderScene(canvas, TestViewport(), models, instances, camera, DrawFilledTriangle,
                &depth_buffer, back_face_culling);
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

// --- Back-face culling -----------------------------------------------------------------

namespace {

// `model` with the corners of its one triangle listed the other way round: the same
// triangle in space, turned inside out. The intensities follow their corners.
Model TurnedInsideOut(Model model) {
    cgfs::Triangle& triangle = model.triangles[0];
    std::swap(triangle.v[1], triangle.v[2]);
    std::swap(triangle.h[1], triangle.h[2]);
    return model;
}

// SmallTriangle lists its corners top right, top left, bottom: counter-clockwise to a
// camera that looks at it along +Z, which therefore sees its back. This is the same
// triangle with its front to that camera, and its outline around the pixel (cx, cy).
Model FrontTriangle(cgfs::Color color) { return TurnedInsideOut(SmallTriangle(color)); }
Outline FrontOutline(int cx, int cy, cgfs::Color color = kWhite) {
    return {{cx + 4, cy + 4}, {cx, cy - 4}, {cx - 4, cy + 4}, color};
}

// The book's cube, corners at x, y, z = ±1, one color per face where the tests look:
// the back red, the right green, the front blue, the left yellow, the top and the
// bottom white. The front is the face at z = -1, toward a camera that looks along +Z.
Model Cube() {
    Model cube;
    cube.vertices = {{1.0, 1.0, 1.0},  {-1.0, 1.0, 1.0},  {-1.0, -1.0, 1.0},  {1.0, -1.0, 1.0},
                     {1.0, 1.0, -1.0}, {-1.0, 1.0, -1.0}, {-1.0, -1.0, -1.0}, {1.0, -1.0, -1.0}};
    cube.triangles = {
        {{0, 1, 2}, kRed},   {{0, 2, 3}, kRed},   {{4, 0, 3}, kGreen},  {{4, 3, 7}, kGreen},
        {{5, 4, 7}, kBlue},  {{5, 7, 6}, kBlue},  {{1, 5, 6}, kYellow}, {{1, 6, 2}, kYellow},
        {{4, 5, 1}, kWhite}, {{4, 1, 0}, kWhite}, {{2, 6, 7}, kWhite},  {{2, 7, 3}, kWhite},
    };
    cube.bounding_sphere = cgfs::MakeBoundingSphere(cube.vertices);
    return cube;
}

// The cube's front face alone: the two triangles of z = -1 on the same eight vertices.
Model FrontOfCube() {
    Model front = Cube();
    front.triangles = {front.triangles[4], front.triangles[5]};
    return front;
}

}  // namespace

TEST_CASE("RenderScene: a triangle turned away from the camera is not drawn, by default",
          "[rasterizer][culling]") {
    const std::vector<Model> models = {FrontTriangle(kWhite), SmallTriangle(kWhite)};
    const std::vector<Instance> front = {{.model = 0, .position = {0.0, 0.0, 1.0}}};
    const std::vector<Instance> back = {{.model = 1, .position = {0.0, 0.0, 1.0}}};

    // Listed clockwise as the camera sees it, the triangle is drawn; listed the other
    // way round, the same three corners leave the canvas empty.
    Canvas front_canvas(kSize, kSize, kBlack);
    RenderScene(front_canvas, TestViewport(), models, front);
    CHECK(Bytes(front_canvas) == Bytes(CanvasWithOutlines({FrontOutline(0, 0)})));
    Canvas back_canvas(kSize, kSize, kBlack);
    RenderScene(back_canvas, TestViewport(), models, back);
    CHECK(LitPixels(back_canvas).empty());

    // The default is BackFaceCulling::kOn.
    CHECK(Bytes(CanvasWithScene(models, front, Camera{}, DrawWireframeTriangle,
                                BackFaceCulling::kOn)) ==
          Bytes(CanvasWithOutlines({FrontOutline(0, 0)})));
    CHECK(LitPixels(
              CanvasWithScene(models, back, Camera{}, DrawWireframeTriangle, BackFaceCulling::kOn))
              .empty());

    // With the culling off both are drawn.
    CHECK(Bytes(CanvasWithScene(models, front, Camera{}, DrawWireframeTriangle,
                                BackFaceCulling::kOff)) ==
          Bytes(CanvasWithOutlines({FrontOutline(0, 0)})));
    CHECK(Bytes(CanvasWithScene(models, back, Camera{}, DrawWireframeTriangle,
                                BackFaceCulling::kOff)) ==
          Bytes(CanvasWithOutlines({SmallOutline(0, 0)})));
}

TEST_CASE("RenderScene: which side of a triangle the camera stands on decides",
          "[rasterizer][culling]") {
    // A triangle at z = 1 of the world with its front toward the origin, and the same
    // one inside out. The default camera sees the first. A camera on the far side, at
    // z = 2 and turned half around to look back at them, sees the second.
    const std::vector<Model> models = {TurnedInsideOut(OffGridTriangle(kWhite)),
                                       OffGridTriangle(kWhite)};
    const std::vector<Instance> facing_origin = {{.model = 0, .position = {0.0, 0.0, 1.0}}};
    const std::vector<Instance> facing_away = {{.model = 1, .position = {0.0, 0.0, 1.0}}};
    const Camera beyond{.position = {0.0, 0.0, 2.0}, .orientation = MakeOYRotationMatrix(180.0)};
    const auto culled = [&](const std::vector<Instance>& instances, const Camera& camera) {
        return CanvasWithScene(models, instances, camera, DrawWireframeTriangle,
                               BackFaceCulling::kOn);
    };

    CHECK_FALSE(LitPixels(culled(facing_origin, Camera{})).empty());
    CHECK(LitPixels(culled(facing_away, Camera{})).empty());
    CHECK(LitPixels(culled(facing_origin, beyond)).empty());
    CHECK_FALSE(LitPixels(culled(facing_away, beyond)).empty());

    // What is drawn is drawn as without the culling.
    CHECK(Bytes(culled(facing_origin, Camera{})) ==
          Bytes(CanvasWithScene(models, facing_origin, Camera{})));
    CHECK(Bytes(culled(facing_away, beyond)) ==
          Bytes(CanvasWithScene(models, facing_away, beyond)));
}

TEST_CASE("RenderScene: an instance's transform turns a face toward the camera or away",
          "[rasterizer][culling]") {
    const std::vector<Model> models = {TurnedInsideOut(OffGridTriangle(kWhite))};
    const auto culled = [&](const Instance& instance) {
        return CanvasWithScene(models, {instance}, Camera{}, DrawWireframeTriangle,
                               BackFaceCulling::kOn);
    };
    const Vec3 ahead{0.0, 0.0, 1.0};

    // As it stands the triangle faces the camera. Turned half around its own origin it
    // is in the same place and shows its back.
    CHECK_FALSE(LitPixels(culled({.model = 0, .position = ahead})).empty());
    CHECK(LitPixels(
              culled({.model = 0, .position = ahead, .orientation = MakeOYRotationMatrix(180.0)}))
              .empty());

    // A scale, larger or smaller, turns nothing: the front stays the front.
    CHECK_FALSE(LitPixels(culled({.model = 0, .position = ahead, .scale = 2.0})).empty());
    CHECK_FALSE(LitPixels(culled({.model = 0, .position = ahead, .scale = 0.5})).empty());

    // Neither does moving it across the picture, though the camera then sees it at an
    // angle: it is the direction to the camera that counts.
    CHECK_FALSE(LitPixels(culled({.model = 0, .position = {0.25, -0.25, 1.0}})).empty());
}

TEST_CASE("RenderScene: of a cube straight ahead only the front face is drawn",
          "[rasterizer][culling]") {
    // The cube 4 ahead on the camera's axis: its front face, at z = 3, covers the pixels
    // from -10 to 10, and its back face, at z = 5, those from -6 to 6. The four sides
    // run between the two and are all turned away from a camera on the axis.
    const std::vector<Model> models = {Cube(), FrontOfCube()};
    const std::vector<Instance> cube = {{.model = 0, .position = {0.0, 0.0, 4.0}}};
    const std::vector<Instance> front = {{.model = 1, .position = {0.0, 0.0, 4.0}}};

    // Painted in list order without the culling, the left face, listed after the front,
    // lies over it.
    const Canvas painted = CanvasWithScene(models, cube, Camera{}, DrawFilledTriangle);
    CHECK(painted.GetPixel(-8, 0) == Rgb8{255, 255, 0});

    // With it the picture is that of the front face alone, even painted in list order:
    // the front faces of a convex object do not overlap.
    const Canvas culled =
        CanvasWithScene(models, cube, Camera{}, DrawFilledTriangle, BackFaceCulling::kOn);
    CHECK(culled.GetPixel(-8, 0) == Rgb8{0, 0, 255});
    CHECK(culled.GetPixel(0, 0) == Rgb8{0, 0, 255});
    CHECK(Bytes(culled) == Bytes(CanvasWithScene(models, front, Camera{}, DrawFilledTriangle)));

    // Through a depth buffer too, where the back faces would have been drawn and hidden.
    CHECK(Bytes(CanvasWithDepthBufferedScene(models, cube, Camera{}, BackFaceCulling::kOn)) ==
          Bytes(CanvasWithDepthBufferedScene(models, front)));

    // An outline is culled like a filled face: the wireframe loses its far edges.
    CHECK(Bytes(CanvasWithScene(models, cube, Camera{}, DrawWireframeTriangle,
                                BackFaceCulling::kOn)) == Bytes(CanvasWithScene(models, front)));
    CHECK(LitPixels(CanvasWithScene(models, cube)).size() >
          LitPixels(CanvasWithScene(models, front)).size());
}

TEST_CASE("RenderScene: a cube seen from a corner shows three faces", "[rasterizer][culling]") {
    // The cube 6 ahead, to the camera's right and below it: the camera sees its front,
    // its left side and its top. Drawn filled through a depth buffer, the three faces
    // alone give the picture of the whole cube without its back faces.
    Model three_faces = Cube();
    three_faces.triangles = {three_faces.triangles[4], three_faces.triangles[5],   // front
                             three_faces.triangles[6], three_faces.triangles[7],   // left
                             three_faces.triangles[8], three_faces.triangles[9]};  // top
    const std::vector<Model> models = {Cube(), three_faces};
    const Vec3 position{1.5, -1.5, 6.0};

    const Canvas culled = CanvasWithDepthBufferedScene(models, {{.model = 0, .position = position}},
                                                       Camera{}, BackFaceCulling::kOn);
    CHECK(Bytes(culled) ==
          Bytes(CanvasWithDepthBufferedScene(models, {{.model = 1, .position = position}})));
    CHECK_FALSE(LitPixels(culled).empty());
}

TEST_CASE("RenderScene: the pieces of a clipped triangle are culled as the triangle is",
          "[rasterizer][culling]") {
    // RightTriangle shows the default camera its back. Cut by the plane x = 0, the two
    // pieces of the triangle are drawn when its front is to the camera and neither is
    // when its back is.
    const std::vector<Model> models = {TurnedInsideOut(RightTriangle()), RightTriangle()};
    const std::vector<Instance> front = {{.model = 0, .position = {0.0, 0.0, 1.0}}};
    const std::vector<Instance> back = {{.model = 1, .position = {0.0, 0.0, 1.0}}};
    const Camera camera{.clipping_planes = {kRightHalf}};

    const Canvas culled =
        CanvasWithScene(models, front, camera, DrawShadedTriangle, BackFaceCulling::kOn);
    CHECK_FALSE(LitPixels(culled).empty());
    CHECK(Bytes(culled) == Bytes(CanvasWithScene(models, front, camera, DrawShadedTriangle)));
    CHECK(LitPixels(CanvasWithScene(models, back, camera, DrawShadedTriangle, BackFaceCulling::kOn))
              .empty());
}

TEST_CASE("RenderScene: from inside an object every face is a back face", "[rasterizer][culling]") {
    // The camera at the center of the cube, with the clipping planes of its viewport.
    // Without the culling the inside of the cube fills the canvas. With it nothing is
    // drawn: all six faces are turned away from a camera inside, which is why a closed
    // object cut open by the near plane shows what is behind it and not its own inside.
    const std::vector<Model> models = {Cube()};
    const std::vector<Instance> around_the_camera = {{.model = 0}};
    const Camera camera{.clipping_planes = MakeClippingPlanes(TestViewport())};

    CHECK_FALSE(
        LitPixels(CanvasWithScene(models, around_the_camera, camera, DrawFilledTriangle)).empty());
    CHECK(LitPixels(CanvasWithScene(models, around_the_camera, camera, DrawFilledTriangle,
                                    BackFaceCulling::kOn))
              .empty());
}
