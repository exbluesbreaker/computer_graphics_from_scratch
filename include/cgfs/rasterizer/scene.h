#pragma once

#include <cgfs/camera/camera.h>
#include <cgfs/camera/viewport.h>
#include <cgfs/image/canvas.h>
#include <cgfs/image/depth_buffer.h>
#include <cgfs/rasterizer/instance.h>
#include <cgfs/rasterizer/model.h>
#include <cgfs/rasterizer/triangle.h>

#include <span>

namespace cgfs {

/// Draw a scene as `camera` sees it: every instance in `instances`, in order; the book's
/// RenderScene, in three steps:
///
///     M_camera = MakeCameraMatrix(camera.position, camera.orientation)
///     scene    = for each instance I:
///                    TransformModel(models[I.model], M_camera * MakeModelMatrix(I))
///     clipped  = ClipScene(scene, camera.clipping_planes)
///     for each instance C of clipped:
///                    RenderObject(C.vertices, C.triangles, draw_triangle, depth_buffer)
///
/// First the transforms: one matrix per instance takes its vertices from the model's
/// coordinates through the world into the camera's, and the instance becomes geometry
/// placed in the scene. Then the clipping, which is done in 3D and so must come after
/// the transforms and before the projection: what the camera's clipping planes leave of
/// every instance (see ClipScene). Last the perspective projection, vertex by vertex
/// inside RenderObject and kept out of the matrix on purpose (see Mat4), which is what
/// leaves room for the middle step.
///
/// The default camera is at the origin looking along +Z and has no clipping planes, so
/// a scene drawn without one is drawn as before. `draw_triangle` is how every triangle
/// is drawn, an outline unless the caller asks for DrawFilledTriangle or
/// DrawShadedTriangle (see RenderObject).
///
/// The rasterizer's scene is a list of models and a list of instances that name them by
/// index and say where they stand: the same shape as an object, a vertex list and a
/// triangle list that indexes it, one level up. It is the book's `scene.instances`, with
/// the models alongside because an Instance names its model rather than holding it, and
/// the camera beside them.
///
/// Instances are drawn in list order. Without a `depth_buffer`, where two overlap the
/// later one shows, and a scene of solid faces must be listed from the back to the
/// front. With one, a buffer of the canvas's size that the caller makes for the picture,
/// every triangle of every instance is drawn through it and a pixel shows whatever is
/// nearest there, in any order: hidden-surface removal (see DepthBuffer).
///
/// Preconditions: every `instance.model` is less than `models.size()`, asserted; after
/// the clipping every vertex has z > 0, asserted by ProjectVertex. The planes of
/// MakeClippingPlanes guarantee the second for any scene; a camera without clipping
/// planes must have every instance entirely in front of it.
void RenderScene(Canvas& canvas, const Viewport& viewport, std::span<const Model> models,
                 std::span<const Instance> instances, const Camera& camera = Camera{},
                 DrawTriangleFunction draw_triangle = DrawWireframeTriangle,
                 DepthBuffer* depth_buffer = nullptr);

}  // namespace cgfs
