#pragma once

// A texture for the tests in which every texel says where it is, so that a pixel
// painted with it shows which texel it was painted with.

#include <cgfs/image/color.h>
#include <cgfs/image/image_io.h>
#include <cgfs/image/texture.h>

#include <cstdint>
#include <utility>

namespace cgfs_test {

/// A texture of `width` x `height` texels in which the texel in column tx of row ty,
/// counted from the top-left corner, has the color (tx * step, ty * step, 200), no
/// channel past 255. A pixel drawn with it at full intensity shows the column of its
/// texel in its red channel and the row in its green one, and its blue tells it from a
/// black background.
///
/// With the step of 1 neighbouring texels are one level apart, and a blend of two is
/// truncated to one of them when it reaches a pixel. A larger step sets them far enough
/// apart for a blend to show, which the tests of bilinear filtering need, and for the
/// mean of a block of them to be told from any one of them, which the tests of
/// mipmapping need: with `mipmapping` on the texture has its smaller copies, and since
/// its colors rise evenly, a texel of a copy has the color of the middle of the block it
/// covers.
inline cgfs::Texture AddressTexture(int width, int height, int step = 1,
                                    cgfs::Mipmapping mipmapping = cgfs::Mipmapping::kOff) {
    cgfs::Image image{.width = width, .height = height};
    for (int ty = 0; ty < height; ++ty) {
        for (int tx = 0; tx < width; ++tx) {
            image.rgb.push_back(static_cast<std::uint8_t>(tx * step));
            image.rgb.push_back(static_cast<std::uint8_t>(ty * step));
            image.rgb.push_back(200);
        }
    }
    return cgfs::Texture(std::move(image), mipmapping);
}

/// The color of an AddressTexture with `step` at the place (`tx`, `ty`) among its texels:
/// for whole numbers the texel in that column and row. Its colors rise evenly with the
/// column and with the row, so blending the texels around a place in between by how near
/// each is, which is bilinear filtering, gives the color of this formula at that place.
inline cgfs::Color TexelOf(double tx, double ty, int step = 1) {
    return {tx * step, ty * step, 200.0};
}

/// The same color as a pixel shows it at full intensity.
inline cgfs::Rgb8 PixelOf(double tx, double ty, int step = 1) {
    return cgfs::ToRgb8(TexelOf(tx, ty, step));
}

}  // namespace cgfs_test
