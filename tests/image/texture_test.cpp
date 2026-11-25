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
