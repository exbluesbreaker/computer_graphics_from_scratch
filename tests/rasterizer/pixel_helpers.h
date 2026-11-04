#pragma once

// Helpers for the rasterizer tests: which pixels a drawing lit, and whole canvases as
// bytes for comparing two drawings.

#include <cgfs/image/canvas.h>
#include <cgfs/image/color.h>
#include <cgfs/rasterizer/point.h>

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <vector>

namespace cgfs_test {

/// Every pixel that differs from the background, sorted by x and then by y.
inline std::vector<cgfs::Point> LitPixels(const cgfs::Canvas& canvas) {
    const cgfs::Rgb8 background = cgfs::ToRgb8(canvas.background());
    std::vector<cgfs::Point> lit;
    for (int x = -canvas.width() / 2; x < canvas.width() / 2; ++x) {
        for (int y = -canvas.height() / 2; y < canvas.height() / 2; ++y) {
            if (canvas.GetPixel(x, y) != background) {
                lit.push_back({x, y});
            }
        }
    }
    return lit;
}

/// The raw pixel bytes of a canvas.
inline std::vector<std::uint8_t> Bytes(const cgfs::Canvas& canvas) {
    return {canvas.Data().begin(), canvas.Data().end()};
}

/// The pixels x_min..x_max of row y.
struct Span {
    int y;
    int x_min;
    int x_max;
};

/// The pixels of some horizontal spans, in the order LitPixels uses.
inline std::vector<cgfs::Point> PixelsOf(std::initializer_list<Span> spans) {
    std::vector<cgfs::Point> pixels;
    for (const Span& span : spans) {
        for (int x = span.x_min; x <= span.x_max; ++x) {
            pixels.push_back({x, span.y});
        }
    }
    std::sort(pixels.begin(), pixels.end(), [](cgfs::Point a, cgfs::Point b) {
        return a.x != b.x ? a.x < b.x : a.y < b.y;
    });
    return pixels;
}

}  // namespace cgfs_test
