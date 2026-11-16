#pragma once

#include <cgfs/camera/viewport.h>
#include <cgfs/math/plane.h>

#include <vector>

namespace cgfs {

/// The clipping volume of a viewport: the five planes that bound what the camera shows
/// through it, in the camera's coordinates (the eye at the origin looking along +Z),
/// each with its normal pointing into the volume. In order:
///
///     near    N = (0, 0, 1)                    D = -d
///     left    N = ( d, 0, W) / |( d, 0, W)|    D = 0
///     right   N = (-d, 0, W) / |(-d, 0, W)|    D = 0
///     bottom  N = (0,  d, H) / |(0,  d, H)|    D = 0
///     top     N = (0, -d, H) / |(0, -d, H)|    D = 0
///
/// for a viewport `w` x `h` at distance `d` on a canvas `cw` x `ch` pixels, with
/// W = (w / 2) (1 + 1 / cw) and H = (h / 2) (1 + 1 / ch): half the viewport's size and
/// half a pixel.
///
/// The near plane is the viewport's own plane, z = d: what is nearer than the viewport,
/// or behind the camera, cannot be projected onto it. Each of the other four passes
/// through the eye, which is why its D is 0, and through one side of a rectangle on the
/// viewport plane. The left one holds the points with x / z = -W / d, the rays through
/// the rectangle's left side; written as d x + W z = 0 its normal is (d, 0, W), and the
/// points in view are those where that expression is positive. The other three are the
/// same plane mirrored and turned. With W = H = d, a field of view of 90 degrees, the
/// normals are the book's (±1 / sqrt(2), 0, 1 / sqrt(2)) and (0, ±1 / sqrt(2), 1 / sqrt(2)).
///
/// The rectangle is the viewport's and half a pixel more on every side, for two
/// reasons, both to do with ToPixel, which truncates a projected coordinate toward zero
/// (see ViewportToCanvas). First, the canvas shows a little more than the viewport: its
/// leftmost column collects what projects up to one pixel to the left of the viewport's
/// left side, and its bottom row what projects up to one pixel below it, so a volume cut
/// exactly at the viewport's sides would leave that column and that row unpainted.
/// Second, a triangle cut by a side plane gets new corners that project exactly onto the
/// plane's line on the canvas. At the viewport's own sides that line is the border
/// between two columns or rows of pixels, and the last bit of the arithmetic would
/// decide on which side of it each corner falls. Half a pixel further out the line runs
/// through the middle of a column or a row, the first one outside the canvas at the top
/// and at the right and the outermost one of the canvas at the bottom and at the left,
/// and every corner cut there lands in it for certain.
///
/// A clipping volume may always be larger than what is shown, the book's own is far
/// larger: whatever it lets through beyond the canvas, PutPixel drops.
///
/// There is no far plane: nothing is too far away to be projected.
///
/// A vertex in front of all five has z >= d > 0 and projects onto the canvas or onto
/// the row or column next to it, so a scene clipped against them meets ProjectVertex's
/// precondition.
[[nodiscard]] std::vector<Plane> MakeClippingPlanes(const Viewport& viewport);

}  // namespace cgfs
