#pragma once

// The texture of the book's example of what mipmapping is for: black and white texels
// in a checkerboard.

#include <cgfs/image/image_io.h>
#include <cgfs/image/texture.h>

#include <cstdint>
#include <utility>

namespace cgfs_test {

/// A texture of `size` x `size` texels, white and black in a checkerboard of single
/// texels, the top-left one white: the texel in column tx of row ty is white where
/// tx + ty is even. Any 2 x 2 texels of it are two of each, so with `mipmapping` on
/// every level after the first is one gray, 127.5.
inline cgfs::Texture FineCheckerboard(int size,
                                      cgfs::Mipmapping mipmapping = cgfs::Mipmapping::kOff) {
    cgfs::Image image{.width = size, .height = size, .rgb = {}};
    for (int ty = 0; ty < size; ++ty) {
        for (int tx = 0; tx < size; ++tx) {
            const auto level = static_cast<std::uint8_t>((tx + ty) % 2 == 0 ? 255 : 0);
            image.rgb.insert(image.rgb.end(), {level, level, level});
        }
    }
    return cgfs::Texture(std::move(image), mipmapping);
}

}  // namespace cgfs_test
