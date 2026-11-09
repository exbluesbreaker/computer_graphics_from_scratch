#include <cgfs/rasterizer/instance.h>

#include <cassert>

namespace cgfs {

Mat4 MakeModelMatrix(const Instance& instance) {
    return MakeTranslationMatrix(instance.position) * instance.orientation *
           MakeScalingMatrix(instance.scale);
}

void RenderInstance(Canvas& canvas, const Viewport& viewport, const Instance& instance,
                    std::span<const Model> models) {
    assert(instance.model < models.size() && "RenderInstance: the model index is out of range");
    RenderModel(canvas, viewport, models[instance.model], MakeModelMatrix(instance));
}

}  // namespace cgfs
