#pragma once

#include <cgfs/camera/viewport.h>
#include <cgfs/image/canvas.h>
#include <cgfs/image/color.h>
#include <cgfs/image/depth_buffer.h>
#include <cgfs/image/texture.h>
#include <cgfs/math/vec3.h>
#include <cgfs/rasterizer/triangle.h>

#include <array>
#include <cstddef>
#include <span>

namespace cgfs {

/// One triangle of an object: the indices of its three corners in the object's vertex
/// list, its color, and an intensity, a normal and texture coordinates at each corner.
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
///
/// `normals[k]` is the normal of the surface at the corner `v[k]`: the direction the
/// surface that the triangle stands for faces there, in the coordinates of the vertices.
/// A model may state it at any length; TransformModel brings it to unit length when it
/// places the model. A triangle is flat and has one normal of its own (TriangleNormal), but
/// the triangles of a sphere stand for a curved surface, which faces a different way at
/// each of a triangle's corners: away from the sphere's center. Lighting computed with
/// those normals shades the triangles as the surface they approximate (see shading.h).
/// The normal belongs to the corner of the triangle and not to the vertex, like `h` and
/// as in the book's model: at a corner of a cube three faces meet, and each faces its
/// own way there. The zero vector, the default, says that the corner has no normal of
/// its own and the triangle's is used, which is right for every flat face: a cube says
/// nothing. The drawing that does not light (RenderObject with a triangle primitive)
/// does not read the normals.
///
/// `uvs[k]` is the point of the model's texture that the corner `v[k]` shows (see Uv and
/// Texture): the book's `uvs`. The three of them pin the triangle onto the texture, and
/// every point of the triangle shows the point of the texture that lies between them in
/// the same proportions. They belong to the corner of the triangle and not to the
/// vertex, like the normals: the vertex at a corner of a crate is the top-right corner
/// of the picture on one face and the top-left corner of the picture on the next. They
/// default to the texture's origin, and nothing reads them unless the model has a
/// texture (see Model), in which case the texture is drawn in place of `color`.
struct Triangle {
    std::array<std::size_t, 3> v{};
    Color color;
    std::array<double, 3> h{1.0, 1.0, 1.0};
    std::array<Vec3, 3> normals{};
    std::array<Uv, 3> uvs{};

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
/// The triangles are drawn in order. Without a `depth_buffer`, where two of them cover
/// the same pixels the later one shows: a wireframe shows the edges at the back too,
/// and filled or shaded triangles come out right only if the list happens to run from
/// the back of the object to its front. With one, every triangle is drawn through it
/// (see DrawFilledTriangle), the projected vertices bringing their 1/z along, and a
/// pixel shows the nearest triangle whatever the order: hidden-surface removal. The
/// buffer is the caller's, like the canvas, because objects hide each other only
/// through a buffer they share: one buffer per picture, passed to every call that draws
/// into it. Nothing is clipped here; ClipScene does that before the object arrives.
///
/// Preconditions: every index is less than `vertices.size()`, asserted; every vertex has
/// z > 0, asserted by ProjectVertex; the depth buffer, if there is one, has the canvas's
/// size, asserted by the triangles.
void RenderObject(Canvas& canvas, const Viewport& viewport, std::span<const Vec3> vertices,
                  std::span<const Triangle> triangles,
                  DrawTriangleFunction draw_triangle = DrawWireframeTriangle,
                  DepthBuffer* depth_buffer = nullptr);

}  // namespace cgfs
