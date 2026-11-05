#include <cgfs/rasterizer/projection.h>

#include <cgfs/camera/viewport.h>
#include <cgfs/math/vec3.h>
#include <cgfs/rasterizer/point.h>

#include <catch2/catch_test_macros.hpp>

#include "support/test_helpers.h"

using cgfs::Point;
using cgfs::ProjectVertex;
using cgfs::Vec3;
using cgfs::Viewport;
using cgfs::ViewportToCanvas;

TEST_CASE("ViewportToCanvas: the default viewport scales by the canvas size", "[rasterizer]") {
    // The CanvasToViewport table of the viewport tests, the other way round.
    const Viewport viewport(1920, 1080);  // 1 x 1 at distance 1
    CHECK(ViewportToCanvas(viewport, 0.0, 0.0) == Point{0, 0});
    CHECK(ViewportToCanvas(viewport, 0.5, 0.5) == Point{960, 540});      // top-right
    CHECK(ViewportToCanvas(viewport, -0.5, -0.5) == Point{-960, -540});  // bottom-left
    CHECK(ViewportToCanvas(viewport, 0.25, -0.25) == Point{480, -270});  // quarter
}

TEST_CASE("ViewportToCanvas: each axis scales by its own ratio", "[rasterizer]") {
    // A 2 x 1 viewport at distance 1 on a 320 x 180 canvas.
    const Viewport viewport(320, 180, 2.0, 1.0);
    CHECK(ViewportToCanvas(viewport, 1.0, 0.5) == Point{160, 90});  // the top-right corner
    CHECK(ViewportToCanvas(viewport, 0.5, 0.5) == Point{80, 90});
    CHECK(ViewportToCanvas(viewport, -1.0, -0.25) == Point{-160, -45});
}

TEST_CASE("ViewportToCanvas: the pixel is found by truncation toward zero", "[rasterizer]") {
    const Viewport viewport(100, 100);
    CHECK(ViewportToCanvas(viewport, 0.337, 0.337) == Point{33, 33});      // 33.7
    CHECK(ViewportToCanvas(viewport, -0.337, -0.337) == Point{-33, -33});  // -33.7, not -34
    CHECK(ViewportToCanvas(viewport, 0.005, -0.005) == Point{0, 0});       // 0.5 and -0.5
}

TEST_CASE("ProjectVertex: a vertex on the viewport plane keeps its x and y", "[rasterizer]") {
    const Viewport viewport(1920, 1080);  // 1 x 1 at distance 1
    CHECK(ProjectVertex(viewport, {0.25, -0.5, 1.0}) == Point{480, -540});
    CHECK(ProjectVertex(viewport, {0.0, 0.0, 1.0}) == Point{0, 0});
}

TEST_CASE("ProjectVertex: farther vertices land closer to the center", "[rasterizer]") {
    const Viewport viewport(1920, 1080);
    CHECK(ProjectVertex(viewport, {0.5, 0.5, 1.0}) == Point{960, 540});  // the top-right corner
    CHECK(ProjectVertex(viewport, {0.5, 0.5, 2.0}) == Point{480, 270});  // twice as far: halfway
    CHECK(ProjectVertex(viewport, {0.5, 0.5, 4.0}) == Point{240, 135});
    // Nearer than the viewport: beyond the canvas, since there is no clipping.
    CHECK(ProjectVertex(viewport, {0.5, 0.5, 0.5}) == Point{1920, 1080});
}

TEST_CASE("ProjectVertex: vertices on one ray from the camera project to one pixel",
          "[rasterizer]") {
    const Viewport viewport(1920, 1080);
    const Point pixel = ProjectVertex(viewport, {0.25, -0.5, 1.0});
    CHECK(ProjectVertex(viewport, {0.5, -1.0, 2.0}) == pixel);
    CHECK(ProjectVertex(viewport, {2.0, -4.0, 8.0}) == pixel);
    CHECK(ProjectVertex(viewport, {0.125, -0.25, 0.5}) == pixel);
}

TEST_CASE("ProjectVertex: moving the viewport away zooms in", "[rasterizer]") {
    // The viewport at distance 2 sees (0.5, 0.5, 4) where the viewport at distance 1
    // sees (0.5, 0.5, 2): the picture scales with the distance.
    const Viewport near(1920, 1080, 1.0, 1.0, 1.0);
    const Viewport far(1920, 1080, 1.0, 1.0, 2.0);
    CHECK(ProjectVertex(far, {0.5, 0.5, 4.0}) == Point{480, 270});
    CHECK(ProjectVertex(far, {0.5, 0.5, 4.0}) == ProjectVertex(near, {0.5, 0.5, 2.0}));
    CHECK(ProjectVertex(far, {0.5, 0.5, 2.0}) == Point{960, 540});
}

TEST_CASE("ProjectVertex inverts CanvasToViewport pixel for pixel", "[rasterizer]") {
    // Sizes that are powers of two, so every product and quotient on the way there and
    // back is exact and the truncation has nothing to remove. On other sizes the round
    // trip can come back a hair below an integer and one pixel off (section 15 of the
    // spec), which is why the demo pins its own picture rather than this identity.
    const Viewport viewport(64, 32, 2.0, 1.0, 2.0);
    for (int x = -32; x < 32; ++x) {
        for (int y = -16; y < 16; ++y) {
            INFO("pixel (" << x << ", " << y << ")");
            CHECK(ProjectVertex(viewport, viewport.CanvasToViewport(x, y)) == Point{x, y});
        }
    }
}
