#include <cgfs/rasterizer/model.h>

namespace cgfs {

Model TransformModel(const Model& model, const Mat4& transform) {
    Model transformed;
    transformed.vertices.reserve(model.vertices.size());
    for (const Vec3 vertex : model.vertices) {
        transformed.vertices.push_back(transform * vertex);
    }
    transformed.triangles = model.triangles;

    // Where the unit vector along X ends up: its length is the transform's scale.
    const Vec3 x_axis{transform.m[0][0], transform.m[1][0], transform.m[2][0]};
    transformed.bounding_sphere = {.center = transform * model.bounding_sphere.center,
                                   .radius = model.bounding_sphere.radius * Length(x_axis)};
    return transformed;
}

void RenderModel(Canvas& canvas, const Viewport& viewport, const Model& model,
                 const Mat4& transform) {
    const Model transformed = TransformModel(model, transform);
    RenderObject(canvas, viewport, transformed.vertices, transformed.triangles);
}

}  // namespace cgfs
