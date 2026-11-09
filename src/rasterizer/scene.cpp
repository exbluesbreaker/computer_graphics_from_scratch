#include <cgfs/math/mat4.h>
#include <cgfs/rasterizer/scene.h>

#include <cassert>

namespace cgfs {

void RenderScene(Canvas& canvas, const Viewport& viewport, std::span<const Model> models,
                 std::span<const Instance> instances, const Camera& camera) {
    const Mat4 camera_matrix = MakeCameraMatrix(camera.position, camera.orientation);
    for (const Instance& instance : instances) {
        assert(instance.model < models.size() && "RenderScene: a model index is out of range");
        RenderModel(canvas, viewport, models[instance.model],
                    camera_matrix * MakeModelMatrix(instance));
    }
}

}  // namespace cgfs
