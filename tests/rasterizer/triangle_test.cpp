#include <cgfs/rasterizer/triangle.h>

#include <cgfs/image/canvas.h>
#include <cgfs/image/depth_buffer.h>
#include <cgfs/rasterizer/line.h>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "rasterizer/pixel_helpers.h"
#include "support/test_helpers.h"

using cgfs::Canvas;
using cgfs::Color;
using cgfs::DepthBuffer;
using cgfs::DrawFilledTriangle;
using cgfs::DrawLine;
using cgfs::DrawShadedTriangle;
using cgfs::DrawWireframeTriangle;
using cgfs::Point;
using cgfs::Rgb8;
using cgfs_test::Bytes;
using cgfs_test::LitPixels;
using cgfs_test::PixelsOf;
using namespace cgfs::colors;

namespace {

// A fresh black canvas with one filled triangle on it.
Canvas CanvasWithTriangle(Point p0, Point p1, Point p2, Color color = kWhite, int width = 32,
                          int height = 32) {
    Canvas canvas(width, height, kBlack);
    DrawFilledTriangle(canvas, p0, p1, p2, color);
    return canvas;
}

}  // namespace

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

TEST_CASE("DrawShadedTriangle: with h = 1 at every vertex it is the filled triangle",
          "[rasterizer]") {
    static_assert(Point{3, 4}.h == 1.0, "a Point without an intensity has the full color");

    // Every shape the filled-triangle tests cover, plus the two in which a row has its
    // left pixel beyond its right one and nothing may be drawn or interpolated for it.
    const Point triangles[][3] = {
        {{0, 0}, {4, 0}, {0, 4}},               // a right triangle
        {{-3, -2}, {4, 1}, {0, 5}},             // a general one
        {{0, 0}, {4, 4}, {0, 4}},               // the long side on the left
        {{0, 0}, {-4, 4}, {0, 4}},              // the long side on the right
        {{0, 6}, {0, 0}, {2, 2}},               // fractional x on the short sides
        {{-2, 0}, {2, 0}, {0, 2}},              // flat bottom
        {{-2, 2}, {2, 2}, {0, 0}},              // flat top
        {{0, 0}, {0, 3}, {0, 6}},               // collinear
        {{-100, -100}, {100, -100}, {0, 100}},  // beyond the canvas
        // Two rows with the middle vertex on the lower one: the middle-row rule misjudges
        // the sides and the lower row is skipped.
        {{0, 0}, {3, 0}, {1, 1}},
        // The two running sums for x end on either side of the apex, which is skipped.
        {{-6, -6}, {-6, -5}, {-5, 2}},
    };
    for (const auto& [a, b, c] : triangles) {
        INFO("triangle (" << a.x << ", " << a.y << ") (" << b.x << ", " << b.y << ") (" << c.x
                          << ", " << c.y << ")");
        Canvas filled(32, 32, kBlack);
        DrawFilledTriangle(filled, a, b, c, kYellow);
        Canvas shaded(32, 32, kBlack);
        DrawShadedTriangle(shaded, a, b, c, kYellow);
        CHECK(Bytes(shaded) == Bytes(filled));
    }
}

TEST_CASE("DrawShadedTriangle: a constant h scales the color everywhere", "[rasterizer]") {
    Canvas canvas(32, 32, kBlack);
    DrawShadedTriangle(canvas, {-3, -2, 0.5}, {4, 1, 0.5}, {0, 5, 0.5}, kWhite);
    const std::vector<Point> lit = LitPixels(canvas);
    REQUIRE(lit.size() > 10);
    for (const Point p : lit) {
        INFO("pixel (" << p.x << ", " << p.y << ")");
        CHECK(canvas.GetPixel(p.x, p.y) == Rgb8{127, 127, 127});  // 127.5 truncated
    }
}

TEST_CASE("DrawShadedTriangle: h is interpolated along the sides and then along every row",
          "[rasterizer]") {
    // The right triangle with the vertical side x = 0 and the hypotenuse from (4, 0) to
    // (0, 4). Every value below is a multiple of 1/4, so the interpolation is exact and
    // the pixels are predictable to the byte: 255 * x / 4, truncated.

    // h = 0 along the vertical side and 1 at (4, 0): along the hypotenuse h falls by 1/4
    // per row and so does the row's width, so every row rises by 1/4 per pixel and
    // h(x, y) = x / 4 over the whole triangle.
    Canvas across(32, 32, kBlack);
    DrawShadedTriangle(across, {0, 0, 0.0}, {4, 0, 1.0}, {0, 4, 0.0}, kWhite);
    for (int y = 0; y <= 4; ++y) {
        for (int x = 0; x + y <= 4; ++x) {
            const auto value = static_cast<std::uint8_t>(255 * x / 4);
            INFO("pixel (" << x << ", " << y << ")");
            CHECK(across.GetPixel(x, y) == Rgb8{value, value, value});
        }
    }
    CHECK(LitPixels(across).size() == 10);  // the x = 0 column is as black as the background

    // h = 0 along the bottom and 1 at (0, 4): both sides rise by 1/4 per row, every row is
    // constant and h(x, y) = y / 4.
    Canvas upward(32, 32, kBlack);
    DrawShadedTriangle(upward, {0, 0, 0.0}, {4, 0, 0.0}, {0, 4, 1.0}, kWhite);
    for (int y = 0; y <= 4; ++y) {
        for (int x = 0; x + y <= 4; ++x) {
            const auto value = static_cast<std::uint8_t>(255 * y / 4);
            INFO("pixel (" << x << ", " << y << ")");
            CHECK(upward.GetPixel(x, y) == Rgb8{value, value, value});
        }
    }
}

TEST_CASE("DrawShadedTriangle: the vertex order does not matter and h stays with its vertex",
          "[rasterizer]") {
    const Point a{-3, -2, 0.1};
    const Point b{4, 1, 0.6};
    const Point c{0, 5, 1.0};
    const auto draw = [](Point p0, Point p1, Point p2) {
        Canvas canvas(32, 32, kBlack);
        DrawShadedTriangle(canvas, p0, p1, p2, kWhite);
        return Bytes(canvas);
    };
    const std::vector<std::uint8_t> reference = draw(a, b, c);
    CHECK(draw(a, c, b) == reference);
    CHECK(draw(b, a, c) == reference);
    CHECK(draw(b, c, a) == reference);
    CHECK(draw(c, a, b) == reference);
    CHECK(draw(c, b, a) == reference);
    // The intensities did travel with their vertices: c is the bright one, a the dim one.
    Canvas canvas(32, 32, kBlack);
    DrawShadedTriangle(canvas, a, b, c, kWhite);
    CHECK(canvas.GetPixel(c.x, c.y) == Rgb8{255, 255, 255});
    CHECK(canvas.GetPixel(a.x, a.y) == Rgb8{25, 25, 25});
}

TEST_CASE("DrawShadedTriangle: two triangles sharing an edge agree on its intensities",
          "[rasterizer]") {
    // The rectangle of the filled-triangle test, cut along its diagonal from (0, 0) to
    // (4, 2). Both triangles interpolate the diagonal from the same two vertices, so the
    // pixels they share, (0, 0), (2, 1) and (4, 2), get the same intensity from either
    // one. The intensities are multiples of 1/4 so that every interpolated value is
    // exact and the comparison needs no tolerance.
    const Point corner{0, 0, 0.25};
    const Point far_corner{4, 2, 0.75};
    Canvas lower(16, 16, kBlack);
    DrawShadedTriangle(lower, corner, {4, 0, 0.5}, far_corner, kWhite);
    Canvas upper(16, 16, kBlack);
    DrawShadedTriangle(upper, corner, far_corner, {0, 2, 0.5}, kWhite);

    CHECK(lower.GetPixel(0, 0) == Rgb8{63, 63, 63});
    CHECK(lower.GetPixel(2, 1) == Rgb8{127, 127, 127});
    CHECK(lower.GetPixel(4, 2) == Rgb8{191, 191, 191});
    CHECK(upper.GetPixel(0, 0) == lower.GetPixel(0, 0));
    CHECK(upper.GetPixel(2, 1) == lower.GetPixel(2, 1));
    CHECK(upper.GetPixel(4, 2) == lower.GetPixel(4, 2));
}

TEST_CASE("DrawShadedTriangle: the shaded color is clamped by the canvas only", "[rasterizer]") {
    Canvas canvas(16, 16, kBlack);
    DrawShadedTriangle(canvas, {-2, -2, 2.0}, {2, -2, 2.0}, {0, 2, 2.0}, Color{100.0, 200.0, 10.0});
    CHECK(canvas.GetPixel(0, 0) == Rgb8{200, 255, 20});

    DrawShadedTriangle(canvas, {-2, -2, -1.0}, {2, -2, -1.0}, {0, 2, -1.0}, kWhite);
    CHECK(canvas.GetPixel(0, 0) == Rgb8{0, 0, 0});
}

// --- With a depth buffer ---------------------------------------------------------------

namespace {

// The same point at the depth z = 1 / inv_z.
Point AtDepth(Point p, double inv_z) {
    p.inv_z = inv_z;
    return p;
}

// Two triangles that overlap around the origin: one pointing up, at z = 2, and one
// pointing down, at z = 4.
struct Flat {
    Point a;
    Point b;
    Point c;
    Color color;
};
const Flat kNear{AtDepth({-6, -6}, 0.5), AtDepth({6, -6}, 0.5), AtDepth({0, 6}, 0.5), kRed};
const Flat kFar{AtDepth({-6, 6}, 0.25), AtDepth({6, 6}, 0.25), AtDepth({0, -6}, 0.25), kBlue};

// A fresh black canvas with the triangles drawn in the order given: through one depth
// buffer shared by all of them, or through none.
Canvas CanvasWithFlats(const std::vector<Flat>& flats, bool depth_buffered) {
    Canvas canvas(32, 32, kBlack);
    DepthBuffer depth_buffer(canvas);
    for (const Flat& flat : flats) {
        DrawFilledTriangle(canvas, flat.a, flat.b, flat.c, flat.color,
                           depth_buffered ? &depth_buffer : nullptr);
    }
    return canvas;
}

}  // namespace

TEST_CASE("DrawFilledTriangle: with a depth buffer the nearer triangle shows, in either order",
          "[rasterizer]") {
    const Canvas near_then_far = CanvasWithFlats({kNear, kFar}, true);
    const Canvas far_then_near = CanvasWithFlats({kFar, kNear}, true);
    CHECK(Bytes(near_then_far) == Bytes(far_then_near));
    CHECK(near_then_far.GetPixel(0, 0) == Rgb8{255, 0, 0});   // both cover it: the near one
    CHECK(near_then_far.GetPixel(0, 5) == Rgb8{255, 0, 0});   // the near one's tip, on the far one
    CHECK(near_then_far.GetPixel(-5, 6) == Rgb8{0, 0, 255});  // only the far one is here

    // It is the picture a painter gets by drawing the far one first, and not the one he
    // gets the other way around.
    CHECK(Bytes(near_then_far) == Bytes(CanvasWithFlats({kFar, kNear}, false)));
    CHECK(Bytes(near_then_far) != Bytes(CanvasWithFlats({kNear, kFar}, false)));
}

TEST_CASE("DrawFilledTriangle: without a depth buffer the later triangle shows", "[rasterizer]") {
    const Canvas near_then_far = CanvasWithFlats({kNear, kFar}, false);
    CHECK(near_then_far.GetPixel(0, 0) == Rgb8{0, 0, 255});
    // The depths are not even looked at: the same picture without them.
    Canvas flat(32, 32, kBlack);
    DrawFilledTriangle(flat, {-6, -6}, {6, -6}, {0, 6}, kRed);
    DrawFilledTriangle(flat, {-6, 6}, {6, 6}, {0, -6}, kBlue);
    CHECK(Bytes(near_then_far) == Bytes(flat));
}

TEST_CASE("DrawFilledTriangle: an empty depth buffer hides nothing and remembers the triangle",
          "[rasterizer]") {
    Canvas plain(32, 32, kBlack);
    DrawFilledTriangle(plain, kNear.a, kNear.b, kNear.c, kRed);

    Canvas canvas(32, 32, kBlack);
    DepthBuffer depth_buffer(canvas);
    DrawFilledTriangle(canvas, kNear.a, kNear.b, kNear.c, kRed, &depth_buffer);
    CHECK(Bytes(canvas) == Bytes(plain));

    // And the buffer remembers its depth exactly there.
    for (int x = -16; x < 16; ++x) {
        for (int y = -16; y < 16; ++y) {
            INFO("pixel (" << x << ", " << y << ")");
            const bool lit = canvas.GetPixel(x, y) != Rgb8{0, 0, 0};
            CHECK(depth_buffer.InvZ(x, y) == (lit ? 0.5 : 0.0));
        }
    }
}

TEST_CASE("DrawFilledTriangle: 1/z is interpolated along the sides and then along every row",
          "[rasterizer]") {
    // 1/z is 1 at the origin and falls by 3/32 per pixel to the right and by 2/32 per
    // pixel upward; every running sum is exact in binary.
    Canvas canvas(32, 32, kBlack);
    DepthBuffer depth_buffer(canvas);
    DrawFilledTriangle(canvas, AtDepth({0, 0}, 1.0), AtDepth({8, 0}, 0.25), AtDepth({0, 8}, 0.5),
                       kWhite, &depth_buffer);

    CHECK(depth_buffer.InvZ(0, 0) == 1.0);  // the vertices keep their own
    CHECK(depth_buffer.InvZ(8, 0) == 0.25);
    CHECK(depth_buffer.InvZ(0, 8) == 0.5);
    CHECK(depth_buffer.InvZ(0, 4) == 0.75);    // halfway up the left side
    CHECK(depth_buffer.InvZ(4, 4) == 0.375);   // halfway along the slanted side
    CHECK(depth_buffer.InvZ(2, 2) == 0.6875);  // inside: 1 - 2 * 3/32 - 2 * 2/32
    // Halfway along the bottom side, from z = 1 to z = 4, the depth is z = 1.6: the mean
    // of 1 and 1/4 is 1/z. It is not z = 2.5, the mean of the depths, 1/z = 0.4.
    CHECK(depth_buffer.InvZ(4, 0) == 0.625);
}

TEST_CASE("DrawFilledTriangle: two triangles that cross are sorted out pixel by pixel",
          "[rasterizer]") {
    // The same three pixels for both: one triangle tilted, from z = 1 at the origin to
    // z = 4 on the right, the other facing the camera at z = 2. Along the bottom row the
    // tilted one is in front as far as 1/z = 1 - 3x/32 > 1/2, that is up to x = 5.
    // Had z been interpolated instead, 1 + 3x/8 < 2 would hold only up to x = 2.
    const std::vector<Flat> tilted_then_facing = {
        {AtDepth({0, 0}, 1.0), AtDepth({8, 0}, 0.25), AtDepth({0, 8}, 0.5), kRed},
        {AtDepth({0, 0}, 0.5), AtDepth({8, 0}, 0.5), AtDepth({0, 8}, 0.5), kBlue},
    };
    const Canvas canvas = CanvasWithFlats(tilted_then_facing, true);
    for (int x = 0; x <= 5; ++x) {
        INFO("x = " << x);
        CHECK(canvas.GetPixel(x, 0) == Rgb8{255, 0, 0});
    }
    for (int x = 6; x <= 8; ++x) {
        INFO("x = " << x);
        CHECK(canvas.GetPixel(x, 0) == Rgb8{0, 0, 255});
    }
    // Drawn the other way around the picture is the same, except on the line where the
    // two are exactly equally deep, 3x + 2y = 16: there the one drawn first stays.
    const std::vector<Flat> facing_then_tilted = {tilted_then_facing[1], tilted_then_facing[0]};
    const Canvas other_way = CanvasWithFlats(facing_then_tilted, true);
    for (int x = 0; x <= 8; ++x) {
        for (int y = 0; x + y <= 8; ++y) {
            INFO("pixel (" << x << ", " << y << ")");
            if (3 * x + 2 * y == 16) {
                CHECK(canvas.GetPixel(x, y) == Rgb8{255, 0, 0});
                CHECK(other_way.GetPixel(x, y) == Rgb8{0, 0, 255});
            } else {
                CHECK(other_way.GetPixel(x, y) == canvas.GetPixel(x, y));
            }
        }
    }
}

TEST_CASE("DrawFilledTriangle: of two triangles at the same depth the first drawn stays",
          "[rasterizer]") {
    const Flat again{kNear.a, kNear.b, kNear.c, kGreen};
    CHECK(Bytes(CanvasWithFlats({kNear, again}, true)) == Bytes(CanvasWithFlats({kNear}, true)));
}

TEST_CASE("DrawFilledTriangle: a triangle without depth is behind everything", "[rasterizer]") {
    // A point made in 2D has 1/z = 0, infinitely far, and nothing is farther than an
    // empty pixel: through a depth buffer such a triangle is not drawn at all.
    Canvas canvas(32, 32, kBlack);
    DepthBuffer depth_buffer(canvas);
    DrawFilledTriangle(canvas, {-6, -6}, {6, -6}, {0, 6}, kRed, &depth_buffer);
    CHECK(LitPixels(canvas).empty());
}

TEST_CASE("DrawFilledTriangle: with a depth buffer a triangle may still reach beyond the canvas",
          "[rasterizer]") {
    Canvas plain(16, 16, kBlack);
    DrawFilledTriangle(plain, {-20, -3}, {20, 0}, {0, 20}, kRed);

    Canvas canvas(16, 16, kBlack);
    DepthBuffer depth_buffer(canvas);
    DrawFilledTriangle(canvas, AtDepth({-20, -3}, 0.5), AtDepth({20, 0}, 0.5),
                       AtDepth({0, 20}, 0.5), kRed, &depth_buffer);
    CHECK(Bytes(canvas) == Bytes(plain));
    CHECK(LitPixels(canvas).size() > 50);
}

TEST_CASE("DrawShadedTriangle: with a depth buffer the nearer triangle shows, with its shading",
          "[rasterizer]") {
    // The triangles of the filled test, the near one fading from black at its base to
    // the full color at its tip.
    Point a = kNear.a;
    Point b = kNear.b;
    a.h = 0.0;
    b.h = 0.0;
    const auto draw = [&](bool near_first, bool depth_buffered) {
        Canvas canvas(32, 32, kBlack);
        DepthBuffer depth_buffer(canvas);
        DepthBuffer* buffer = depth_buffered ? &depth_buffer : nullptr;
        if (near_first) {
            DrawShadedTriangle(canvas, a, b, kNear.c, kRed, buffer);
        }
        DrawShadedTriangle(canvas, kFar.a, kFar.b, kFar.c, kBlue, buffer);
        if (!near_first) {
            DrawShadedTriangle(canvas, a, b, kNear.c, kRed, buffer);
        }
        return canvas;
    };

    const Canvas near_then_far = draw(true, true);
    CHECK(Bytes(near_then_far) == Bytes(draw(false, true)));
    // The painter's picture with the far one first: the same pixels and the same shades.
    CHECK(Bytes(near_then_far) == Bytes(draw(false, false)));
    CHECK(near_then_far.GetPixel(0, 6) == Rgb8{255, 0, 0});  // the tip, h = 1
    CHECK(near_then_far.GetPixel(0, 0) == Rgb8{127, 0, 0});  // halfway up, h = 1/2
}

TEST_CASE("DrawShadedTriangle: with a depth buffer and h = 1 it is the filled triangle",
          "[rasterizer]") {
    Canvas filled(32, 32, kBlack);
    DepthBuffer filled_depths(filled);
    Canvas shaded(32, 32, kBlack);
    DepthBuffer shaded_depths(shaded);
    for (const Flat& flat : {kNear, kFar}) {
        DrawFilledTriangle(filled, flat.a, flat.b, flat.c, flat.color, &filled_depths);
        DrawShadedTriangle(shaded, flat.a, flat.b, flat.c, flat.color, &shaded_depths);
    }
    CHECK(Bytes(shaded) == Bytes(filled));
    for (int x = -16; x < 16; ++x) {
        for (int y = -16; y < 16; ++y) {
            CHECK(shaded_depths.InvZ(x, y) == filled_depths.InvZ(x, y));
        }
    }
}

TEST_CASE("DrawWireframeTriangle: takes a depth buffer and ignores it", "[rasterizer]") {
    Canvas plain(32, 32, kBlack);
    DrawWireframeTriangle(plain, kFar.a, kFar.b, kFar.c, kBlue);

    // An outline at z = 4 drawn after a filled triangle at z = 2: nothing of it is
    // hidden, and it leaves no depth behind.
    Canvas canvas(32, 32, kBlack);
    DepthBuffer depth_buffer(canvas);
    DrawFilledTriangle(canvas, kNear.a, kNear.b, kNear.c, kRed, &depth_buffer);
    DrawWireframeTriangle(canvas, kFar.a, kFar.b, kFar.c, kBlue, &depth_buffer);
    for (const Point pixel : LitPixels(plain)) {
        INFO("pixel (" << pixel.x << ", " << pixel.y << ")");
        CHECK(canvas.GetPixel(pixel.x, pixel.y) == Rgb8{0, 0, 255});
    }
    CHECK(depth_buffer.InvZ(-5, 6) == 0.0);  // on the outline, outside the filled triangle
}
