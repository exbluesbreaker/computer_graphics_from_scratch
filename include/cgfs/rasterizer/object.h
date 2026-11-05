#pragma once

#include <cgfs/camera/viewport.h>
#include <cgfs/image/canvas.h>
#include <cgfs/image/color.h>
#include <cgfs/math/vec3.h>

#include <array>
#include <cstddef>
#include <span>

namespace cgfs {

/// One triangle of an object: the indices of its three corners in the object's vertex
/// list, and its color.
///
/// A triangle does not hold its corners, it names them, so a vertex that several
/// triangles meet at (every vertex of a closed surface) is stored once and projected
/// once, and the triangles around it agree on where it lands. A cube is eight vertices
/// and twelve triangles, not thirty-six vertices.
struct Triangle {
    std::array<std::size_t, 3> v{};
    Color color;
};

/// Draw the wireframe of an object made of triangles: `vertices` are its corners in 3D,
/// `triangles` its faces, each naming three of the corners by index.
///
/// First every vertex is projected onto the canvas with ProjectVertex through `viewport`,
/// into a list `projected` parallel to `vertices`; then every triangle is drawn with
/// DrawWireframeTriangle through `projected[v[0]]`, `projected[v[1]]` and
/// `projected[v[2]]` in its own color, the book's RenderObject. The triangles are drawn
/// in order, so where two edges cover the same pixels the later triangle's color shows.
/// It is a wireframe: every edge is drawn, those at the back too, since there is no
/// hidden-surface removal yet; and there is no clipping, as for ProjectVertex.
///
/// Preconditions: every index is less than `vertices.size()`, asserted; every vertex has
/// z > 0, asserted by ProjectVertex.
void RenderObject(Canvas& canvas, const Viewport& viewport, std::span<const Vec3> vertices,
                  std::span<const Triangle> triangles);

}  // namespace cgfs
