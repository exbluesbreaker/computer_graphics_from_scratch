#pragma once

#include <cgfs/image/canvas.h>
#include <cgfs/image/color.h>
#include <cgfs/rasterizer/point.h>

namespace cgfs {

/// Draw the outline of the triangle p0 p1 p2: its three edges, with DrawLine.
void DrawWireframeTriangle(Canvas& canvas, Point p0, Point p1, Point p2, Color color);

/// Fill the triangle p0 p1 p2 with `color`, edges included.
///
/// A triangle is a stack of horizontal segments, one per row from its lowest vertex to
/// its highest, and each row needs only its leftmost and rightmost x. The vertices are
/// sorted by y so that y0 <= y1 <= y2; then the "long" side p0 p2 spans every row, and
/// the two "short" sides p0 p1 and p1 p2 together span the same rows. Interpolate gives
/// the x of each side at every row; the short sides are concatenated (dropping the row
/// of p1, which both contain) into one list the length of the long side's. Which list is
/// the left one is decided by comparing the two at the middle row, since the sides do
/// not cross. Finally every row is filled from the left x to the right x; both are
/// converted to pixels with ToPixel first, the same rule as DrawLine, so a filled
/// triangle's vertical-ish edges land on exactly the pixels the wireframe draws.
///
/// The vertex order does not matter. Degenerate triangles whose vertices are collinear
/// come out as (part of) a line. No clipping, as for DrawLine.
void DrawFilledTriangle(Canvas& canvas, Point p0, Point p1, Point p2, Color color);

/// Fill the triangle p0 p1 p2 with `color` scaled by an intensity that varies smoothly
/// over it: each vertex has its own intensity `h` (see Point), and every pixel gets
/// `color * h` with `h` interpolated linearly from the vertices.
///
/// The fill proceeds exactly as in DrawFilledTriangle, and h travels along with x:
/// where the sides give an x per row they give an h per row too, from the same
/// Interpolate call pattern over the same rows, and the short sides' h lists are
/// concatenated like their x lists. Each row then runs a second interpolation, of h
/// over the row's pixels from the left side's h to the right side's, and multiplies the
/// color by it pixel by pixel. Interpolating the vertex values along the sides and
/// then along every row is the same thing as interpolating them over the triangle's
/// plane, which is what "shading" will mean once the vertices carry lighting.
///
/// The pixels covered are exactly those DrawFilledTriangle covers for the same three
/// points, so a shaded triangle with h = 1 at every vertex is the filled triangle. Two
/// shaded triangles that share an edge agree on its pixels' intensities, because both
/// interpolate the edge from the same two vertices. The product `color * h` is clamped
/// only when it reaches the canvas. The vertex order does not matter; no clipping.
void DrawShadedTriangle(Canvas& canvas, Point p0, Point p1, Point p2, Color color);

}  // namespace cgfs
