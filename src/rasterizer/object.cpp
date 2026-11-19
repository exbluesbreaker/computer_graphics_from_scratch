#include <cgfs/rasterizer/object.h>
#include <cgfs/rasterizer/point.h>
#include <cgfs/rasterizer/projection.h>

#include <cassert>
#include <vector>

namespace cgfs {

namespace {

/// Corner `k` of the triangle as a canvas point: where its vertex was projected to, with
/// the triangle's intensity at that corner. The projection knows where a corner lands
/// and how deep it was; the triangle knows how bright it is there.
Point Corner(const Triangle& triangle, std::size_t k, std::span<const Point> projected) {
    Point corner = projected[triangle.v[k]];
    corner.h = triangle.h[k];
    return corner;
}

/// Draw one triangle of the object through the projected vertices.
void RenderTriangle(Canvas& canvas, const Triangle& triangle, std::span<const Point> projected,
                    DrawTriangleFunction draw_triangle, DepthBuffer* depth_buffer) {
    assert(triangle.v[0] < projected.size() && triangle.v[1] < projected.size() &&
           triangle.v[2] < projected.size() && "RenderObject: a vertex index is out of range");
    draw_triangle(canvas, Corner(triangle, 0, projected), Corner(triangle, 1, projected),
                  Corner(triangle, 2, projected), triangle.color, depth_buffer);
}

}  // namespace

void RenderObject(Canvas& canvas, const Viewport& viewport, std::span<const Vec3> vertices,
                  std::span<const Triangle> triangles, DrawTriangleFunction draw_triangle,
                  DepthBuffer* depth_buffer) {
    std::vector<Point> projected;
    projected.reserve(vertices.size());
    for (const Vec3 vertex : vertices) {
        projected.push_back(ProjectVertex(viewport, vertex));
    }
    for (const Triangle& triangle : triangles) {
        RenderTriangle(canvas, triangle, projected, draw_triangle, depth_buffer);
    }
}

}  // namespace cgfs
