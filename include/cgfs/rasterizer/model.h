#pragma once

#include <cgfs/camera/viewport.h>
#include <cgfs/image/canvas.h>
#include <cgfs/math/bounding_sphere.h>
#include <cgfs/math/mat4.h>
#include <cgfs/math/vec3.h>
#include <cgfs/rasterizer/object.h>

#include <vector>

namespace cgfs {

/// The shape of an object, described once: its vertices in the object's own coordinates
/// (model space, with the object's center at the origin) and the triangles that name
/// them by index; exactly the two lists RenderObject takes.
///
/// A model says what an object looks like, not where it is. Where it is, and how many
/// times it appears, is the business of Instance: a scene with twenty cubes stores the
/// cube's eight vertices and twelve triangles once, as one Model, and twenty Instances
/// of it. This is the book's `model { vertices, triangles }`.
///
/// `bounding_sphere` is a sphere around the vertices, in the same coordinates as they
/// are, the book's `bounds_center` and `bounds_radius`. Clipping reads it to decide about
/// the whole model at once (see BoundingSphere and ClipInstanceAgainstPlane). The default
/// is the sphere that contains everything, with which every triangle is examined;
/// `MakeBoundingSphere(vertices)` gives a model a real one.
struct Model {
    std::vector<Vec3> vertices;
    std::vector<Triangle> triangles;
    BoundingSphere bounding_sphere;

    bool operator==(const Model&) const = default;
};

/// The model under a transform, as a model of its own: every vertex V becomes
/// `transform * V`, the triangles are the same, and the bounding sphere moves along with
/// the vertices. Its center is transformed like a vertex, and its radius is multiplied
/// by the factor by which the transform scales lengths, the book's `radius * scale`.
///
/// That factor is read off the matrix: the first column of the matrix is where the unit
/// vector along X ends up (before the translation), so its length is how much longer a
/// length of 1 has become. The transforms of this pipeline are a rotation, a scaling
/// that is the same in every direction and a translation, and for those every direction
/// is scaled by that one factor: the instance's `scale`, whatever the rotation.
///
/// With the camera matrix times an instance's model matrix as the transform, the result
/// is the instance as geometry in the camera's coordinates, "placed in the scene" in the
/// book's words: what the clipping step works on, and what RenderObject projects.
///
/// Precondition: `transform` scales uniformly, as described; otherwise the sphere may
/// not contain the transformed vertices.
[[nodiscard]] Model TransformModel(const Model& model, const Mat4& transform);

/// Draw the wireframe of a model under a transform: the vertices of
/// `TransformModel(model, transform)` are drawn with the model's triangles by
/// RenderObject, which projects each of them with ProjectVertex and outlines each
/// triangle; the book's RenderModel. `transform` is the whole chain from the model's
/// coordinates to the camera's, usually the camera matrix times the instance's model
/// matrix (see RenderScene). The projection is not part of it, on purpose (see Mat4).
/// Nothing is clipped; RenderScene does that.
///
/// Preconditions: every transformed vertex has z > 0, asserted by ProjectVertex; every
/// triangle's indices are less than `model.vertices.size()`, asserted by RenderObject.
void RenderModel(Canvas& canvas, const Viewport& viewport, const Model& model,
                 const Mat4& transform);

}  // namespace cgfs
