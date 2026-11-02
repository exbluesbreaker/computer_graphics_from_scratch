#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace cgfs {

/// An RGB image as raw bytes: row-major, top row first, 3 bytes per pixel, no padding.
struct Image {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgb;
};

/// Write `rgb` as a binary PPM (P6, maxval 255).
///
/// Throws std::invalid_argument when the dimensions do not match the buffer size and
/// std::runtime_error on I/O failure.
void WritePpm(const std::filesystem::path& path, int width, int height,
              std::span<const std::uint8_t> rgb);

/// Write `rgb` as a PNG (via stb_image_write). Same error behaviour as WritePpm.
void WritePng(const std::filesystem::path& path, int width, int height,
              std::span<const std::uint8_t> rgb);

/// Read a binary PPM (P6, maxval 255), the format of the golden test images.
///
/// Comments in the header are not supported. Throws std::runtime_error when the file
/// cannot be opened or is not a P6 image this reader understands.
[[nodiscard]] Image ReadPpm(const std::filesystem::path& path);

}  // namespace cgfs
