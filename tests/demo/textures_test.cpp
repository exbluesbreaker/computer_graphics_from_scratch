// The textures of the demo pictures are files in the source tree. These tests pin what
// is in them, so that a texture that was converted again, or replaced, does not change
// the textured pictures unnoticed.

#include <cgfs/demo/textures.h>
#include <cgfs/image/color.h>
#include <cgfs/image/texture.h>

#include <catch2/catch_test_macros.hpp>

#include "support/test_helpers.h"

using cgfs::Color;
using cgfs::Texture;

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
