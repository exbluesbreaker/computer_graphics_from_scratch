#include <cgfs/rasterizer.h>

#include <cgfs/canvas.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <vector>

#include "test_helpers.h"

using Catch::Matchers::WithinAbs;
using cgfs::Canvas;
using cgfs::Color;
using cgfs::DrawFilledTriangle;
using cgfs::DrawLine;
using cgfs::DrawWireframeTriangle;
using cgfs::Interpolate;
using cgfs::Point;
using cgfs::Rgb8;
using namespace cgfs::colors;

namespace {

// Every pixel that differs from the background, sorted by x and then by y.
std::vector<Point> LitPixels(const Canvas& canvas) {
    const Rgb8 background = cgfs::ToRgb8(canvas.background());
    std::vector<Point> lit;
    for (int x = -canvas.width() / 2; x < canvas.width() / 2; ++x) {
        for (int y = -canvas.height() / 2; y < canvas.height() / 2; ++y) {
            if (canvas.GetPixel(x, y) != background) {
                lit.push_back({x, y});
            }
        }
    }
    return lit;
}

// A fresh black canvas with one line on it.
Canvas CanvasWithLine(Point p0, Point p1, Color color = kWhite, int width = 32,
                      int height = 32) {
    Canvas canvas(width, height, kBlack);
    DrawLine(canvas, p0, p1, color);
    return canvas;
}

std::vector<std::uint8_t> Bytes(const Canvas& canvas) {
    return {canvas.Data().begin(), canvas.Data().end()};
}

// A fresh black canvas with one filled triangle on it.
Canvas CanvasWithTriangle(Point p0, Point p1, Point p2, Color color = kWhite, int width = 32,
                          int height = 32) {
    Canvas canvas(width, height, kBlack);
    DrawFilledTriangle(canvas, p0, p1, p2, color);
    return canvas;
}

// The pixels x_min..x_max of row y.
struct Span {
    int y;
    int x_min;
    int x_max;
};

// The pixels of some horizontal spans, in the order LitPixels uses.
std::vector<Point> PixelsOf(std::initializer_list<Span> spans) {
    std::vector<Point> pixels;
    for (const Span& span : spans) {
        for (int x = span.x_min; x <= span.x_max; ++x) {
            pixels.push_back({x, span.y});
        }
    }
    std::sort(pixels.begin(), pixels.end(),
              [](Point a, Point b) { return a.x != b.x ? a.x < b.x : a.y < b.y; });
    return pixels;
}

}  // namespace

TEST_CASE("Interpolate: a single point gives the start value only", "[rasterizer]") {
    CHECK(Interpolate(3, 7.5, 3, 100.0) == std::vector<double>{7.5});
}

TEST_CASE("Interpolate: one value per integer step, both ends included", "[rasterizer]") {
    const std::vector<double> values = Interpolate(0, 0.0, 4, 8.0);
    REQUIRE(values.size() == 5);
    CHECK(values == std::vector<double>{0.0, 2.0, 4.0, 6.0, 8.0});
}

TEST_CASE("Interpolate: fractional slopes give fractional values", "[rasterizer]") {
    CHECK(Interpolate(0, 0.0, 4, 2.0) == std::vector<double>{0.0, 0.5, 1.0, 1.5, 2.0});
}

TEST_CASE("Interpolate: the dependent variable may decrease", "[rasterizer]") {
    CHECK(Interpolate(-2, 10.0, 2, 2.0) == std::vector<double>{10.0, 8.0, 6.0, 4.0, 2.0});
}

TEST_CASE("Interpolate: equal end values give a constant", "[rasterizer]") {
    CHECK(Interpolate(5, 3.0, 8, 3.0) == std::vector<double>{3.0, 3.0, 3.0, 3.0});
}

TEST_CASE("Interpolate: the independent variable may be negative", "[rasterizer]") {
    CHECK(Interpolate(-5, 1.0, -1, 5.0) == std::vector<double>{1.0, 2.0, 3.0, 4.0, 5.0});
}

TEST_CASE("Interpolate: values[k] is d0 + k * slope", "[rasterizer]") {
    // A slope that is not exactly representable: 1/3. The running sum drifts by a few
    // ulps at most.
    const std::vector<double> values = Interpolate(10, 2.0, 40, 12.0);
    REQUIRE(values.size() == 31);
    for (std::size_t k = 0; k < values.size(); ++k) {
        CHECK_THAT(values[k], WithinAbs(2.0 + static_cast<double>(k) / 3.0, 1e-12));
    }
}

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

TEST_CASE("DrawFilledTriangle: a right triangle fills its rows", "[rasterizer]") {
    const Canvas canvas = CanvasWithTriangle({0, 0}, {4, 0}, {0, 4});
    CHECK(LitPixels(canvas) == PixelsOf({{0, 0, 4}, {1, 0, 3}, {2, 0, 2}, {3, 0, 1}, {4, 0, 0}}));
}

TEST_CASE("DrawFilledTriangle: the vertex order does not matter", "[rasterizer]") {
    const Point a{-3, -2};
    const Point b{4, 1};
    const Point c{0, 5};
    const std::vector<std::uint8_t> reference = Bytes(CanvasWithTriangle(a, b, c));
    CHECK(Bytes(CanvasWithTriangle(a, c, b)) == reference);
    CHECK(Bytes(CanvasWithTriangle(b, a, c)) == reference);
    CHECK(Bytes(CanvasWithTriangle(b, c, a)) == reference);
    CHECK(Bytes(CanvasWithTriangle(c, a, b)) == reference);
    CHECK(Bytes(CanvasWithTriangle(c, b, a)) == reference);
    CHECK(LitPixels(CanvasWithTriangle(a, b, c)).size() > 10);  // and something was drawn
}

TEST_CASE("DrawFilledTriangle: the long side may be on either side", "[rasterizer]") {
    // The long side p0 p2 is the vertical edge x = 0; the other two sides lean right...
    const Canvas long_side_left = CanvasWithTriangle({0, 0}, {4, 4}, {0, 4});
    CHECK(LitPixels(long_side_left) ==
          PixelsOf({{0, 0, 0}, {1, 0, 1}, {2, 0, 2}, {3, 0, 3}, {4, 0, 4}}));

    // ... or left.
    const Canvas long_side_right = CanvasWithTriangle({0, 0}, {-4, 4}, {0, 4});
    CHECK(LitPixels(long_side_right) ==
          PixelsOf({{0, 0, 0}, {1, -1, 0}, {2, -2, 0}, {3, -3, 0}, {4, -4, 0}}));
}

TEST_CASE("DrawFilledTriangle: the short sides are joined at the middle vertex and x is "
          "truncated",
          "[rasterizer]") {
    // Sorted by y: (0, 0), (2, 2), (0, 6). The long side is x = 0. The short sides give
    // x = 0, 1 (rows 0..1, the row of the middle vertex dropped) followed by
    // x = 2, 1.5, 1, 0.5, 0 (rows 2..6); 1.5 and 0.5 truncate to 1 and 0.
    const Canvas canvas = CanvasWithTriangle({0, 6}, {0, 0}, {2, 2});
    CHECK(LitPixels(canvas) == PixelsOf({{0, 0, 0},
                                         {1, 0, 1},
                                         {2, 0, 2},
                                         {3, 0, 1},
                                         {4, 0, 1},
                                         {5, 0, 0},
                                         {6, 0, 0}}));
}

TEST_CASE("DrawFilledTriangle: flat bottom and flat top", "[rasterizer]") {
    // Flat bottom: y0 == y1, so the first short side is a single row and drops out.
    const Canvas flat_bottom = CanvasWithTriangle({-2, 0}, {2, 0}, {0, 2});
    CHECK(LitPixels(flat_bottom) == PixelsOf({{0, -2, 2}, {1, -1, 1}, {2, 0, 0}}));

    // Flat top: y1 == y2, so the second short side is a single row.
    const Canvas flat_top = CanvasWithTriangle({-2, 2}, {2, 2}, {0, 0});
    CHECK(LitPixels(flat_top) == PixelsOf({{0, 0, 0}, {1, -1, 1}, {2, -2, 2}}));
}

TEST_CASE("DrawFilledTriangle: collinear vertices give a line", "[rasterizer]") {
    const Canvas canvas = CanvasWithTriangle({0, 0}, {0, 3}, {0, 6});
    CHECK(LitPixels(canvas) == PixelsOf({{0, 0, 0}, {1, 0, 0}, {2, 0, 0}, {3, 0, 0},
                                         {4, 0, 0}, {5, 0, 0}, {6, 0, 0}}));
}

TEST_CASE("DrawFilledTriangle: two triangles sharing an edge tile without gaps",
          "[rasterizer]") {
    // The rectangle 0..4 x 0..2 cut along its diagonal from (0, 0) to (4, 2).
    Canvas canvas(16, 16, kBlack);
    DrawFilledTriangle(canvas, {0, 0}, {4, 0}, {4, 2}, kRed);
    DrawFilledTriangle(canvas, {0, 0}, {4, 2}, {0, 2}, kBlue);

    // Every pixel of the rectangle is covered and nothing outside it.
    CHECK(LitPixels(canvas) == PixelsOf({{0, 0, 4}, {1, 0, 4}, {2, 0, 4}}));

    // Both triangles interpolate the shared edge from the same endpoints, so they agree
    // on its pixels: (0, 0), (2, 1) and (4, 2). The blue one, drawn last, owns them.
    std::size_t red = 0;
    std::size_t blue = 0;
    for (const Point p : LitPixels(canvas)) {
        red += canvas.GetPixel(p.x, p.y) == Rgb8{255, 0, 0};
        blue += canvas.GetPixel(p.x, p.y) == Rgb8{0, 0, 255};
    }
    CHECK(red == 6);
    CHECK(blue == 9);
    CHECK(canvas.GetPixel(0, 0) == Rgb8{0, 0, 255});
    CHECK(canvas.GetPixel(2, 1) == Rgb8{0, 0, 255});
    CHECK(canvas.GetPixel(4, 2) == Rgb8{0, 0, 255});
    CHECK(canvas.GetPixel(4, 0) == Rgb8{255, 0, 0});
}

TEST_CASE("DrawFilledTriangle: a triangle may reach beyond the canvas", "[rasterizer]") {
    // An 8x8 canvas covers -4 <= x, y < 4.
    const Canvas covered = CanvasWithTriangle({-100, -100}, {100, -100}, {0, 100}, kWhite, 8, 8);
    CHECK(LitPixels(covered).size() == 64);

    const Canvas outside = CanvasWithTriangle({10, 10}, {20, 10}, {15, 20}, kWhite, 8, 8);
    CHECK(LitPixels(outside).empty());
}

TEST_CASE("DrawFilledTriangle: pixels take the triangle's color", "[rasterizer]") {
    const Canvas canvas = CanvasWithTriangle({-2, -2}, {2, -2}, {0, 2}, Color{300.0, 127.9, -5.0});
    CHECK(canvas.GetPixel(0, 0) == Rgb8{255, 127, 0});
    CHECK(canvas.GetPixel(0, -2) == Rgb8{255, 127, 0});
    CHECK(canvas.GetPixel(3, 0) == Rgb8{0, 0, 0});
}

TEST_CASE("DrawWireframeTriangle: draws the three edges", "[rasterizer]") {
    const Point a{-6, -4};
    const Point b{7, -1};
    const Point c{1, 8};

    Canvas wireframe(32, 32, kBlack);
    DrawWireframeTriangle(wireframe, a, b, c, kGreen);

    Canvas edges(32, 32, kBlack);
    DrawLine(edges, a, b, kGreen);
    DrawLine(edges, b, c, kGreen);
    DrawLine(edges, c, a, kGreen);

    CHECK(Bytes(wireframe) == Bytes(edges));
    CHECK(wireframe.GetPixel(a.x, a.y) == Rgb8{0, 255, 0});
    CHECK(wireframe.GetPixel(b.x, b.y) == Rgb8{0, 255, 0});
    CHECK(wireframe.GetPixel(c.x, c.y) == Rgb8{0, 255, 0});
}
