#pragma once

#include <cgfs/camera/viewport.h>
#include <cgfs/image/canvas.h>
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
struct Model {
    std::vector<Vec3> vertices;
    std::vector<Triangle> triangles;
};

/// Draw the wireframe of a model under a transform: every vertex V becomes
/// `transform * V`, and the transformed vertices are drawn with the model's triangles by
/// RenderObject, which projects each of them with ProjectVertex and outlines each
/// triangle; the book's RenderModel. `transform` is the whole chain from the model's
/// coordinates to the camera's, usually the camera matrix times the instance's model
/// matrix (see RenderScene). The projection is not part of it, on purpose (see Mat4).
///
/// Preconditions: every transformed vertex has z > 0, asserted by ProjectVertex; every
/// triangle's indices are less than `model.vertices.size()`, asserted by RenderObject.
void RenderModel(Canvas& canvas, const Viewport& viewport, const Model& model,
                 const Mat4& transform);

}  // namespace cgfs
