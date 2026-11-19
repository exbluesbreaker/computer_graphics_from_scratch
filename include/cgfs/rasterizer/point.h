#pragma once

namespace cgfs {

/// A point on the canvas, in the canvas's centered integer coordinates (see Canvas),
/// with an intensity for the shaded primitives and a depth for the depth-buffered ones.
///
/// The rasterizer works in 2D: everything it draws is made of these, and the 3D part
/// of the pipeline ends by producing them (see ProjectVertex in projection.h).
///
/// `h` is the book's shading value: a multiplier for the color at this vertex, 0 for
/// black and 1 for the color itself. DrawShadedTriangle interpolates it across the
/// triangle, exactly as it interpolates x; the other primitives ignore it. It defaults
/// to 1, so a `Point{x, y}` is "the full color" and the unshaded primitives never see a
/// difference. Values outside [0, 1] are not clamped here; like a light's intensity in
/// the ray tracer they are clamped by the canvas, see ToRgb8.
///
/// `inv_z` is 1/z of the vertex this point is the projection of, the one thing left of
/// its depth once it is a pixel: larger is nearer, and 0 is infinitely far (see
/// DepthBuffer for why it is 1/z and not z). ProjectVertex sets it, and the filled and
/// shaded triangles interpolate it like h when they are given a depth buffer; nothing
/// else reads it. It defaults to 0: a point made in 2D has no depth.
struct Point {
    int x = 0;
    int y = 0;
    double h = 1.0;
    double inv_z = 0.0;

    bool operator==(const Point&) const = default;
};

/// The pixel an interpolated coordinate falls in: truncated toward zero, the book's
/// convention and the same rule ToRgb8 applies to colors. -0.5 and 0.5 both land on
/// pixel 0, so the staircase of a line mirrors at the axes. Every primitive converts its
/// interpolated coordinates through this one function.
[[nodiscard]] constexpr int ToPixel(double coordinate) { return static_cast<int>(coordinate); }

}  // namespace cgfs
