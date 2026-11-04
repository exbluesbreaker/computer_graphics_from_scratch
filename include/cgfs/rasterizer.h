#pragma once

#include <cgfs/canvas.h>
#include <cgfs/color.h>

#include <vector>

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

/// Linear interpolation of a dependent variable d over an integer range of an
/// independent variable i: the values d takes at i0, i0 + 1, ..., i1 when it is d0 at i0
/// and d1 at i1.
///
/// Returns i1 - i0 + 1 values, so values[i - i0] is d at i. The slope
/// a = (d1 - d0) / (i1 - i0) is "how much d changes per unit of i"; starting from d0,
/// every step adds a to a running value, as in the book's listing. That running sum is
/// the whole secret of the rasterizer: a line is one pixel per column (or row) with the
/// other coordinate interpolated this way, and triangles and shading reuse the same
/// function for edges and intensities.
///
/// When i0 == i1 the range is a single point and the result is {d0}; d1 is ignored,
/// which also avoids the division by zero.
///
/// Precondition: i0 <= i1. The callers sort their endpoints first. With i0 > i1 the
/// result is empty (the loop "for i = i0 to i1" runs zero times).
[[nodiscard]] std::vector<double> Interpolate(int i0, double d0, int i1, double d1);

/// Draw the straight segment from p0 to p1, both endpoints included, in `color`.
///
/// The line is treated as a function of whichever coordinate changes more. For a
/// horizontal-ish line (|x1 - x0| > |y1 - y0|) x is the independent variable: the
/// endpoints are swapped if needed so that x0 < x1, y is interpolated over x0..x1 and
/// one pixel is set per column. For a vertical-ish line (including exact diagonals) the
/// roles of x and y are exchanged and one pixel is set per row. Either way consecutive
/// pixels are at most one step apart in the other coordinate, so the line has no gaps,
/// and the swap means a line looks the same whichever endpoint comes first.
///
/// The interpolated coordinate is a double; the pixel it lands in is found by
/// truncation toward zero, as the book's code does (and as ToRgb8 does for colors).
/// Pixels outside the canvas are dropped by PutPixel, so a line may run off the canvas;
/// there is no clipping yet.
void DrawLine(Canvas& canvas, Point p0, Point p1, Color color);

}  // namespace cgfs
