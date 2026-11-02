#include <cgfs/viewport.h>

#include <cgfs/canvas.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <stdexcept>

#include "test_helpers.h"

using Catch::Matchers::WithinAbs;
using cgfs::Canvas;
using cgfs::Vec3;
using cgfs::Viewport;

TEST_CASE("Default viewport maps the center, the corners and a quarter point", "[viewport]") {
    const Viewport viewport(1920, 1080);  // 1 x 1 at distance 1
    CHECK(viewport.CanvasToViewport(0, 0) == Vec3{0.0, 0.0, 1.0});
    CHECK(viewport.CanvasToViewport(960, 540) == Vec3{0.5, 0.5, 1.0});      // top-right
    CHECK(viewport.CanvasToViewport(-960, -540) == Vec3{-0.5, -0.5, 1.0});  // bottom-left
    CHECK(viewport.CanvasToViewport(480, -270) == Vec3{0.25, -0.25, 1.0});  // quarter
}

TEST_CASE("Viewport built from a canvas copies its dimensions", "[viewport]") {
    const Canvas canvas(480, 270);
    const Viewport viewport(canvas, 2.0, 1.5, 3.0);
    CHECK(viewport.canvas_width() == 480);
    CHECK(viewport.canvas_height() == 270);
    CHECK(viewport.width() == 2.0);
    CHECK(viewport.height() == 1.5);
    CHECK(viewport.distance() == 3.0);
    CHECK(viewport.CanvasToViewport(240, 135) == Vec3{1.0, 0.75, 3.0});
}

TEST_CASE("MatchingCanvasAspect keeps the image from stretching", "[viewport]") {
    const Canvas canvas(1920, 1080);

    const Viewport wide = Viewport::MatchingCanvasAspect(canvas);
    CHECK_THAT(wide.width(), WithinAbs(16.0 / 9.0, 1e-12));
    CHECK(wide.height() == 1.0);
    CHECK(wide.distance() == 1.0);
    const Vec3 top_right = wide.CanvasToViewport(960, 540);
    CHECK_THAT(top_right.x, WithinAbs(16.0 / 18.0, 1e-12));
    CHECK(top_right.y == 0.5);
    CHECK(top_right.z == 1.0);

    const Viewport custom = Viewport::MatchingCanvasAspect(canvas, 2.0, 3.0);
    CHECK_THAT(custom.width(), WithinAbs(2.0 * 16.0 / 9.0, 1e-12));
    CHECK(custom.height() == 2.0);
    CHECK(custom.distance() == 3.0);
}

TEST_CASE("Field of view", "[viewport]") {
    const auto square = Viewport(1920, 1080).Fov();
    CHECK_THAT(square.horizontal_degrees, WithinAbs(53.13, 0.01));
    CHECK_THAT(square.vertical_degrees, WithinAbs(53.13, 0.01));

    const auto wide = Viewport(1920, 1080, 16.0 / 9.0, 1.0, 1.0).Fov();
    CHECK_THAT(wide.horizontal_degrees, WithinAbs(83.27, 0.01));
    CHECK_THAT(wide.vertical_degrees, WithinAbs(53.13, 0.01));

    // Moving the viewport further away narrows the field of view (zooms in).
    const auto zoomed = Viewport(1920, 1080, 1.0, 1.0, 2.0).Fov();
    CHECK_THAT(zoomed.horizontal_degrees, WithinAbs(28.07, 0.01));
}

TEST_CASE("Viewport rejects non-positive arguments", "[viewport]") {
    CHECK_THROWS_AS(Viewport(0, 10), std::invalid_argument);
    CHECK_THROWS_AS(Viewport(10, 0), std::invalid_argument);
    CHECK_THROWS_AS(Viewport(10, 10, 0.0), std::invalid_argument);
    CHECK_THROWS_AS(Viewport(10, 10, 1.0, -1.0), std::invalid_argument);
    CHECK_THROWS_AS(Viewport(10, 10, 1.0, 1.0, 0.0), std::invalid_argument);
}
