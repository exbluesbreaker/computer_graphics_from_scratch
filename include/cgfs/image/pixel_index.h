#pragma once

#include <cstddef>
#include <optional>

namespace cgfs {

/// Where the pixel (x, y) of a grid of `width` x `height` pixels is kept, when the grid
/// is stored row by row with the top row first and (x, y) are centered coordinates:
/// (0, 0) the middle of the grid, x growing to the right and y upward.
///
///     column = width / 2 + x,   row = height / 2 - 1 - y,   index = row * width + column
///
/// The origin sits at column width/2 and row height/2 - 1, so that y = 0 is the lowest
/// row of the upper half. Returns nullopt when the pixel is outside the grid.
///
/// This is the one definition of the canvas's coordinate system. Everything that keeps
/// a value per pixel, the Canvas its colors and the DepthBuffer its depths, finds the
/// pixel through it, so they cannot disagree about where a pixel is or whether it exists.
[[nodiscard]] constexpr std::optional<std::size_t> PixelIndex(int width, int height, int x,
                                                              int y) noexcept {
    const int column = width / 2 + x;
    const int row = height / 2 - 1 - y;
    if (column < 0 || column >= width || row < 0 || row >= height) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(row) * static_cast<std::size_t>(width) +
           static_cast<std::size_t>(column);
}

}  // namespace cgfs
