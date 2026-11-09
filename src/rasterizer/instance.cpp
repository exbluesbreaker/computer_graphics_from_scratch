#include <cgfs/rasterizer/instance.h>
#include <cgfs/rasterizer/object.h>

#include <cassert>
#include <vector>

namespace cgfs {

void RenderInstance(Canvas& canvas, const Viewport& viewport, const Instance& instance,
                    std::span<const Model> models) {
    assert(instance.model < models.size() && "RenderInstance: the model index is out of range");
    const Model& model = models[instance.model];
    std::vector<Vec3> moved;
    moved.reserve(model.vertices.size());
    for (const Vec3 vertex : model.vertices) {
        moved.push_back(vertex + instance.position);
    }
    RenderObject(canvas, viewport, moved, model.triangles);
}

}  // namespace cgfs
