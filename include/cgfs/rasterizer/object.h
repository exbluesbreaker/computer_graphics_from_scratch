#pragma once

#include <cgfs/camera/viewport.h>
#include <cgfs/image/canvas.h>
#include <cgfs/image/color.h>
#include <cgfs/math/vec3.h>
#include <cgfs/rasterizer/triangle.h>

#include <array>
#include <cstddef>
#include <span>

namespace cgfs {

/// One triangle of an object: the indices of its three corners in the object's vertex
/// list, its color, and an intensity at each corner.
///
/// A triangle does not hold its corners, it names them, so a vertex that several
/// triangles meet at (every vertex of a closed surface) is stored once and projected
/// once, and the triangles around it agree on where it lands. A cube is eight vertices
/// and twelve triangles, not thirty-six vertices.
///
/// `h[k]` is the intensity of the triangle at its corner `v[k]`, the `h` of the Point
/// that corner is drawn through (see Point): a multiplier for the color, 0 for black and
/// 1 for the color itself, which DrawShadedTriangle interpolates across the triangle and
/// the other primitives ignore. It belongs to the corner of the triangle rather than to
/// the vertex, so two triangles that meet at a vertex may be differently bright there:
/// the faces of a cube can each have their own shade. It defaults to 1 at every corner,
/// the full color, so a triangle written as `{{0, 1, 2}, color}` is what it was.
struct Triangle {
    std::array<std::size_t, 3> v{};
    Color color;
    std::array<double, 3> h{1.0, 1.0, 1.0};

    bool operator==(const Triangle&) const = default;
};

/// Draw an object made of triangles: `vertices` are its corners in 3D, `triangles` its
/// faces, each naming three of the corners by index.
///
/// First every vertex is projected onto the canvas with ProjectVertex through `viewport`,
/// into a list `projected` parallel to `vertices`; then every triangle is drawn with
/// `draw_triangle` through `projected[v[0]]`, `projected[v[1]]` and `projected[v[2]]`,
/// each with the triangle's intensity at that corner as its `h`, in the triangle's own
/// color: the book's RenderObject. `draw_triangle` is DrawWireframeTriangle unless the
/// caller says otherwise, the outline the book draws at this point; DrawFilledTriangle
/// fills the faces and DrawShadedTriangle shades them by the corners' intensities.
///
/// The triangles are drawn in order, so where two of them cover the same pixels the
/// later one shows. There is no hidden-surface removal yet: a wireframe shows the edges
/// at the back too, and filled or shaded triangles come out right only if the list
/// happens to run from the back of the object to its front. Nor is anything clipped
/// here; ClipScene does that before the object arrives.
///
/// Preconditions: every index is less than `vertices.size()`, asserted; every vertex has
/// z > 0, asserted by ProjectVertex.
void RenderObject(Canvas& canvas, const Viewport& viewport, std::span<const Vec3> vertices,
                  std::span<const Triangle> triangles,
                  DrawTriangleFunction draw_triangle = DrawWireframeTriangle);

}  // namespace cgfs
