#include <cgfs/rasterizer/projection.h>

#include <cgfs/camera/viewport.h>
#include <cgfs/math/vec3.h>
#include <cgfs/rasterizer/point.h>

#include <catch2/catch_test_macros.hpp>

#include "support/test_helpers.h"

using cgfs::Point;
using cgfs::ProjectVertex;
using cgfs::UnprojectVertex;
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
    CHECK(ProjectVertex(viewport, {0.25, -0.5, 1.0}) == Point{.x = 480, .y = -540, .inv_z = 1.0});
    CHECK(ProjectVertex(viewport, {0.0, 0.0, 1.0}) == Point{.x = 0, .y = 0, .inv_z = 1.0});
}

TEST_CASE("ProjectVertex: farther vertices land closer to the center", "[rasterizer]") {
    const Viewport viewport(1920, 1080);
    // The top-right corner; twice as far, halfway; and so on.
    CHECK(ProjectVertex(viewport, {0.5, 0.5, 1.0}) == Point{.x = 960, .y = 540, .inv_z = 1.0});
    CHECK(ProjectVertex(viewport, {0.5, 0.5, 2.0}) == Point{.x = 480, .y = 270, .inv_z = 0.5});
    CHECK(ProjectVertex(viewport, {0.5, 0.5, 4.0}) == Point{.x = 240, .y = 135, .inv_z = 0.25});
    // Nearer than the viewport: beyond the canvas, since there is no clipping.
    CHECK(ProjectVertex(viewport, {0.5, 0.5, 0.5}) == Point{.x = 1920, .y = 1080, .inv_z = 2.0});
}

TEST_CASE("ProjectVertex: vertices on one ray from the camera project to one pixel",
          "[rasterizer]") {
    const Viewport viewport(1920, 1080);
    const Point pixel = ProjectVertex(viewport, {0.25, -0.5, 1.0});
    const Vec3 on_the_ray[] = {{0.5, -1.0, 2.0}, {2.0, -4.0, 8.0}, {0.125, -0.25, 0.5}};
    for (const Vec3 vertex : on_the_ray) {
        const Point projected = ProjectVertex(viewport, vertex);
        CHECK(projected.x == pixel.x);
        CHECK(projected.y == pixel.y);
    }
}

TEST_CASE("ProjectVertex: the point remembers 1/z, which tells vertices on one ray apart",
          "[rasterizer]") {
    const Viewport viewport(1920, 1080);
    CHECK(ProjectVertex(viewport, {0.25, -0.5, 1.0}).inv_z == 1.0);
    CHECK(ProjectVertex(viewport, {0.5, -1.0, 2.0}).inv_z == 0.5);
    CHECK(ProjectVertex(viewport, {2.0, -4.0, 8.0}).inv_z == 0.125);
    // The nearer vertex has the larger 1/z, wherever it is on the canvas.
    const Point nearer = ProjectVertex(viewport, {0.1, 0.2, 3.0});
    const Point farther = ProjectVertex(viewport, {-0.4, 0.0, 5.0});
    CHECK(nearer.inv_z > farther.inv_z);
    // It is the vertex's depth and not the viewport's distance that is remembered.
    const Viewport far(1920, 1080, 1.0, 1.0, 2.0);
    CHECK(ProjectVertex(far, {0.5, 0.5, 4.0}).inv_z == 0.25);
}

TEST_CASE("ViewportToCanvas: a viewport point has no depth", "[rasterizer]") {
    const Viewport viewport(1920, 1080);
    CHECK(ViewportToCanvas(viewport, 0.25, -0.25).inv_z == 0.0);
}

TEST_CASE("ProjectVertex: moving the viewport away zooms in", "[rasterizer]") {
    // The viewport at distance 2 sees (0.5, 0.5, 4) where the viewport at distance 1
    // sees (0.5, 0.5, 2): the picture scales with the distance.
    const Viewport near(1920, 1080, 1.0, 1.0, 1.0);
    const Viewport far(1920, 1080, 1.0, 1.0, 2.0);
    const Point zoomed = ProjectVertex(far, {0.5, 0.5, 4.0});
    CHECK(zoomed == Point{.x = 480, .y = 270, .inv_z = 0.25});
    const Point unzoomed = ProjectVertex(near, {0.5, 0.5, 2.0});
    CHECK(zoomed.x == unzoomed.x);
    CHECK(zoomed.y == unzoomed.y);
    CHECK(ProjectVertex(far, {0.5, 0.5, 2.0}) == Point{.x = 960, .y = 540, .inv_z = 0.5});
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
            // The viewport is at distance 2, so that is the depth of every point on it.
            CHECK(ProjectVertex(viewport, viewport.CanvasToViewport(x, y)) ==
                  Point{.x = x, .y = y, .inv_z = 0.5});
        }
    }
}

TEST_CASE("UnprojectVertex: a pixel and its depth give back the vertex", "[rasterizer]") {
    const Viewport viewport(1920, 1080);  // 1 x 1 at distance 1
    // The pixel (480, -540) looks through the viewport point (0.25, -0.5, 1): at the
    // depth 1 that is the vertex itself.
    CHECK(UnprojectVertex(viewport, 480, -540, 1.0) == Vec3{0.25, -0.5, 1.0});
    // The same pixel at the depth 4: four times as far along the same ray.
    CHECK(UnprojectVertex(viewport, 480, -540, 0.25) == Vec3{1.0, -2.0, 4.0});
    // The middle pixel looks along the axis.
    CHECK(UnprojectVertex(viewport, 0, 0, 0.5) == Vec3{0.0, 0.0, 2.0});
}

TEST_CASE("UnprojectVertex: the depth is the vertex's whatever the viewport's distance",
          "[rasterizer]") {
    // Through a viewport at distance 2 the pixel (960, 540) looks through (0.5, 0.5, 2),
    // and at the depth 4 it shows the point twice as far along that ray.
    const Viewport far(1920, 1080, 1.0, 1.0, 2.0);
    CHECK(UnprojectVertex(far, 960, 540, 0.5) == Vec3{0.5, 0.5, 2.0});
    CHECK(UnprojectVertex(far, 960, 540, 0.25) == Vec3{1.0, 1.0, 4.0});
}

TEST_CASE("UnprojectVertex inverts ProjectVertex", "[rasterizer]") {
    // Sizes that are powers of two, as in the round trip above: on every ray through a
    // pixel, a vertex at each of three depths is projected to that pixel with its 1/z,
    // and the pixel with that 1/z is unprojected to the vertex, exactly.
    const Viewport viewport(64, 32, 2.0, 1.0, 2.0);
    for (int x = -32; x < 32; ++x) {
        for (int y = -16; y < 16; ++y) {
            for (const double z : {2.0, 4.0, 8.0}) {
                INFO("pixel (" << x << ", " << y << ") at depth " << z);
                const Vec3 vertex = viewport.CanvasToViewport(x, y) * (z / 2.0);
                const Point projected = ProjectVertex(viewport, vertex);
                CHECK(projected == Point{.x = x, .y = y, .inv_z = 1.0 / z});
                CHECK(UnprojectVertex(viewport, projected.x, projected.y, projected.inv_z) ==
                      vertex);
            }
        }
    }
}
