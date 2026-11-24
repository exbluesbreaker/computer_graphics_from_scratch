#pragma once

#include <cgfs/camera/viewport.h>
#include <cgfs/image/canvas.h>
#include <cgfs/image/color.h>
#include <cgfs/image/depth_buffer.h>
#include <cgfs/lighting/light.h>
#include <cgfs/rasterizer/point.h>

#include <span>

namespace cgfs {

/// Draw the outline of the triangle p0 p1 p2: its three edges, with DrawLine.
///
/// `depth_buffer` is there because the three triangle primitives share a signature (see
/// DrawTriangleFunction) and is ignored: an outline is drawn whole, it neither hides
/// anything nor is hidden.
void DrawWireframeTriangle(Canvas& canvas, Point p0, Point p1, Point p2, Color color,
                           DepthBuffer* depth_buffer = nullptr);

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
///
/// With a `depth_buffer` the triangle is drawn only where it is the nearest thing so
/// far, which is hidden-surface removal. Each vertex says how deep it is by its `inv_z`,
/// 1/z (see Point), and that travels along with x: the sides get a 1/z per row from the
/// same Interpolate calls over the same rows, every row interpolates it from its left
/// end to its right end, and a pixel is set only if DepthBuffer::UpdateIfCloser accepts
/// the triangle's 1/z there; the buffer then remembers it. The pixels considered are
/// the same as without one.
///
/// It is 1/z that is interpolated, not z, because only 1/z changes by equal steps across
/// the picture. The triangle lies in a plane A X + B Y + C Z + D = 0, and its point
/// (X, Y, Z) is seen at x = X d / Z, y = Y d / Z on the viewport. Put X = x Z / d and
/// Y = y Z / d into the plane's equation and divide by Z D:
///
///     1/Z = -(A x + B y + C d) / (D d)
///
/// which is linear in x and y, the kind of quantity Interpolate's running sum follows
/// exactly. Z is the reciprocal of that and is not: halfway along the picture of an edge
/// from z = 1 to z = 4 the depth is 1.6, not 2.5.
///
/// Without a depth buffer every pixel of the triangle is set, whatever was there.
/// Precondition: the depth buffer, if there is one, has the canvas's size; asserted.
void DrawFilledTriangle(Canvas& canvas, Point p0, Point p1, Point p2, Color color,
                        DepthBuffer* depth_buffer = nullptr);

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
///
/// With a `depth_buffer` only the pixels where the triangle is the nearest thing so far
/// are set, exactly as in DrawFilledTriangle: 1/z is a third quantity that travels along
/// the sides and the rows with x and h. Without one every pixel is set. Precondition:
/// the depth buffer, if there is one, has the canvas's size; asserted.
void DrawShadedTriangle(Canvas& canvas, Point p0, Point p1, Point p2, Color color,
                        DepthBuffer* depth_buffer = nullptr);

/// A way to draw a triangle: the signature the three primitives above share. Whoever
/// draws many triangles (RenderObject, RenderScene) takes one of them as a parameter and
/// so draws its objects as wireframes, filled or shaded without knowing which, and
/// passes on the depth buffer it was given, or none.
using DrawTriangleFunction = void (*)(Canvas& canvas, Point p0, Point p1, Point p2, Color color,
                                      DepthBuffer* depth_buffer);

/// Fill the triangle p0 p1 p2 with `color` lit pixel by pixel: the primitive of Phong
/// shading.
///
/// DrawShadedTriangle is handed the light at the three vertices and spreads it over the
/// triangle, so it cannot show what the light does between them: a highlight in the
/// middle of a face, or a lamp close to it. Here every pixel computes its own light with
/// the illumination equation (ComputeIllumination), which asks three things of a pixel:
///
/// - The point P of the triangle that the pixel shows. 1/z is interpolated along the
///   sides and along every row, as for a depth buffer, and UnprojectVertex takes the
///   pixel and its 1/z back to the point in the camera's coordinates.
/// - The normal N of the surface there. Every vertex brings its normal (see Point), and
///   the three components of it are three more quantities that travel along the sides
///   and the rows like h: nx, ny and nz are interpolated one by one, put together again
///   at the pixel and normalized, a vector between two unit vectors being shorter than
///   either.
/// - The direction V from the point to the viewer. The camera is the origin of its own
///   coordinates, so V = -P.
///
/// The pixel is set to `color * ComputeIllumination(P, N, V, specular, lights)`.
/// `lights` are in the camera's coordinates, like the normals and like P; `specular` is
/// the exponent of the surface's highlight, kMatte for none; `viewport` is the one the
/// points were projected through.
///
/// With the same normal at all three vertices the surface is flat and is still lit
/// pixel by pixel, the directions to a point light and to the viewer changing across it.
/// With three different normals the triangle is shaded as a piece of the curved surface
/// they are the normals of. Either way the pixels covered are exactly those that
/// DrawFilledTriangle covers for the same three points. The vertex order does not
/// matter; no clipping.
///
/// With a `depth_buffer` only the pixels where the triangle is the nearest thing so far
/// are set, as in DrawFilledTriangle, and only those are lit, the lighting being the
/// expensive part. Without one every pixel is lit and set.
///
/// Preconditions: every point has `inv_z` > 0, as the projection of a vertex in front
/// of the camera has, asserted by UnprojectVertex; every point has a normal that is not
/// the zero vector, and no two of the normals are opposite, so that nothing between them
/// is zero either; the depth buffer, if there is one, has the canvas's size, asserted.
void DrawPhongTriangle(Canvas& canvas, const Viewport& viewport, Point p0, Point p1, Point p2,
                       Color color, double specular, std::span<const Light> lights,
                       DepthBuffer* depth_buffer = nullptr);

}  // namespace cgfs
