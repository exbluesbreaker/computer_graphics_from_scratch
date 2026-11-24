#pragma once

#include <cgfs/math/plane.h>
#include <cgfs/math/vec3.h>
#include <cgfs/rasterizer/model.h>
#include <cgfs/rasterizer/object.h>

#include <optional>
#include <span>
#include <vector>

namespace cgfs {

// Clipping: what is left of a scene inside the clipping volume.
//
// The clipping volume is the part of space the camera can draw, bounded by planes whose
// normals point into it (MakeClippingPlanes gives the five of a viewport). Clipping is a
// 3D operation: it takes the geometry of the scene and returns new geometry, the
// intersection of the scene with the volume. It therefore happens after the scene has
// been placed, on vertices that have been through the model and camera transforms, and
// before the perspective projection, which cannot handle a vertex behind the camera at
// all.
//
// The volume is an intersection of half-spaces, so clipping against it is clipping
// against one plane after another, and each plane is handled at three scales, coarse to
// fine: a whole instance by its bounding sphere, and only if that cannot decide, its
// triangles one by one, each of which is kept, dropped or cut.
//
// An "instance" here is the book's at this stage of the pipeline: no longer a model
// index with a transform (Instance) but the geometry itself, a Model whose vertices and
// bounding sphere are in the camera's coordinates, as TransformModel returns it. A
// "scene" is a list of those.

/// Clip one triangle against one plane and append what is left of it, no triangle, one
/// or two, to `clipped_triangles`; the book's ClipTriangle, which returns that list.
///
/// `vertices` is the vertex list the triangle's indices refer to. With d0, d1 and d2 the
/// signed distances of its corners to the plane (positive in front):
///
/// - none of them negative: the triangle is in front of the plane and is appended as it
///   is.
/// - none of them positive: it is behind the plane and nothing is appended.
/// - exactly one positive, the corner A: the part in front is the tip of the triangle
///   at A. With B and C the other corners, B' where the side AB crosses the plane and
///   C' where AC does, the triangle A B' C' is appended.
/// - exactly two positive, A and B, the corner C behind: the part in front is a
///   quadrilateral. With A' where AC crosses the plane and B' where BC does, it is
///   appended as the two triangles A B A' and A' B B'.
///
/// A corner exactly on the plane (distance 0) counts as neither in front nor behind, so
/// a triangle that only touches the plane is kept or dropped whole rather than cut into
/// a triangle and a sliver without area.
///
/// The crossing points are new vertices, appended to `vertices`; the triangles appended
/// name them by index, like any other. A crossing is always computed from the corner in
/// front toward the corner behind (IntersectSegmentPlane), so two triangles that share
/// the cut side compute the same point bit for bit and still meet there. The same
/// fraction t gives the new corner its intensity, hA + t (hB - hA), and its normal,
/// NA + t (NB - NA), which is no normal where neither corner had one, and is left as
/// long as the interpolation makes it (see shading.h). The color is the triangle's. A, B and C are
/// the triangle's corners rotated, never swapped, so every piece keeps the winding of the triangle
/// it was cut from.
///
/// The corners that were cut off stay in `vertices`, named by no triangle any more;
/// ClipInstanceAgainstPlane removes them. Precondition: the triangle's indices are less
/// than `vertices.size()`, asserted.
void ClipTriangle(const Triangle& triangle, const Plane& plane, std::vector<Vec3>& vertices,
                  std::vector<Triangle>& clipped_triangles);

/// Clip a list of triangles against one plane: ClipTriangle for each, in order, the
/// results concatenated; the book's ClipTrianglesAgainstPlane. The new vertices are
/// appended to `vertices`, after the ones that were there.
[[nodiscard]] std::vector<Triangle> ClipTrianglesAgainstPlane(std::span<const Triangle> triangles,
                                                              const Plane& plane,
                                                              std::vector<Vec3>& vertices);

/// Clip an instance against one plane; the book's ClipInstanceAgainstPlane. With d the
/// signed distance from the plane to the center of the instance's bounding sphere and r
/// the sphere's radius:
///
/// - d > r: the sphere, and the instance in it, is entirely in front of the plane. The
///   instance is returned as it is, without a look at its triangles.
/// - d < -r: it is entirely behind the plane. Nothing is returned.
/// - otherwise the sphere straddles the plane, and the instance is returned with its
///   triangles clipped by ClipTrianglesAgainstPlane. It may turn out to have none left,
///   when the sphere reached across the plane but the triangles did not; it is still
///   returned, an instance that draws nothing.
///
/// In the third case the vertices no triangle names any more, the corners that were cut
/// off, are removed and the triangles renumbered. Every vertex of the result is then in
/// front of the plane or on it (to within the rounding of the crossing points), which is
/// what lets a clipped instance be projected: the book's own demo leaves the cut-off
/// vertices in the list and projects them to no effect, but ProjectVertex's precondition
/// here covers every vertex in the list. The bounding sphere is kept as it is: what is
/// left of the instance is still inside it.
///
/// The sphere is trusted: the first two cases do not check the triangles. Precondition:
/// |plane.normal| = 1, so that d is a distance and can be compared with r.
[[nodiscard]] std::optional<Model> ClipInstanceAgainstPlane(Model instance, const Plane& plane);

/// Clip an instance against every plane in turn, each plane working on what the one
/// before left; the book's ClipInstance. Nothing is returned as soon as one plane
/// leaves nothing, and the later planes are not consulted. With no planes the instance
/// is returned as it is.
[[nodiscard]] std::optional<Model> ClipInstance(Model instance, std::span<const Plane> planes);

/// Clip a scene: every instance clipped by ClipInstance against `planes`, in order, the
/// ones of which nothing is left removed; the book's ClipScene. The result is the
/// intersection of the scene with the clipping volume, ready to be projected. With no
/// planes it is the scene itself.
[[nodiscard]] std::vector<Model> ClipScene(std::vector<Model> scene, std::span<const Plane> planes);

}  // namespace cgfs
