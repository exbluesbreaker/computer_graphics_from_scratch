#include <cgfs/image/color.h>
#include <cgfs/image/image_io.h>
#include <cgfs/image/texture.h>

#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

#include "support/address_texture.h"
#include "support/test_helpers.h"

using cgfs::Color;
using cgfs::Image;
using cgfs::Texture;
using cgfs::TextureFilter;
using cgfs::Uv;
using cgfs_test::AddressTexture;
using cgfs_test::TexelOf;

TEST_CASE("Uv is the texture's origin unless given", "[texture]") {
    CHECK(Uv{} == Uv{0.0, 0.0});
    CHECK(Uv{0.25, 0.75}.u == 0.25);
    CHECK(Uv{0.25, 0.75}.v == 0.75);
    CHECK(Uv{0.25, 0.75} != Uv{0.75, 0.25});
}

TEST_CASE("Texture rejects an image without pixels or with the wrong number of bytes",
          "[texture]") {
    CHECK_THROWS_AS(Texture(Image{.width = 0, .height = 2, .rgb = {}}), std::invalid_argument);
    CHECK_THROWS_AS(Texture(Image{.width = 2, .height = -1, .rgb = {}}), std::invalid_argument);
    CHECK_THROWS_AS(Texture(Image{}), std::invalid_argument);
    // 2 x 2 texels are twelve bytes.
    CHECK_THROWS_AS(Texture(Image{.width = 2, .height = 2, .rgb = std::vector<std::uint8_t>(11)}),
                    std::invalid_argument);
    CHECK_THROWS_AS(Texture(Image{.width = 2, .height = 2, .rgb = std::vector<std::uint8_t>(13)}),
                    std::invalid_argument);
    CHECK_NOTHROW(Texture(Image{.width = 2, .height = 2, .rgb = std::vector<std::uint8_t>(12)}));
}

TEST_CASE("Texture has the size of its image", "[texture]") {
    const Texture texture = AddressTexture(4, 2);
    CHECK(texture.width() == 4);
    CHECK(texture.height() == 2);
}

TEST_CASE("Texture::GetTexel: a texel is three bytes of the image, as a color", "[texture]") {
    const Texture texture(Image{.width = 2, .height = 1, .rgb = {10, 20, 30, 255, 0, 128}});
    CHECK(texture.GetTexel(0.25, 0.5) == Color{10.0, 20.0, 30.0});
    CHECK(texture.GetTexel(0.75, 0.5) == Color{255.0, 0.0, 128.0});
}

TEST_CASE("Texture::GetTexel: the origin is the top-left corner, u runs right and v runs down",
          "[texture]") {
    const Texture texture = AddressTexture(4, 2);
    CHECK(texture.GetTexel(0.0, 0.0) == TexelOf(0, 0));  // the first texel of the image
    CHECK(texture.GetTexel(0.9, 0.1) == TexelOf(3, 0));  // top right
    CHECK(texture.GetTexel(0.1, 0.9) == TexelOf(0, 1));  // bottom left
    CHECK(texture.GetTexel(0.9, 0.9) == TexelOf(3, 1));  // the last texel of the image
}

TEST_CASE("Texture::GetTexel: u and v times the size, rounded down", "[texture]") {
    // Four texels across: the first from u = 0 to 0.25, the last from 0.75 to 1. Two
    // down: the upper from v = 0 to 0.5.
    const Texture texture = AddressTexture(4, 2);
    CHECK(texture.GetTexel(0.0, 0.25) == TexelOf(0, 0));
    CHECK(texture.GetTexel(0.24, 0.25) == TexelOf(0, 0));
    CHECK(texture.GetTexel(0.25, 0.25) == TexelOf(1, 0));  // a texel starts at its left edge
    CHECK(texture.GetTexel(0.49, 0.25) == TexelOf(1, 0));
    CHECK(texture.GetTexel(0.5, 0.25) == TexelOf(2, 0));
    CHECK(texture.GetTexel(0.74, 0.25) == TexelOf(2, 0));
    CHECK(texture.GetTexel(0.75, 0.25) == TexelOf(3, 0));
    CHECK(texture.GetTexel(0.99, 0.25) == TexelOf(3, 0));
    CHECK(texture.GetTexel(0.6, 0.49) == TexelOf(2, 0));
    CHECK(texture.GetTexel(0.6, 0.5) == TexelOf(2, 1));  // and at its top edge
}

TEST_CASE("Texture::GetTexel: every point of a texel has the texel's color", "[texture]") {
    // Nearest neighbor filtering: nothing is blended, and how near a point is to the
    // next texel does not show.
    const Texture texture = AddressTexture(4, 4);
    for (int ty = 0; ty < 4; ++ty) {
        for (int tx = 0; tx < 4; ++tx) {
            for (const double du : {0.01, 0.5, 0.99}) {
                for (const double dv : {0.01, 0.5, 0.99}) {
                    INFO("texel (" << tx << ", " << ty << ") at (" << du << ", " << dv << ")");
                    CHECK(texture.GetTexel((tx + du) / 4.0, (ty + dv) / 4.0) == TexelOf(tx, ty));
                }
            }
        }
    }
}

TEST_CASE("Texture::GetTexel: the right and bottom edges belong to the last texel", "[texture]") {
    // u = 1 times the width is one column past the last.
    const Texture texture = AddressTexture(4, 2);
    CHECK(texture.GetTexel(1.0, 0.25) == TexelOf(3, 0));
    CHECK(texture.GetTexel(0.1, 1.0) == TexelOf(0, 1));
    CHECK(texture.GetTexel(1.0, 1.0) == TexelOf(3, 1));
}

TEST_CASE("Texture::GetTexel: a coordinate beyond an edge is taken to the edge", "[texture]") {
    const Texture texture = AddressTexture(4, 2);
    // A rounding error outside, as an interpolated coordinate can be.
    CHECK(texture.GetTexel(-1e-17, 0.25) == TexelOf(0, 0));
    CHECK(texture.GetTexel(1.0000000000000002, 0.25) == TexelOf(3, 0));
    CHECK(texture.GetTexel(0.6, -1e-17) == TexelOf(2, 0));
    CHECK(texture.GetTexel(0.6, 1.0000000000000002) == TexelOf(2, 1));
    // And far outside: nothing repeats and nothing wraps around.
    CHECK(texture.GetTexel(-0.3, 0.25) == TexelOf(0, 0));
    CHECK(texture.GetTexel(1.3, 0.25) == TexelOf(3, 0));
    CHECK(texture.GetTexel(2.1, 0.25) == TexelOf(3, 0));
    CHECK(texture.GetTexel(0.6, -5.0) == TexelOf(2, 0));
    CHECK(texture.GetTexel(0.6, 5.0) == TexelOf(2, 1));
    CHECK(texture.GetTexel(-1e300, 1e300) == TexelOf(0, 1));
    const double infinity = std::numeric_limits<double>::infinity();
    CHECK(texture.GetTexel(infinity, -infinity) == TexelOf(3, 0));
}

TEST_CASE("Texture::GetTexel: the coordinates do not depend on the size of the texture",
          "[texture]") {
    // The same picture at two sizes: four texels, and sixteen in which every one of the
    // four is repeated 2 x 2. A point (u, v) shows the same color on both.
    const std::vector<std::uint8_t> colors = {255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 0};
    const Texture small(Image{.width = 2, .height = 2, .rgb = colors});
    Image doubled{.width = 4, .height = 4};
    for (int ty = 0; ty < 4; ++ty) {
        for (int tx = 0; tx < 4; ++tx) {
            const auto from = static_cast<std::size_t>(((ty / 2) * 2 + tx / 2) * 3);
            doubled.rgb.insert(doubled.rgb.end(),
                               {colors[from], colors[from + 1], colors[from + 2]});
        }
    }
    const Texture large(std::move(doubled));
    for (const double u : {0.0, 0.2, 0.45, 0.55, 0.8, 1.0}) {
        for (const double v : {0.0, 0.3, 0.49, 0.51, 0.7, 1.0}) {
            INFO("(u, v) = (" << u << ", " << v << ")");
            CHECK(small.GetTexel(u, v) == large.GetTexel(u, v));
        }
    }
    CHECK(small.GetTexel(0.8, 0.2) == Color{0.0, 255.0, 0.0});  // the top-right quarter
}

TEST_CASE("Texture::GetTexel: a texture of one texel has one color everywhere", "[texture]") {
    const Texture texture(Image{.width = 1, .height = 1, .rgb = {7, 8, 9}});
    for (const double u : {-1.0, 0.0, 0.5, 1.0, 2.0}) {
        for (const double v : {-1.0, 0.0, 0.5, 1.0, 2.0}) {
            CHECK(texture.GetTexel(u, v) == Color{7.0, 8.0, 9.0});
        }
    }
}

// --- Bilinear filtering --------------------------------------------------------------------

TEST_CASE("Texture::GetTexel: nearest neighbor filtering is the filter unless another is given",
          "[texture]") {
    const Texture texture = AddressTexture(4, 2, 50);
    for (const double u : {0.0, 0.1, 0.3, 0.55, 0.9, 1.0}) {
        for (const double v : {0.0, 0.2, 0.6, 1.0}) {
            INFO("(u, v) = (" << u << ", " << v << ")");
            CHECK(texture.GetTexel(u, v) ==
                  texture.GetTexel(u, v, TextureFilter::kNearestNeighbor));
        }
    }
    // And bilinear filtering is another answer: tx = 1.2, a fifth of the way from the
    // second texel to the third.
    CHECK(texture.GetTexel(0.3, 0.0) == TexelOf(1, 0, 50));
    CHECK(texture.GetTexel(0.3, 0.0, TextureFilter::kBilinear) != TexelOf(1, 0, 50));
}

TEST_CASE("Texture::GetTexel, bilinear: at whole tx and ty the color is the texel's own",
          "[texture]") {
    // No fraction, fx = fy = 0: the top-left of the four has all the weight, and it is
    // the texel nearest neighbor filtering returns there.
    const Texture texture = AddressTexture(4, 4, 60);
    for (int ty = 0; ty < 4; ++ty) {
        for (int tx = 0; tx < 4; ++tx) {
            INFO("texel (" << tx << ", " << ty << ")");
            const Color at_texel = texture.GetTexel(tx / 4.0, ty / 4.0, TextureFilter::kBilinear);
            CHECK(at_texel == TexelOf(tx, ty, 60));
            CHECK(at_texel == texture.GetTexel(tx / 4.0, ty / 4.0));
        }
    }
}

TEST_CASE("Texture::GetTexel, bilinear: between two texels the color is between them, by fx",
          "[texture]") {
    // Two texels side by side, so tx = 2u, and one row, so nothing is blended up and down.
    const Texture texture(Image{.width = 2, .height = 1, .rgb = {0, 0, 0, 200, 100, 40}});
    const auto bilinear = [&](double u) {
        return texture.GetTexel(u, 0.5, TextureFilter::kBilinear);
    };
    CHECK(bilinear(0.0) == Color{0.0, 0.0, 0.0});        // tx = 0: the left texel
    CHECK(bilinear(0.125) == Color{50.0, 25.0, 10.0});   // fx = 1/4: a quarter of the right one
    CHECK(bilinear(0.25) == Color{100.0, 50.0, 20.0});   // fx = 1/2: half of each
    CHECK(bilinear(0.375) == Color{150.0, 75.0, 30.0});  // fx = 3/4: the weight of TR is fx,
                                                         // not 1 - fx
    CHECK(bilinear(0.5) == Color{200.0, 100.0, 40.0});   // tx = 1: the right texel
}

TEST_CASE("Texture::GetTexel, bilinear: a ramp where nearest neighbor filtering has a step",
          "[texture]") {
    // The same two texels in one channel, 0 and 200, at sixteen equal steps of u. Nearest
    // neighbor filtering shows the left texel for half of them and jumps. Bilinear
    // filtering climbs by equal amounts from the place of the left texel, tx = 0, to
    // that of the right one, tx = 1, which is u = 0.5: it treats a texel as the point at
    // the top-left corner of its square, so the ramp lies over the left texel's square.
    // The right texel has no neighbour to blend with and keeps its color to the edge.
    const Texture texture(Image{.width = 2, .height = 1, .rgb = {0, 0, 0, 200, 200, 200}});
    for (int i = 0; i <= 16; ++i) {
        const double u = i / 16.0;
        INFO("u = " << u);
        CHECK(texture.GetTexel(u, 0.0).r == (i < 8 ? 0.0 : 200.0));
        CHECK(texture.GetTexel(u, 0.0, TextureFilter::kBilinear).r == (i < 8 ? 25.0 * i : 200.0));
    }
}

TEST_CASE("Texture::GetTexel, bilinear: four texels, interpolated across and then down",
          "[texture]") {
    //     TL = (0, 0, 0)     TR = (160, 0, 0)
    //     BL = (0, 80, 0)    BR = (160, 80, 240)
    const Texture texture(
        Image{.width = 2, .height = 2, .rgb = {0, 0, 0, 160, 0, 0, 0, 80, 0, 160, 80, 240}});

    // (u, v) = (1/8, 3/8) is (tx, ty) = (1/4, 3/4): fx = 1/4 and fy = 3/4.
    //     CT = 3/4 TL + 1/4 TR = (40, 0, 0)
    //     CB = 3/4 BL + 1/4 BR = (40, 80, 60)
    //     C  = 1/4 CT + 3/4 CB = (40, 60, 45)
    CHECK(texture.GetTexel(0.125, 0.375, TextureFilter::kBilinear) == Color{40.0, 60.0, 45.0});

    // Halfway between all four is their mean.
    CHECK(texture.GetTexel(0.25, 0.25, TextureFilter::kBilinear) == Color{80.0, 40.0, 60.0});

    // Along the top row, fy = 0, only the top two count, and down the left column,
    // fx = 0, only the left two.
    CHECK(texture.GetTexel(0.25, 0.0, TextureFilter::kBilinear) == Color{80.0, 0.0, 0.0});
    CHECK(texture.GetTexel(0.0, 0.25, TextureFilter::kBilinear) == Color{0.0, 40.0, 0.0});

    // u and v are not exchanged: the same fractions the other way round.
    CHECK(texture.GetTexel(0.375, 0.125, TextureFilter::kBilinear) == Color{120.0, 20.0, 45.0});
}

TEST_CASE(
    "Texture::GetTexel, bilinear: colors that rise evenly with the column and the row are "
    "found in between",
    "[texture]") {
    // The texel in column tx of row ty is (32 tx, 32 ty, 200), so the blend of the four
    // around a point is (32 tx, 32 ty, 200) for the point's own tx and ty: the picture
    // of the coordinates, without the steps. Every value here is a multiple of 1/8 texel,
    // so it comes out exactly.
    const Texture texture = AddressTexture(8, 4, 32);
    for (int i = 0; i <= 56; ++i) {      // tx = i / 8, up to the last column
        for (int j = 0; j <= 24; ++j) {  // ty = j / 8, up to the last row
            const double tx = i / 8.0;
            const double ty = j / 8.0;
            INFO("(tx, ty) = (" << tx << ", " << ty << ")");
            CHECK(texture.GetTexel(tx / 8.0, ty / 4.0, TextureFilter::kBilinear) ==
                  TexelOf(tx, ty, 32));
        }
    }
    // Nearest neighbor filtering has the texel of the whole parts there.
    CHECK(texture.GetTexel(2.75 / 8.0, 1.5 / 4.0) == TexelOf(2, 1, 32));
}

TEST_CASE(
    "Texture::GetTexel, bilinear: the last column and the last row stand in for the ones "
    "beyond them",
    "[texture]") {
    // From tx = 3, the place of the last of four columns, to the right edge the blend
    // would need a fifth column. The last takes its place: the color is the last
    // column's own all the way, as under nearest neighbor filtering, and nothing of the
    // first column, the opposite edge, shows.
    const Texture texture = AddressTexture(4, 2, 50);
    for (const double u : {0.75, 0.8, 0.9, 0.99, 1.0}) {
        INFO("u = " << u);
        CHECK(texture.GetTexel(u, 0.0, TextureFilter::kBilinear) == TexelOf(3, 0, 50));
    }
    for (const double v : {0.5, 0.6, 0.99, 1.0}) {
        INFO("v = " << v);
        CHECK(texture.GetTexel(0.0, v, TextureFilter::kBilinear) == TexelOf(0, 1, 50));
    }
    CHECK(texture.GetTexel(1.0, 1.0, TextureFilter::kBilinear) == TexelOf(3, 1, 50));
    CHECK(texture.GetTexel(0.9, 0.9, TextureFilter::kBilinear) == TexelOf(3, 1, 50));

    // Just before it the last two columns are still blended: tx = 2.5.
    CHECK(texture.GetTexel(0.625, 0.0, TextureFilter::kBilinear) == TexelOf(2.5, 0, 50));
    // And in the last column the rows still are: ty = 0.5.
    CHECK(texture.GetTexel(1.0, 0.25, TextureFilter::kBilinear) == TexelOf(3, 0.5, 50));
}

TEST_CASE("Texture::GetTexel, bilinear: a coordinate beyond an edge is taken to the edge",
          "[texture]") {
    const Texture texture = AddressTexture(4, 2, 50);
    const auto bilinear = [&](double u, double v) {
        return texture.GetTexel(u, v, TextureFilter::kBilinear);
    };
    // A rounding error outside, as an interpolated coordinate can be.
    CHECK(bilinear(-1e-17, 0.0) == TexelOf(0, 0, 50));
    CHECK(bilinear(1.0000000000000002, 0.0) == TexelOf(3, 0, 50));
    CHECK(bilinear(0.5, -1e-17) == TexelOf(2, 0, 50));
    CHECK(bilinear(0.5, 1.0000000000000002) == TexelOf(2, 1, 50));
    // And far outside: nothing repeats and nothing wraps around.
    CHECK(bilinear(-0.3, 0.0) == TexelOf(0, 0, 50));
    CHECK(bilinear(1.3, 0.0) == TexelOf(3, 0, 50));
    CHECK(bilinear(2.1, 0.0) == TexelOf(3, 0, 50));
    CHECK(bilinear(0.5, -5.0) == TexelOf(2, 0, 50));
    CHECK(bilinear(0.5, 5.0) == TexelOf(2, 1, 50));
    CHECK(bilinear(-1e300, 1e300) == TexelOf(0, 1, 50));
    const double infinity = std::numeric_limits<double>::infinity();
    CHECK(bilinear(infinity, -infinity) == TexelOf(3, 0, 50));
    // The other coordinate is still blended: beyond the left edge, halfway down.
    CHECK(bilinear(-0.3, 0.25) == TexelOf(0, 0.5, 50));
}

TEST_CASE("Texture::GetTexel, bilinear: texels of one color blend to exactly that color",
          "[texture]") {
    // Written out as (1 - f) a + f b, a blend of 255 and 255 comes out as
    // 254.99999999999997 for about one fraction in twenty-six, and the canvas would
    // truncate that to 254: a plain surface with a speckle. Fractions of every kind, on a
    // texture large enough to have some: sixteen texels of one color.
    Image image{.width = 4, .height = 4};
    for (int i = 0; i < 16; ++i) {
        image.rgb.insert(image.rgb.end(), {255, 128, 1});
    }
    const Texture plain(std::move(image));
    std::size_t off = 0;
    for (int i = 0; i <= 300; ++i) {
        for (int j = 0; j <= 300; ++j) {
            const double u = i / 299.0 - 0.001;
            const double v = j / 293.0 - 0.01;
            if (plain.GetTexel(u, v, TextureFilter::kBilinear) != Color{255.0, 128.0, 1.0}) {
                ++off;
            }
        }
    }
    CHECK(off == 0);
}

TEST_CASE("Texture::GetTexel, bilinear: a blend is never outside the texels it is made of",
          "[texture]") {
    // A checkerboard of 254 and 255: whatever the fractions, no channel leaves [254, 255].
    Image image{.width = 4, .height = 4};
    for (int ty = 0; ty < 4; ++ty) {
        for (int tx = 0; tx < 4; ++tx) {
            const auto level = static_cast<std::uint8_t>((tx + ty) % 2 == 0 ? 255 : 254);
            image.rgb.insert(image.rgb.end(), {level, level, level});
        }
    }
    const Texture checkerboard(std::move(image));
    std::size_t outside = 0;
    std::size_t in_between = 0;
    for (int i = 0; i <= 300; ++i) {
        for (int j = 0; j <= 300; ++j) {
            const Color color =
                checkerboard.GetTexel(i / 299.0, j / 293.0, TextureFilter::kBilinear);
            if (color.r < 254.0 || color.r > 255.0) {
                ++outside;
            }
            if (color.r > 254.0 && color.r < 255.0) {
                ++in_between;
            }
        }
    }
    CHECK(outside == 0);
    CHECK(in_between > 50000);  // and most of them are blends
}

TEST_CASE("Texture::GetTexel, bilinear: a texture of one texel has one color everywhere",
          "[texture]") {
    const Texture texture(Image{.width = 1, .height = 1, .rgb = {7, 8, 9}});
    for (const double u : {-1.0, 0.0, 0.5, 1.0, 2.0}) {
        for (const double v : {-1.0, 0.0, 0.5, 1.0, 2.0}) {
            CHECK(texture.GetTexel(u, v, TextureFilter::kBilinear) == Color{7.0, 8.0, 9.0});
        }
    }
}
