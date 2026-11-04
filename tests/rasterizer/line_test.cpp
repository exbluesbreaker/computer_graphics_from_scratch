#include <cgfs/rasterizer/line.h>

#include <cgfs/image/canvas.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include "rasterizer/pixel_helpers.h"
#include "support/test_helpers.h"

using cgfs::Canvas;
using cgfs::Color;
using cgfs::DrawLine;
using cgfs::Point;
using cgfs::Rgb8;
using cgfs_test::Bytes;
using cgfs_test::LitPixels;
using namespace cgfs::colors;

namespace {

// A fresh black canvas with one line on it.
Canvas CanvasWithLine(Point p0, Point p1, Color color = kWhite, int width = 32,
                      int height = 32) {
    Canvas canvas(width, height, kBlack);
    DrawLine(canvas, p0, p1, color);
    return canvas;
}

}  // namespace

TEST_CASE("DrawLine: a horizontal line lights one pixel per column", "[rasterizer]") {
    const Canvas canvas = CanvasWithLine({0, 2}, {4, 2});
    CHECK(LitPixels(canvas) == std::vector<Point>{{0, 2}, {1, 2}, {2, 2}, {3, 2}, {4, 2}});
}

TEST_CASE("DrawLine: a vertical line lights one pixel per row", "[rasterizer]") {
    const Canvas canvas = CanvasWithLine({-3, -1}, {-3, 3});
    CHECK(LitPixels(canvas) ==
          std::vector<Point>{{-3, -1}, {-3, 0}, {-3, 1}, {-3, 2}, {-3, 3}});
}

TEST_CASE("DrawLine: a diagonal lights one pixel per row and column", "[rasterizer]") {
    const Canvas canvas = CanvasWithLine({0, 0}, {3, 3});
    CHECK(LitPixels(canvas) == std::vector<Point>{{0, 0}, {1, 1}, {2, 2}, {3, 3}});
}

TEST_CASE("DrawLine: a zero-length line is a single pixel", "[rasterizer]") {
    const Canvas canvas = CanvasWithLine({5, -7}, {5, -7});
    CHECK(LitPixels(canvas) == std::vector<Point>{{5, -7}});
}

TEST_CASE("DrawLine: a horizontal-ish line steps y by truncation", "[rasterizer]") {
    // Slope 1/2: y runs 0, 0.5, 1, 1.5, 2 and truncates to 0, 0, 1, 1, 2.
    const Canvas canvas = CanvasWithLine({0, 0}, {4, 2});
    CHECK(LitPixels(canvas) == std::vector<Point>{{0, 0}, {1, 0}, {2, 1}, {3, 1}, {4, 2}});
}

TEST_CASE("DrawLine: truncation is toward zero, so the staircase mirrors at the axis",
          "[rasterizer]") {
    // The same line pointing the other way: y runs -2, -1.5, -1, -0.5, 0 and truncates
    // to -2, -1, -1, 0, 0 (not -2, -2, -1, -1, 0 as flooring would give).
    const Canvas canvas = CanvasWithLine({0, 0}, {-4, -2});
    CHECK(LitPixels(canvas) ==
          std::vector<Point>{{-4, -2}, {-3, -1}, {-2, -1}, {-1, 0}, {0, 0}});
}

TEST_CASE("DrawLine: a vertical-ish line steps x by truncation", "[rasterizer]") {
    // Slope 2: x runs 0, 0.5, 1, 1.5, 2 over the rows and truncates to 0, 0, 1, 1, 2.
    const Canvas canvas = CanvasWithLine({0, 0}, {2, 4});
    CHECK(LitPixels(canvas) == std::vector<Point>{{0, 0}, {0, 1}, {1, 2}, {1, 3}, {2, 4}});

    const Canvas mirrored = CanvasWithLine({0, 0}, {-2, -4});
    CHECK(LitPixels(mirrored) ==
          std::vector<Point>{{-2, -4}, {-1, -3}, {-1, -2}, {0, -1}, {0, 0}});
}

TEST_CASE("DrawLine: the endpoint order does not matter", "[rasterizer]") {
    const Point pairs[][2] = {
        {{-10, -3}, {10, 4}},   // horizontal-ish, rising
        {{-10, 4}, {10, -3}},   // horizontal-ish, falling
        {{-3, -10}, {4, 10}},   // vertical-ish, rising
        {{4, -10}, {-3, 10}},   // vertical-ish, falling
        {{-6, -6}, {6, 6}},     // diagonal
        {{-8, 0}, {8, 0}},      // horizontal
        {{0, -8}, {0, 8}},      // vertical
    };
    for (const auto& [p0, p1] : pairs) {
        INFO("line from (" << p0.x << ", " << p0.y << ") to (" << p1.x << ", " << p1.y << ")");
        CHECK(Bytes(CanvasWithLine(p0, p1)) == Bytes(CanvasWithLine(p1, p0)));
    }
}

TEST_CASE("DrawLine: a horizontal-ish line has exactly one pixel in every column",
          "[rasterizer]") {
    const Canvas canvas = CanvasWithLine({-10, -3}, {10, 4});
    const std::vector<Point> lit = LitPixels(canvas);
    REQUIRE(lit.size() == 21);
    for (int x = -10; x <= 10; ++x) {
        CHECK(lit[static_cast<std::size_t>(x + 10)].x == x);
    }
    // No gaps: consecutive pixels are at most one row apart.
    for (std::size_t i = 1; i < lit.size(); ++i) {
        CHECK(std::abs(lit[i].y - lit[i - 1].y) <= 1);
    }
}

TEST_CASE("DrawLine: a vertical-ish line has exactly one pixel in every row", "[rasterizer]") {
    const Canvas canvas = CanvasWithLine({-3, -10}, {4, 10});
    std::vector<Point> lit = LitPixels(canvas);
    REQUIRE(lit.size() == 21);
    std::sort(lit.begin(), lit.end(), [](Point a, Point b) { return a.y < b.y; });
    for (int y = -10; y <= 10; ++y) {
        CHECK(lit[static_cast<std::size_t>(y + 10)].y == y);
    }
    for (std::size_t i = 1; i < lit.size(); ++i) {
        CHECK(std::abs(lit[i].x - lit[i - 1].x) <= 1);
    }
}

TEST_CASE("DrawLine: a line may run off the canvas", "[rasterizer]") {
    // An 8x8 canvas covers -4 <= x, y < 4; the lines reach far outside it.
    const Canvas horizontal = CanvasWithLine({-20, 1}, {20, 1}, kWhite, 8, 8);
    CHECK(LitPixels(horizontal) == std::vector<Point>{{-4, 1}, {-3, 1}, {-2, 1}, {-1, 1},
                                                      {0, 1}, {1, 1}, {2, 1}, {3, 1}});

    const Canvas steep = CanvasWithLine({0, -100}, {0, 100}, kWhite, 8, 8);
    CHECK(LitPixels(steep).size() == 8);

    const Canvas outside = CanvasWithLine({10, 10}, {20, 15}, kWhite, 8, 8);
    CHECK(LitPixels(outside).empty());
}

TEST_CASE("DrawLine: pixels take the line's color", "[rasterizer]") {
    Canvas canvas(8, 8, kBlack);
    DrawLine(canvas, {-2, -2}, {2, 2}, Color{300.0, 127.9, -5.0});
    CHECK(canvas.GetPixel(0, 0) == Rgb8{255, 127, 0});
    CHECK(canvas.GetPixel(2, 2) == Rgb8{255, 127, 0});
    CHECK(canvas.GetPixel(0, 1) == Rgb8{0, 0, 0});
}

