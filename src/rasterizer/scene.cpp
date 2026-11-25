#include <cgfs/math/mat4.h>
#include <cgfs/math/vec3.h>
#include <cgfs/rasterizer/clipping.h>
#include <cgfs/rasterizer/culling.h>
#include <cgfs/rasterizer/object.h>
#include <cgfs/rasterizer/scene.h>
#include <cgfs/rasterizer/shading.h>

#include <cassert>
#include <utility>
#include <vector>

namespace cgfs {

namespace {

/// The scene as geometry in the camera's coordinates, ready to be projected: the first
/// three steps of RenderScene, which are the same with lights and without.
std::vector<Model> PlaceScene(std::span<const Model> models, std::span<const Instance> instances,
                              const Mat4& camera_matrix, const Camera& camera,
                              BackFaceCulling back_face_culling) {
    // The model and camera transforms: every instance becomes geometry in the camera's
    // coordinates.
    std::vector<Model> scene;
    scene.reserve(instances.size());
    for (const Instance& instance : instances) {
        assert(instance.model < models.size() && "RenderScene: a model index is out of range");
        scene.push_back(
            TransformModel(models[instance.model], camera_matrix * MakeModelMatrix(instance)));
    }

    // Back-face culling: the camera's coordinates have the camera at the origin. Before
    // the clipping, which then has fewer triangles to cut and removes the vertices that
    // only back faces named from every instance it cuts.
    if (back_face_culling == BackFaceCulling::kOn) {
        constexpr Vec3 kCameraPosition{0.0, 0.0, 0.0};
        for (Model& instance : scene) {
            instance = CullBackFaces(std::move(instance), kCameraPosition);
        }
    }

    // Clipping, in 3D: after the transforms, before the projection.
    return ClipScene(std::move(scene), camera.clipping_planes);
}

}  // namespace

void RenderScene(Canvas& canvas, const Viewport& viewport, std::span<const Model> models,
                 std::span<const Instance> instances, const Camera& camera,
                 DrawTriangleFunction draw_triangle, DepthBuffer* depth_buffer,
                 BackFaceCulling back_face_culling) {
    const Mat4 camera_matrix = MakeCameraMatrix(camera.position, camera.orientation);
    const std::vector<Model> clipped_scene =
        PlaceScene(models, instances, camera_matrix, camera, back_face_culling);

    // The perspective projection and the triangles.
    for (const Model& instance : clipped_scene) {
        RenderObject(canvas, viewport, instance.vertices, instance.triangles, draw_triangle,
                     depth_buffer);
    }
}

void RenderScene(Canvas& canvas, const Viewport& viewport, std::span<const Model> models,
                 std::span<const Instance> instances, const Camera& camera,
                 std::span<const Light> lights, ShadingModel shading_model,
                 DepthBuffer* depth_buffer, BackFaceCulling back_face_culling,
                 TextureMapping texture_mapping) {
    const Mat4 camera_matrix = MakeCameraMatrix(camera.position, camera.orientation);
    const std::vector<Model> clipped_scene =
        PlaceScene(models, instances, camera_matrix, camera, back_face_culling);

    // The lights go where the scene went: into the camera's coordinates.
    std::vector<Light> lights_in_camera;
    lights_in_camera.reserve(lights.size());
    for (const Light& light : lights) {
        lights_in_camera.push_back(TransformLight(light, camera_matrix));
    }

    // The perspective projection, the light and the triangles, in their colors or in
    // their models' textures.
    for (const Model& instance : clipped_scene) {
        RenderObject(canvas, viewport, instance, lights_in_camera, shading_model, depth_buffer,
                     texture_mapping);
    }
}

}  // namespace cgfs
