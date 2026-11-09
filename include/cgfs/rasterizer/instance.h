#pragma once

#include <cgfs/camera/viewport.h>
#include <cgfs/image/canvas.h>
#include <cgfs/math/vec3.h>
#include <cgfs/rasterizer/model.h>

#include <cstddef>
#include <span>

namespace cgfs {

/// One appearance of a model in the scene: which model, and where it stands.
///
/// `model` is an index into the scene's model list, the way a Triangle's corners are
/// indices into the vertex list: an instance does not hold a copy of its model, it names
/// it, so the model is stored once however many instances show it. `position` is where
/// the model's origin lands in the camera's coordinates; every vertex moves with it,
/// which is all the "transform" there is at this stage (scale and rotation come with the
/// transforms chapter). The book's `instance { model = cube, position = (...) }`.
struct Instance {
    std::size_t model = 0;
    Vec3 position;
};

/// Draw the wireframe of one instance: `models[instance.model]` with every vertex moved
/// by `instance.position`; the book's RenderInstance.
///
/// Every vertex V of the model becomes V' = V + position, in the camera's coordinates,
/// and the moved vertices are drawn with the model's triangles by RenderObject, which
/// projects each vertex once and outlines each triangle. The model is not changed, so the
/// next instance of it starts from the same vertices.
///
/// Preconditions: `instance.model < models.size()`, asserted; every moved vertex has
/// z > 0, asserted by ProjectVertex, so an instance must stand entirely in front of the
/// camera until the clipping step. No hidden-surface removal, as for RenderObject.
void RenderInstance(Canvas& canvas, const Viewport& viewport, const Instance& instance,
                    std::span<const Model> models);

}  // namespace cgfs
