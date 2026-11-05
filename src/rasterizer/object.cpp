#include <cgfs/rasterizer/object.h>
#include <cgfs/rasterizer/point.h>
#include <cgfs/rasterizer/projection.h>
#include <cgfs/rasterizer/triangle.h>

#include <cassert>
#include <vector>

namespace cgfs {

namespace {

/// Draw one triangle of the object through the projected vertices.
void RenderTriangle(Canvas& canvas, const Triangle& triangle, std::span<const Point> projected) {
    assert(triangle.v[0] < projected.size() && triangle.v[1] < projected.size() &&
           triangle.v[2] < projected.size() && "RenderObject: a vertex index is out of range");
    DrawWireframeTriangle(canvas, projected[triangle.v[0]], projected[triangle.v[1]],
                          projected[triangle.v[2]], triangle.color);
}

}  // namespace

void RenderObject(Canvas& canvas, const Viewport& viewport, std::span<const Vec3> vertices,
                  std::span<const Triangle> triangles) {
    std::vector<Point> projected;
    projected.reserve(vertices.size());
    for (const Vec3 vertex : vertices) {
        projected.push_back(ProjectVertex(viewport, vertex));
    }
    for (const Triangle& triangle : triangles) {
        RenderTriangle(canvas, triangle, projected);
    }
}

}  // namespace cgfs
