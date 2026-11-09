#pragma once

#include <cgfs/camera/viewport.h>
#include <cgfs/image/canvas.h>
#include <cgfs/rasterizer/instance.h>
#include <cgfs/rasterizer/model.h>

#include <span>

namespace cgfs {

/// Draw a scene: every instance in `instances`, in order, with RenderInstance; the book's
/// RenderScene.
///
/// The rasterizer's scene is a list of models and a list of instances that name them by
/// index and say where they stand: the same shape as an object, a vertex list and a
/// triangle list that indexes it, one level up. It is the book's `scene.instances`, with
/// the models alongside because an Instance names its model rather than holding it.
/// Nothing else yet; the camera joins when the scene gets one.
///
/// Instances are drawn in list order, so where two overlap the later one's edges show.
/// There is no hidden-surface removal and no clipping, as for RenderObject: every
/// instance must stand in front of the camera. Precondition: every `instance.model` is
/// less than `models.size()`, asserted by RenderInstance.
void RenderScene(Canvas& canvas, const Viewport& viewport, std::span<const Model> models,
                 std::span<const Instance> instances);

}  // namespace cgfs
