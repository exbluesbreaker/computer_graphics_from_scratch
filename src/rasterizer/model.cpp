#include <cgfs/rasterizer/model.h>

#include <vector>

namespace cgfs {

void RenderModel(Canvas& canvas, const Viewport& viewport, const Model& model,
                 const Mat4& transform) {
    std::vector<Vec3> transformed;
    transformed.reserve(model.vertices.size());
    for (const Vec3 vertex : model.vertices) {
        transformed.push_back(transform * vertex);
    }
    RenderObject(canvas, viewport, transformed, model.triangles);
}

}  // namespace cgfs
