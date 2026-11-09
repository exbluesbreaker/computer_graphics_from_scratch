#include <cgfs/rasterizer/scene.h>

namespace cgfs {

void RenderScene(Canvas& canvas, const Viewport& viewport, std::span<const Model> models,
                 std::span<const Instance> instances) {
    for (const Instance& instance : instances) {
        RenderInstance(canvas, viewport, instance, models);
    }
}

}  // namespace cgfs
