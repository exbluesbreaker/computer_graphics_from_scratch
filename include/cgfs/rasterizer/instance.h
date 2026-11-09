#pragma once

#include <cgfs/camera/viewport.h>
#include <cgfs/image/canvas.h>
#include <cgfs/math/mat4.h>
#include <cgfs/math/vec3.h>
#include <cgfs/rasterizer/model.h>

#include <cstddef>
#include <span>

namespace cgfs {

/// One appearance of a model in the scene: which model, and its model transform, that is
/// where it stands, which way it is turned and how big it is.
///
/// `model` is an index into the scene's model list, the way a Triangle's corners are
/// indices into the vertex list: an instance does not hold a copy of its model, it names
/// it, so the model is stored once however many instances show it. The other fields are
/// the book's `transform { scale, rotation, translation }`: `scale` a factor about the
/// model's origin, `orientation` a rotation matrix (MakeOYRotationMatrix builds one) and
/// `position` the point of the world where the model's origin lands. The defaults, scale
/// 1 and the identity, place a model as it is, so `{model, position}` still means what it
/// meant before there were transforms.
struct Instance {
    std::size_t model = 0;
    Vec3 position;
    Mat4 orientation = Mat4::Identity();
    double scale = 1.0;
};

/// The model matrix of an instance, the book's M_model = I_T I_R I_S:
///
///     MakeTranslationMatrix(position) * orientation * MakeScalingMatrix(scale)
///
/// Applied to a vertex it scales first, then rotates, then translates, the book's
/// ApplyTransform: the instance grows and turns about its own origin, while it is still
/// in model space, and only then moves to its place in the world. Translating first
/// would make the rotation swing the instance around the world's origin instead. With
/// the default orientation and scale the matrix adds exactly `position` to a vertex.
[[nodiscard]] Mat4 MakeModelMatrix(const Instance& instance);

/// Draw the wireframe of one instance as seen from the origin:
/// `RenderModel(models[instance.model], MakeModelMatrix(instance))`, the book's
/// RenderInstance before the camera moves; RenderScene adds the camera. The model is not
/// changed, so the next instance of it starts from the same vertices.
///
/// Preconditions: `instance.model < models.size()`, asserted; every transformed vertex
/// has z > 0, asserted by ProjectVertex, so an instance must stand entirely in front of
/// the camera until the clipping step. No hidden-surface removal, as for RenderObject.
void RenderInstance(Canvas& canvas, const Viewport& viewport, const Instance& instance,
                    std::span<const Model> models);

}  // namespace cgfs
