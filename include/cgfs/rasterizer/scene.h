#pragma once

#include <cgfs/camera/camera.h>
#include <cgfs/camera/viewport.h>
#include <cgfs/image/canvas.h>
#include <cgfs/rasterizer/instance.h>
#include <cgfs/rasterizer/model.h>

#include <span>

namespace cgfs {

/// Draw a scene as `camera` sees it: every instance in `instances`, in order; the book's
/// RenderScene:
///
///     M_camera = MakeCameraMatrix(camera.position, camera.orientation)
///     for each instance I:  RenderModel(models[I.model], M_camera * MakeModelMatrix(I))
///
/// One matrix per instance takes a vertex from the model's coordinates through the world
/// into the camera's; the perspective projection then happens vertex by vertex inside
/// RenderObject, kept out of the matrix on purpose (see Mat4). The default camera is at
/// the origin looking along +Z, so a scene drawn without one is drawn as before.
///
/// The rasterizer's scene is a list of models and a list of instances that name them by
/// index and say where they stand: the same shape as an object, a vertex list and a
/// triangle list that indexes it, one level up. It is the book's `scene.instances`, with
/// the models alongside because an Instance names its model rather than holding it, and
/// the camera beside them.
///
/// Instances are drawn in list order, so where two overlap the later one's edges show.
/// There is no hidden-surface removal and no clipping, as for RenderObject: every
/// instance must stand in front of the camera, in the camera's coordinates, which a
/// camera that moves into the scene can violate until the clipping step. Precondition:
/// every `instance.model` is less than `models.size()`, asserted.
void RenderScene(Canvas& canvas, const Viewport& viewport, std::span<const Model> models,
                 std::span<const Instance> instances, const Camera& camera = Camera{});

}  // namespace cgfs
