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
#include "support/checkerboard_texture.h"
#include "support/test_helpers.h"

using cgfs::Color;
using cgfs::Image;
using cgfs::Mipmapping;
using cgfs::Texture;
using cgfs::TextureFilter;
using cgfs::Uv;
using cgfs_test::AddressTexture;
using cgfs_test::FineCheckerboard;
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

// --- Mipmapping ----------------------------------------------------------------------------

namespace {

// The color of `texture` at the middle of the texel in column `tx` of row `ty` of
// `level`, a level that is `width` x `height` texels.
Color LevelTexel(const Texture& texture, int level, int width, int height, int tx, int ty) {
    return texture.GetTexel((tx + 0.5) / width, (ty + 0.5) / height,
                            TextureFilter::kNearestNeighbor, level);
}

}  // namespace

TEST_CASE("Texture: without mipmapping a texture has one level, itself", "[texture][mipmap]") {
    CHECK(AddressTexture(8, 8).level_count() == 1);
    CHECK(Texture(Image{.width = 1, .height = 1, .rgb = {1, 2, 3}}).level_count() == 1);
    const Texture said(Image{.width = 2, .height = 2, .rgb = std::vector<std::uint8_t>(12)},
                       Mipmapping::kOff);
    CHECK(said.level_count() == 1);
}

TEST_CASE("Texture: with mipmapping it is halved until one texel is left", "[texture][mipmap]") {
    const auto levels = [](int width, int height) {
        const auto bytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3;
        return Texture(
                   Image{.width = width, .height = height, .rgb = std::vector<std::uint8_t>(bytes)},
                   Mipmapping::kOn)
            .level_count();
    };
    CHECK(levels(1, 1) == 1);       // nothing to halve
    CHECK(levels(2, 2) == 2);       // 2, 1
    CHECK(levels(8, 8) == 4);       // 8, 4, 2, 1
    CHECK(levels(512, 512) == 10);  // the crate: 512, 256, 128, 64, 32, 16, 8, 4, 2, 1
    CHECK(levels(8, 2) == 4);       // 8 x 2, 4 x 1, 2 x 1, 1 x 1: until both are one
    CHECK(levels(1, 4) == 3);       // 1 x 4, 1 x 2, 1 x 1
    CHECK(levels(5, 3) == 3);       // 5 x 3, 2 x 1, 1 x 1: halves are rounded down
    CHECK(levels(7, 7) == 3);       // 7, 3, 1

    // The size and the texels of the texture itself are what they are without.
    const Texture plain = AddressTexture(8, 4, 32);
    Image image{.width = 8, .height = 4};
    for (int ty = 0; ty < 4; ++ty) {
        for (int tx = 0; tx < 8; ++tx) {
            image.rgb.insert(image.rgb.end(), {static_cast<std::uint8_t>(tx * 32),
                                               static_cast<std::uint8_t>(ty * 32), 200});
        }
    }
    const Texture mipmapped(std::move(image), Mipmapping::kOn);
    CHECK(mipmapped.width() == 8);
    CHECK(mipmapped.height() == 4);
    for (const double u : {0.0, 0.1, 0.33, 0.5, 0.77, 1.0}) {
        for (const double v : {0.0, 0.2, 0.6, 1.0}) {
            INFO("(u, v) = (" << u << ", " << v << ")");
            CHECK(mipmapped.GetTexel(u, v) == plain.GetTexel(u, v));
            CHECK(mipmapped.GetTexel(u, v, TextureFilter::kBilinear) ==
                  plain.GetTexel(u, v, TextureFilter::kBilinear));
            CHECK(mipmapped.GetTexel(u, v, TextureFilter::kNearestNeighbor, 0) ==
                  plain.GetTexel(u, v));
        }
    }
}

TEST_CASE("Texture: a texel of the next level is the mean of the four it covers",
          "[texture][mipmap]") {
    // 4 x 2 texels, one channel telling them apart:
    //
    //      0   10 |  40  80
    //      2   20 | 120 200
    //
    // The next level is 2 x 1: the mean of the left four and the mean of the right four.
    // Means are kept as they come out, fractions and all: no texel of a level is a byte.
    const Texture texture(
        Image{.width = 4, .height = 2, .rgb = {0, 0, 9, 10, 0, 9, 40,  0, 9, 80,  0, 9,
                                               2, 1, 9, 20, 1, 9, 120, 1, 9, 200, 3, 9}},
        Mipmapping::kOn);
    REQUIRE(texture.level_count() == 3);
    CHECK(LevelTexel(texture, 1, 2, 1, 0, 0) == Color{8.0, 0.5, 9.0});  // (0 + 10 + 2 + 20) / 4
    CHECK(LevelTexel(texture, 1, 2, 1, 1, 0) ==
          Color{110.0, 1.0, 9.0});  // (40 + 80 + 120 + 200) / 4
    // The last level is one texel. A level that is one texel high has no second row, and
    // its two texels count twice: the mean of the two, which is the mean of all eight.
    CHECK(LevelTexel(texture, 2, 1, 1, 0, 0) == Color{59.0, 0.75, 9.0});
}

TEST_CASE("Texture: a texel of level k is the mean of the 2^k x 2^k texels it covers, exactly",
          "[texture][mipmap]") {
    // The texel in column tx of row ty is (20 tx, 20 ty, 200), so the mean of a block of
    // texels is the color of the middle of the block, and every mean is exact.
    Image image{.width = 8, .height = 8};
    for (int ty = 0; ty < 8; ++ty) {
        for (int tx = 0; tx < 8; ++tx) {
            image.rgb.insert(image.rgb.end(), {static_cast<std::uint8_t>(tx * 20),
                                               static_cast<std::uint8_t>(ty * 20), 200});
        }
    }
    const Texture texture(std::move(image), Mipmapping::kOn);
    REQUIRE(texture.level_count() == 4);
    for (int level = 1; level < 4; ++level) {
        const int block = 1 << level;  // texels of the texture along the side of a texel
        const int size = 8 / block;    // texels of the level along its side
        for (int ty = 0; ty < size; ++ty) {
            for (int tx = 0; tx < size; ++tx) {
                INFO("level " << level << ", texel (" << tx << ", " << ty << ")");
                // The block covers the columns tx * block to tx * block + block - 1.
                const double middle_x = tx * block + (block - 1) / 2.0;
                const double middle_y = ty * block + (block - 1) / 2.0;
                CHECK(LevelTexel(texture, level, size, size, tx, ty) ==
                      TexelOf(middle_x, middle_y, 20));
            }
        }
    }
    // The last level is the mean color of the whole texture.
    CHECK(texture.GetTexel(0.3, 0.9, TextureFilter::kNearestNeighbor, 3) == TexelOf(3.5, 3.5, 20));
}

TEST_CASE("Texture: the levels of the book's checkerboard are gray", "[texture][mipmap]") {
    // Black and white texels in a checkerboard: every 2 x 2 texels are two of each, and
    // their mean is the gray halfway, 127.5. So is every level after the first.
    const Texture checkerboard = FineCheckerboard(8, Mipmapping::kOn);
    REQUIRE(checkerboard.level_count() == 4);
    CHECK(checkerboard.GetTexel(0.0, 0.0) == Color{255.0, 255.0, 255.0});
    CHECK(checkerboard.GetTexel(0.13, 0.0) == Color{0.0, 0.0, 0.0});
    for (int level = 1; level < 4; ++level) {
        for (const double u : {0.0, 0.2, 0.5, 0.7, 1.0}) {
            for (const double v : {0.0, 0.3, 0.6, 1.0}) {
                INFO("level " << level << ", (u, v) = (" << u << ", " << v << ")");
                CHECK(checkerboard.GetTexel(u, v, TextureFilter::kNearestNeighbor, level) ==
                      Color{127.5, 127.5, 127.5});
                CHECK(checkerboard.GetTexel(u, v, TextureFilter::kBilinear, level) ==
                      Color{127.5, 127.5, 127.5});
            }
        }
    }
}

TEST_CASE("Texture: (u, v) is the same point of the picture at every level", "[texture][mipmap]") {
    // A picture of 4 x 4 texels, and the same picture at twice the size, every texel
    // repeated 2 x 2. Halving the second gives the first back: its level 1 is the small
    // picture, at every (u, v).
    const Texture small = AddressTexture(4, 4, 50);
    Image doubled{.width = 8, .height = 8};
    for (int ty = 0; ty < 8; ++ty) {
        for (int tx = 0; tx < 8; ++tx) {
            doubled.rgb.insert(doubled.rgb.end(), {static_cast<std::uint8_t>((tx / 2) * 50),
                                                   static_cast<std::uint8_t>((ty / 2) * 50), 200});
        }
    }
    const Texture large(std::move(doubled), Mipmapping::kOn);
    for (int i = 0; i <= 40; ++i) {
        for (int j = 0; j <= 40; ++j) {
            const double u = i / 40.0;
            const double v = j / 40.0;
            INFO("(u, v) = (" << u << ", " << v << ")");
            CHECK(large.GetTexel(u, v, TextureFilter::kNearestNeighbor, 1) == small.GetTexel(u, v));
            CHECK(large.GetTexel(u, v, TextureFilter::kBilinear, 1) ==
                  small.GetTexel(u, v, TextureFilter::kBilinear));
        }
    }
}

TEST_CASE("Texture::GetTexel: a level is looked up as a texture of its own size",
          "[texture][mipmap]") {
    // Level 1 of an 8 x 8 texture has 4 texels across: its first from u = 0 to 0.25 and
    // its last from 0.75 to 1, where the texture itself has two texels each.
    Image image{.width = 8, .height = 8};
    for (int ty = 0; ty < 8; ++ty) {
        for (int tx = 0; tx < 8; ++tx) {
            image.rgb.insert(image.rgb.end(), {static_cast<std::uint8_t>(tx * 20),
                                               static_cast<std::uint8_t>(ty * 20), 200});
        }
    }
    const Texture texture(std::move(image), Mipmapping::kOn);
    const auto at_level_1 = [&](double u, double v, TextureFilter filter) {
        return texture.GetTexel(u, v, filter, 1);
    };
    // Nearest neighbor filtering: the texels of level 1 are (40 x + 10, 40 y + 10, 200).
    CHECK(at_level_1(0.0, 0.0, TextureFilter::kNearestNeighbor) == Color{10.0, 10.0, 200.0});
    CHECK(at_level_1(0.24, 0.0, TextureFilter::kNearestNeighbor) == Color{10.0, 10.0, 200.0});
    CHECK(at_level_1(0.25, 0.0, TextureFilter::kNearestNeighbor) == Color{50.0, 10.0, 200.0});
    CHECK(at_level_1(0.74, 0.3, TextureFilter::kNearestNeighbor) == Color{90.0, 50.0, 200.0});
    CHECK(at_level_1(1.0, 1.0, TextureFilter::kNearestNeighbor) == Color{130.0, 130.0, 200.0});
    CHECK(at_level_1(-3.0, 7.0, TextureFilter::kNearestNeighbor) == Color{10.0, 130.0, 200.0});

    // Bilinear filtering blends the texels of the level: tx = 4u, and halfway between
    // its first two texels, u = 1/8, is halfway between 10 and 50. From its last column
    // on, tx = 3, the color is that column's.
    CHECK(at_level_1(0.0, 0.0, TextureFilter::kBilinear) == Color{10.0, 10.0, 200.0});
    CHECK(at_level_1(0.125, 0.0, TextureFilter::kBilinear) == Color{30.0, 10.0, 200.0});
    CHECK(at_level_1(0.25, 0.375, TextureFilter::kBilinear) == Color{50.0, 70.0, 200.0});
    CHECK(at_level_1(0.75, 0.0, TextureFilter::kBilinear) == Color{130.0, 10.0, 200.0});
    CHECK(at_level_1(0.9, 0.0, TextureFilter::kBilinear) == Color{130.0, 10.0, 200.0});

    // The last level is one texel, and every point of the texture has its color.
    for (const double u : {-1.0, 0.0, 0.4, 1.0, 2.0}) {
        CHECK(texture.GetTexel(u, 0.6, TextureFilter::kNearestNeighbor, 3) ==
              Color{70.0, 70.0, 200.0});
        CHECK(texture.GetTexel(u, 0.6, TextureFilter::kBilinear, 3) == Color{70.0, 70.0, 200.0});
    }
}

TEST_CASE("Texture: an odd last column or row is part of no texel of the next level",
          "[texture][mipmap]") {
    // 5 x 3 texels, the last column and the last row loud:
    //
    //      10  20  30  40 | 250
    //      50  60  70  80 | 250
    //     ----------------+----
    //     250 250 250 250 | 250
    //
    // Half of 5 is 2 and half of 3 is 1: level 1 is 2 x 1, made of the first four columns
    // of the first two rows.
    const Texture texture(
        Image{.width = 5, .height = 3, .rgb = {10,  0, 0, 20,  0, 0, 30,  0, 0, 40,  0, 0,
                                               250, 0, 0, 50,  0, 0, 60,  0, 0, 70,  0, 0,
                                               80,  0, 0, 250, 0, 0, 250, 0, 0, 250, 0, 0,
                                               250, 0, 0, 250, 0, 0, 250, 0, 0}},
        Mipmapping::kOn);
    REQUIRE(texture.level_count() == 3);
    CHECK(LevelTexel(texture, 1, 2, 1, 0, 0) == Color{35.0, 0.0, 0.0});  // 10, 20, 50, 60
    CHECK(LevelTexel(texture, 1, 2, 1, 1, 0) == Color{55.0, 0.0, 0.0});  // 30, 40, 70, 80
    CHECK(LevelTexel(texture, 2, 1, 1, 0, 0) == Color{45.0, 0.0, 0.0});
}

TEST_CASE("Texture: a level one texel wide is halved along its length", "[texture][mipmap]") {
    // 1 x 4 texels: 0, 100, 40, 80 down the one column. Level 1 is 1 x 2, the means of
    // the pairs, and level 2 the mean of those.
    const Texture texture(
        Image{.width = 1, .height = 4, .rgb = {0, 0, 0, 100, 0, 0, 40, 0, 0, 80, 0, 0}},
        Mipmapping::kOn);
    REQUIRE(texture.level_count() == 3);
    CHECK(LevelTexel(texture, 1, 1, 2, 0, 0) == Color{50.0, 0.0, 0.0});
    CHECK(LevelTexel(texture, 1, 1, 2, 0, 1) == Color{60.0, 0.0, 0.0});
    CHECK(LevelTexel(texture, 2, 1, 1, 0, 0) == Color{55.0, 0.0, 0.0});
}

TEST_CASE("Texture: a texture of one color has that color at every level, exactly",
          "[texture][mipmap]") {
    Image image{.width = 16, .height = 16};
    for (int i = 0; i < 256; ++i) {
        image.rgb.insert(image.rgb.end(), {255, 128, 1});
    }
    const Texture plain(std::move(image), Mipmapping::kOn);
    REQUIRE(plain.level_count() == 5);
    for (int level = 0; level < 5; ++level) {
        for (int i = 0; i <= 30; ++i) {
            for (int j = 0; j <= 30; ++j) {
                INFO("level " << level);
                CHECK(plain.GetTexel(i / 29.0, j / 31.0, TextureFilter::kBilinear, level) ==
                      Color{255.0, 128.0, 1.0});
            }
        }
    }
}
