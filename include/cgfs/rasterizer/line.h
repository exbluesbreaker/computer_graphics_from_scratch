#pragma once

#include <cgfs/image/canvas.h>
#include <cgfs/image/color.h>
#include <cgfs/rasterizer/point.h>

namespace cgfs {

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
/// The interpolated coordinate is a double; the pixel it lands in is found with
/// ToPixel. Pixels outside the canvas are dropped by PutPixel, so a line may run off the
/// canvas; there is no clipping yet.
void DrawLine(Canvas& canvas, Point p0, Point p1, Color color);

}  // namespace cgfs
