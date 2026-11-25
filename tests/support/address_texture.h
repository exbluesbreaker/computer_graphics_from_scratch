#pragma once

// A texture for the tests in which every texel says where it is, so that a pixel
// painted with it shows which texel it was painted with.

#include <cgfs/image/color.h>
#include <cgfs/image/image_io.h>
#include <cgfs/image/texture.h>

#include <cstdint>
#include <utility>

namespace cgfs_test {

/// A texture of `width` x `height` texels, at most 256 each way, in which the texel in
/// column tx of row ty, counted from the top-left corner, has the color (tx, ty, 200).
/// A pixel drawn with it at full intensity shows the column of its texel in its red
/// channel and the row in its green one, and its blue tells it from a black background.
inline cgfs::Texture AddressTexture(int width, int height) {
    cgfs::Image image{.width = width, .height = height};
    for (int ty = 0; ty < height; ++ty) {
        for (int tx = 0; tx < width; ++tx) {
            image.rgb.push_back(static_cast<std::uint8_t>(tx));
            image.rgb.push_back(static_cast<std::uint8_t>(ty));
            image.rgb.push_back(200);
        }
    }
    return cgfs::Texture(std::move(image));
}

/// The texel in column `tx` of row `ty` of an AddressTexture, as a color.
inline cgfs::Color TexelOf(int tx, int ty) {
    return {static_cast<double>(tx), static_cast<double>(ty), 200.0};
}

/// The same texel as a pixel shows it at full intensity.
inline cgfs::Rgb8 PixelOf(int tx, int ty) { return cgfs::ToRgb8(TexelOf(tx, ty)); }

}  // namespace cgfs_test
