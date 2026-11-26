#include <cgfs/image/image_io.h>
#include <cgfs/image/texture.h>
#include <cgfs/rasterizer/mipmap_level.h>
#include <cgfs/rasterizer/point.h>

#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "support/address_texture.h"
#include "support/test_helpers.h"

using cgfs::Image;
using cgfs::MipmapLevel;
using cgfs::Mipmapping;
using cgfs::Point;
using cgfs::Texture;
using cgfs_test::AddressTexture;

namespace {

// A texture of `width` x `height` texels with all its mipmap levels. What is in it does
// not matter here: a level is chosen by sizes alone.
Texture Mipmapped(int width, int height) {
    const auto bytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3;
    return Texture(Image{.width = width, .height = height, .rgb = std::vector<std::uint8_t>(bytes)},
                   Mipmapping::kOn);
}

// The point (x, y) of the canvas showing the point (u, v) of a texture.
Point Showing(int x, int y, double u, double v) {
    Point p{x, y};
    p.uv = {u, v};
    return p;
}

// The level for half a square that is `side` pixels along each side and shows the whole
// texture: the right triangle with the texture's top-left, top-right and bottom-left
// corners at its three corners.
int LevelOfSquare(int side, const Texture& texture) {
    return MipmapLevel(Showing(0, 0, 0.0, 0.0), Showing(side, 0, 1.0, 0.0),
                       Showing(0, side, 0.0, 1.0), texture);
}

}  // namespace

TEST_CASE("MipmapLevel: a texture without mipmapping is drawn as itself at any size",
          "[rasterizer][mipmap]") {
    const Texture texture = AddressTexture(64, 64);
    REQUIRE(texture.level_count() == 1);
    for (const int side : {1, 4, 64, 500}) {
        CHECK(LevelOfSquare(side, texture) == 0);
    }
    // A triangle without area, too.
    CHECK(MipmapLevel(Showing(0, 0, 0.0, 0.0), Showing(3, 3, 1.0, 0.0), Showing(6, 6, 0.0, 1.0),
                      texture) == 0);
}

TEST_CASE("MipmapLevel: a square half as wide as its texture gets the half-size copy",
          "[rasterizer][mipmap]") {
    // The book's example: 16 texels on 8 pixels, two texels to a pixel.
    const Texture texture = Mipmapped(16, 16);
    REQUIRE(texture.level_count() == 5);
    CHECK(LevelOfSquare(16, texture) == 0);  // a texel to a pixel: the texture itself
    CHECK(LevelOfSquare(8, texture) == 1);   // half the width: the copy at half the size
    CHECK(LevelOfSquare(4, texture) == 2);   // a quarter
    CHECK(LevelOfSquare(2, texture) == 3);   // an eighth
    CHECK(LevelOfSquare(1, texture) == 4);   // one pixel: the copy of one texel
}

TEST_CASE("MipmapLevel: a square 10 times smaller than its texture gets the copy 8 times smaller",
          "[rasterizer][mipmap]") {
    // The book's other example. 160 texels on 16 pixels: the copies are 2, 4, 8 and 16
    // times smaller, and 8 is the nearest to 10.
    const Texture texture = Mipmapped(160, 160);
    REQUIRE(texture.level_count() == 8);  // 160, 80, 40, 20, 10, 5, 2, 1
    CHECK(LevelOfSquare(16, texture) == 3);
}

TEST_CASE("MipmapLevel: a texture drawn at its own size or larger is drawn as itself",
          "[rasterizer][mipmap]") {
    const Texture texture = Mipmapped(64, 64);
    for (const int side : {64, 65, 100, 128, 640, 2000}) {
        INFO("side " << side);
        CHECK(LevelOfSquare(side, texture) == 0);
    }
}

TEST_CASE("MipmapLevel: the border between two levels is halfway between their scales, as a ratio",
          "[rasterizer][mipmap]") {
    // 256 texels on n pixels are s = 256 / n texels to a pixel. Level k suits s = 2^k, and
    // the border between k and k + 1 is s = 2^(k + 1/2): 1.414, 2.828, 5.657, 11.31.
    const Texture texture = Mipmapped(256, 256);
    CHECK(LevelOfSquare(182, texture) == 0);  // s = 1.407
    CHECK(LevelOfSquare(181, texture) == 1);  // s = 1.414, just past the border
    CHECK(LevelOfSquare(128, texture) == 1);  // s = 2, what level 1 is for
    CHECK(LevelOfSquare(91, texture) == 1);   // s = 2.813
    CHECK(LevelOfSquare(90, texture) == 2);   // s = 2.844
    CHECK(LevelOfSquare(64, texture) == 2);   // s = 4
    CHECK(LevelOfSquare(46, texture) == 2);   // s = 5.565
    CHECK(LevelOfSquare(45, texture) == 3);   // s = 5.689
    CHECK(LevelOfSquare(23, texture) == 3);   // s = 11.13
    CHECK(LevelOfSquare(22, texture) == 4);   // s = 11.64
}

TEST_CASE("MipmapLevel: a triangle drawn smaller than the last level suits gets the last",
          "[rasterizer][mipmap]") {
    const Texture texture = Mipmapped(8, 8);
    REQUIRE(texture.level_count() == 4);
    CHECK(LevelOfSquare(1, texture) == 3);
    // Four times its 8 texels on one pixel: there is no smaller copy than one texel.
    CHECK(MipmapLevel(Showing(0, 0, 0.0, 0.0), Showing(1, 0, 4.0, 0.0), Showing(0, 1, 0.0, 4.0),
                      texture) == 3);
}

TEST_CASE("MipmapLevel: the level goes by texels, whatever the size of the texture",
          "[rasterizer][mipmap]") {
    // The same square with the same picture at three sizes: the larger the texture, the
    // smaller it is drawn, and each gets the copy that has about 16 texels across.
    CHECK(LevelOfSquare(16, Mipmapped(16, 16)) == 0);
    CHECK(LevelOfSquare(16, Mipmapped(32, 32)) == 1);
    CHECK(LevelOfSquare(16, Mipmapped(64, 64)) == 2);
    CHECK(LevelOfSquare(16, Mipmapped(512, 512)) == 5);
    // And a part of a texture is as many texels as it is: a quarter of 64 texels across
    // on 16 pixels is a texel to a pixel.
    const Texture texture = Mipmapped(64, 64);
    CHECK(MipmapLevel(Showing(0, 0, 0.5, 0.5), Showing(16, 0, 0.75, 0.5), Showing(0, 16, 0.5, 0.75),
                      texture) == 0);
}

TEST_CASE("MipmapLevel: the direction in which the texture is drawn smallest decides",
          "[rasterizer][mipmap]") {
    // A floor tile far away: 64 x 64 texels on 64 pixels across and 4 up. Across, a texel
    // to a pixel; up, 16 texels to a pixel, and it is those the pixels would skip.
    const Texture texture = Mipmapped(64, 64);
    REQUIRE(texture.level_count() == 7);
    CHECK(MipmapLevel(Showing(0, 0, 0.0, 0.0), Showing(64, 0, 1.0, 0.0), Showing(0, 4, 0.0, 1.0),
                      texture) == 4);
    // The other way round: 16 texels to a pixel across.
    CHECK(MipmapLevel(Showing(0, 0, 0.0, 0.0), Showing(4, 0, 1.0, 0.0), Showing(0, 64, 0.0, 1.0),
                      texture) == 4);
    // The mean of the two directions, 4 texels to a pixel, would have said level 2.
    CHECK(LevelOfSquare(16, texture) == 2);

    // The small direction need not lie along a side or along a row of pixels. Here the
    // texture's u runs along the canvas's diagonal (32, 32), 45 pixels for 64 texels,
    // and its v along the other diagonal, (-3, 3): 64 texels on 4.24 pixels, 15 to a
    // pixel.
    CHECK(MipmapLevel(Showing(0, 0, 0.0, 0.0), Showing(32, 32, 1.0, 0.0), Showing(-3, 3, 0.0, 1.0),
                      texture) == 4);
    // Nor is it what the rows alone say. Here the texture's 64 texels of v lie on 6 rows
    // of pixels, 10.7 to a row, which would be level 3. But the corner that shows the
    // bottom of the texture is pushed 64 pixels to the side, so going up a row also
    // slides 10.7 texels along u: a pixel step upward is (-10.7, 10.7) texels, 15.1.
    CHECK(MipmapLevel(Showing(0, 0, 0.0, 0.0), Showing(64, 0, 1.0, 0.0), Showing(64, 6, 0.0, 1.0),
                      texture) == 4);
    CHECK(MipmapLevel(Showing(0, 0, 0.0, 0.0), Showing(64, 0, 1.0, 0.0), Showing(0, 6, 0.0, 1.0),
                      texture) == 3);  // the same without the push
}

TEST_CASE("MipmapLevel: the vertex order does not matter", "[rasterizer][mipmap]") {
    const Texture texture = Mipmapped(256, 256);
    const Point a = Showing(-9, -7, 0.1, 0.9);
    const Point b = Showing(10, -2, 0.95, 0.7);
    const Point c = Showing(-1, 11, 0.4, 0.05);
    const int level = MipmapLevel(a, b, c, texture);
    CHECK(level == 4);  // about 12 texels to a pixel in the smallest direction
    CHECK(MipmapLevel(a, c, b, texture) == level);
    CHECK(MipmapLevel(b, a, c, texture) == level);
    CHECK(MipmapLevel(b, c, a, texture) == level);
    CHECK(MipmapLevel(c, a, b, texture) == level);
    CHECK(MipmapLevel(c, b, a, texture) == level);
}

TEST_CASE("MipmapLevel: a triangle exactly on a border gets the smaller copy",
          "[rasterizer][mipmap]") {
    // A pixel step along x is (1, -1) texels and one along y is (1, 1): 1.414 texels
    // whichever way, exactly the border between the levels 0 and 1.
    const Texture small = Mipmapped(4, 4);
    CHECK(MipmapLevel(Showing(0, 0, 0.25, 0.5), Showing(1, 0, 0.5, 0.25), Showing(0, 1, 0.5, 0.75),
                      small) == 1);

    // Tiles of a far floor, one pixel high, with the diagonal of a 512 x 512 texture
    // running up them: a pixel step upward is (256, -256) texels, 362.04, the border
    // between the last two levels. For the first the arithmetic lands on the border and
    // for the second a rounding error below it; both are on it.
    const Texture crate = Mipmapped(512, 512);
    REQUIRE(crate.level_count() == 10);
    CHECK(MipmapLevel(Showing(0, 0, 0.0, 1.0), Showing(8, 1, 1.0, 1.0), Showing(-8, 1, 0.0, 0.0),
                      crate) == 9);
    CHECK(MipmapLevel(Showing(0, 0, 0.0, 1.0), Showing(6, 1, 1.0, 1.0), Showing(-6, 1, 0.0, 0.0),
                      crate) == 9);
    // A tile two pixels high is well inside the level before.
    CHECK(MipmapLevel(Showing(0, 0, 0.0, 1.0), Showing(6, 2, 1.0, 1.0), Showing(-6, 2, 0.0, 0.0),
                      crate) == 8);
}

TEST_CASE("MipmapLevel: a triangle without area on the canvas gets the last level",
          "[rasterizer][mipmap]") {
    // Seen edge on: three corners on one line, with the whole texture between them.
    const Texture texture = Mipmapped(64, 64);
    CHECK(MipmapLevel(Showing(0, 0, 0.0, 0.0), Showing(5, 5, 1.0, 0.0), Showing(9, 9, 0.0, 1.0),
                      texture) == 6);
    // And all three on one pixel.
    CHECK(MipmapLevel(Showing(2, 2, 0.0, 0.0), Showing(2, 2, 1.0, 0.0), Showing(2, 2, 0.0, 1.0),
                      texture) == 6);
}

TEST_CASE("MipmapLevel: three corners that show one point of the texture get the texture itself",
          "[rasterizer][mipmap]") {
    // Nothing of the texture is spread over the triangle, so nothing is skipped.
    const Texture texture = Mipmapped(64, 64);
    CHECK(MipmapLevel(Showing(0, 0, 0.3, 0.6), Showing(20, 0, 0.3, 0.6), Showing(0, 20, 0.3, 0.6),
                      texture) == 0);
    // Even for a triangle without area.
    CHECK(MipmapLevel(Showing(0, 0, 0.3, 0.6), Showing(5, 5, 0.3, 0.6), Showing(9, 9, 0.3, 0.6),
                      texture) == 0);
    // A point is a Point of the texture's origin unless told otherwise.
    CHECK(MipmapLevel({0, 0}, {20, 0}, {0, 20}, texture) == 0);
}

TEST_CASE("MipmapLevel: a texture that is not square is measured in its own texels each way",
          "[rasterizer][mipmap]") {
    // 64 x 16 texels on a square of 16 pixels: 4 texels to a pixel across and 1 down.
    const Texture wide = Mipmapped(64, 16);
    REQUIRE(wide.level_count() == 7);  // 64 x 16, 32 x 8, 16 x 4, 8 x 2, 4 x 1, 2 x 1, 1 x 1
    CHECK(LevelOfSquare(16, wide) == 2);
    // On 64 pixels across and 16 down it is drawn at its own size.
    CHECK(MipmapLevel(Showing(0, 0, 0.0, 0.0), Showing(64, 0, 1.0, 0.0), Showing(0, 16, 0.0, 1.0),
                      wide) == 0);
}
