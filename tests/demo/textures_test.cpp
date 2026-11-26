// The textures of the demo pictures are a file in the source tree and a loop. These
// tests pin what is in them, so that a texture that was converted again, or replaced,
// does not change the textured pictures unnoticed.

#include <cgfs/demo/textures.h>
#include <cgfs/image/color.h>
#include <cgfs/image/texture.h>

#include <catch2/catch_test_macros.hpp>
#include <cstddef>

#include "support/test_helpers.h"

using cgfs::Color;
using cgfs::Mipmapping;
using cgfs::Texture;
using cgfs::TextureFilter;

TEST_CASE("the crate texture is 512 x 512 texels", "[demo][texture]") {
    const Texture crate = cgfs::demo::CrateTexture();
    CHECK(crate.width() == 512);
    CHECK(crate.height() == 512);
}

TEST_CASE("the crate texture has the texels of its file", "[demo][texture]") {
    const Texture crate = cgfs::demo::CrateTexture();
    // The middle of the texel in column tx of row ty.
    const auto texel = [&](int tx, int ty) {
        return crate.GetTexel((tx + 0.5) / 512.0, (ty + 0.5) / 512.0);
    };
    CHECK(texel(0, 0) == Color{112.0, 93.0, 63.0});        // the top-left corner of the frame
    CHECK(texel(511, 0) == Color{100.0, 86.0, 47.0});      // top right
    CHECK(texel(0, 511) == Color{38.0, 20.0, 46.0});       // bottom left
    CHECK(texel(511, 511) == Color{34.0, 20.0, 0.0});      // bottom right
    CHECK(texel(256, 256) == Color{187.0, 173.0, 134.0});  // on the plank across the middle
    CHECK(texel(100, 100) == Color{181.0, 164.0, 121.0});
    CHECK(texel(400, 60) == Color{231.0, 196.0, 130.0});  // on the top board of the frame
    // And by the coordinates a model uses: its corners.
    CHECK(crate.GetTexel(0.0, 0.0) == texel(0, 0));
    CHECK(crate.GetTexel(1.0, 1.0) == texel(511, 511));
}

TEST_CASE("the crate texture has its mipmap levels when asked for them",
          "[demo][texture][mipmap]") {
    CHECK(cgfs::demo::CrateTexture().level_count() == 1);
    CHECK(cgfs::demo::CrateTexture(Mipmapping::kOff).level_count() == 1);
    const Texture crate = cgfs::demo::CrateTexture(Mipmapping::kOn);
    CHECK(crate.level_count() == 10);  // 512, 256, 128, 64, 32, 16, 8, 4, 2, 1
    CHECK(crate.width() == 512);
    // The texture itself is the file's, as without.
    CHECK(crate.GetTexel(0.0, 0.0) == Color{112.0, 93.0, 63.0});
    CHECK(crate.GetTexel(1.0, 1.0) == Color{34.0, 20.0, 0.0});

    // Its last level is one texel: the mean color of the crate, of all 262144 texels.
    // The sums are whole numbers and the division is by a power of two, so the two ways
    // of taking the mean agree to the last bit.
    Color sum;
    for (int ty = 0; ty < 512; ++ty) {
        for (int tx = 0; tx < 512; ++tx) {
            sum += crate.GetTexel((tx + 0.5) / 512.0, (ty + 0.5) / 512.0);
        }
    }
    const Color mean = sum * (1.0 / 262144.0);
    CHECK(crate.GetTexel(0.5, 0.5, TextureFilter::kNearestNeighbor, 9) == mean);
    CHECK(crate.GetTexel(0.0, 1.0, TextureFilter::kBilinear, 9) == mean);
    // A wood brown.
    CHECK(mean.r > mean.g);
    CHECK(mean.g > mean.b);
    CHECK(mean.r > 100.0);
    CHECK(mean.r < 180.0);
}

TEST_CASE("the checkerboard texture is 64 x 64 squares of 4 x 4 texels, black and white",
          "[demo][texture]") {
    const Texture checkerboard = cgfs::demo::CheckerboardTexture();
    CHECK(checkerboard.width() == 256);
    CHECK(checkerboard.height() == 256);
    CHECK(checkerboard.level_count() == 1);
    constexpr Color kWhite{255.0, 255.0, 255.0};
    constexpr Color kBlack{0.0, 0.0, 0.0};
    std::size_t white = 0;
    for (int ty = 0; ty < 256; ++ty) {
        for (int tx = 0; tx < 256; ++tx) {
            const Color texel = checkerboard.GetTexel((tx + 0.5) / 256.0, (ty + 0.5) / 256.0);
            const bool white_square = (tx / 4 + ty / 4) % 2 == 0;
            if (texel != (white_square ? kWhite : kBlack)) {
                FAIL("texel (" << tx << ", " << ty << ") is in the wrong color");
            }
            if (texel == kWhite) {
                ++white;
            }
        }
    }
    CHECK(white == 256 * 256 / 2);                     // half of them, as in the book
    CHECK(checkerboard.GetTexel(0.0, 0.0) == kWhite);  // the top-left square
    CHECK(checkerboard.GetTexel(1.0, 1.0) == kWhite);  // and the one diagonally across
    CHECK(checkerboard.GetTexel(1.0, 0.0) == kBlack);
}

TEST_CASE("the levels of the checkerboard texture are smaller checkerboards and then one gray",
          "[demo][texture][mipmap]") {
    const Texture checkerboard = cgfs::demo::CheckerboardTexture(Mipmapping::kOn);
    REQUIRE(checkerboard.level_count() == 9);  // 256, 128, 64, 32, 16, 8, 4, 2, 1
    constexpr Color kWhite{255.0, 255.0, 255.0};
    constexpr Color kBlack{0.0, 0.0, 0.0};
    constexpr Color kGray{127.5, 127.5, 127.5};
    // Squares of 2 x 2 texels at half the size and of single texels at a quarter: still
    // the same picture, a texel of the level lying inside one square.
    for (const int level : {1, 2}) {
        const int size = 256 >> level;
        const int square = 4 >> level;
        for (int ty = 0; ty < size; ++ty) {
            for (int tx = 0; tx < size; ++tx) {
                const Color texel = checkerboard.GetTexel((tx + 0.5) / size, (ty + 0.5) / size,
                                                          TextureFilter::kNearestNeighbor, level);
                const bool white_square = (tx / square + ty / square) % 2 == 0;
                if (texel != (white_square ? kWhite : kBlack)) {
                    FAIL("level " << level << ": texel (" << tx << ", " << ty
                                  << ") is in the wrong color");
                }
            }
        }
    }
    // From an eighth of the size on a texel covers as many black texels as white ones.
    for (int level = 3; level < 9; ++level) {
        for (const double u : {0.0, 0.21, 0.5, 0.83, 1.0}) {
            for (const double v : {0.0, 0.37, 0.5, 0.9, 1.0}) {
                INFO("level " << level << ", (u, v) = (" << u << ", " << v << ")");
                CHECK(checkerboard.GetTexel(u, v, TextureFilter::kNearestNeighbor, level) == kGray);
                CHECK(checkerboard.GetTexel(u, v, TextureFilter::kBilinear, level) == kGray);
            }
        }
    }
}
