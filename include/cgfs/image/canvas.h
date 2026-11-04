#pragma once

#include <cgfs/image/color.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <vector>

namespace cgfs {

/// A pixel buffer with a centered coordinate system.
///
/// (0, 0) is the middle of the canvas, x grows to the right and y grows upward:
///
///     -width/2 <= x < width/2   and   -height/2 <= y < height/2   (integer division)
///
/// Pixels are stored as 8-bit RGB, top row first. Colors are clamped to [0, 255] and
/// truncated on the way in (see ToRgb8).
class Canvas {
public:
    /// Throws std::invalid_argument when width or height is not positive.
    explicit Canvas(int width = 1920, int height = 1080, Color background = colors::kWhite);

    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }
    [[nodiscard]] Color background() const noexcept { return background_; }

    /// Fill the whole canvas with the background color.
    void Clear();
    /// Make `color` the background color, then fill the whole canvas with it.
    void Clear(Color color);

    /// Set the pixel at centered coordinates (x, y). Out-of-range pixels are ignored.
    void PutPixel(int x, int y, Color color);
    /// The stored pixel at centered coordinates (x, y), or nullopt when out of range.
    [[nodiscard]] std::optional<Rgb8> GetPixel(int x, int y) const;

    /// The raw pixel bytes: row-major, top row first, RGB, no padding.
    [[nodiscard]] std::span<const std::uint8_t> Data() const noexcept { return pixels_; }

    /// Write the canvas to an image file. ".ppm" and ".png" are supported.
    /// Throws std::runtime_error for any other extension or on I/O failure.
    void Save(const std::filesystem::path& path) const;

private:
    /// Byte offset of pixel (x, y) in pixels_, or nullopt when out of range.
    [[nodiscard]] std::optional<std::size_t> Offset(int x, int y) const noexcept;

    int width_;
    int height_;
    Color background_;
    std::vector<std::uint8_t> pixels_;
};

}  // namespace cgfs
