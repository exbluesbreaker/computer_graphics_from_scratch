#include <cgfs/math/mat4.h>
#include <cgfs/rasterizer/clipping.h>
#include <cgfs/rasterizer/object.h>
#include <cgfs/rasterizer/scene.h>

#include <cassert>
#include <utility>
#include <vector>

namespace cgfs {

void RenderScene(Canvas& canvas, const Viewport& viewport, std::span<const Model> models,
                 std::span<const Instance> instances, const Camera& camera,
                 DrawTriangleFunction draw_triangle, DepthBuffer* depth_buffer) {
    // The model and camera transforms: every instance becomes geometry in the camera's
    // coordinates.
    const Mat4 camera_matrix = MakeCameraMatrix(camera.position, camera.orientation);
    std::vector<Model> scene;
    scene.reserve(instances.size());
    for (const Instance& instance : instances) {
        assert(instance.model < models.size() && "RenderScene: a model index is out of range");
        scene.push_back(
            TransformModel(models[instance.model], camera_matrix * MakeModelMatrix(instance)));
    }

    // Clipping, in 3D: after the transforms, before the projection.
    const std::vector<Model> clipped_scene = ClipScene(std::move(scene), camera.clipping_planes);

    // The perspective projection and the triangles.
    for (const Model& instance : clipped_scene) {
        RenderObject(canvas, viewport, instance.vertices, instance.triangles, draw_triangle,
                     depth_buffer);
    }
}

}  // namespace cgfs
