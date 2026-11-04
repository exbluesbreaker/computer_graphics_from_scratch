#pragma once

namespace cgfs {

/// A point on the canvas, in the canvas's centered integer coordinates (see Canvas).
///
/// The rasterizer works in 2D: everything it draws is made of these, and the 3D part
/// of the pipeline (projection, which comes later) ends by producing them.
struct Point {
    int x = 0;
    int y = 0;

    bool operator==(const Point&) const = default;
};

/// The pixel an interpolated coordinate falls in: truncated toward zero, the book's
/// convention and the same rule ToRgb8 applies to colors. -0.5 and 0.5 both land on
/// pixel 0, so the staircase of a line mirrors at the axes. Every primitive converts its
/// interpolated coordinates through this one function.
[[nodiscard]] constexpr int ToPixel(double coordinate) { return static_cast<int>(coordinate); }

}  // namespace cgfs
