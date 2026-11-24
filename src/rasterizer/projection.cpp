#include <cgfs/rasterizer/projection.h>

#include <cassert>

namespace cgfs {

Point ViewportToCanvas(const Viewport& viewport, double x, double y) {
    return {ToPixel(x * viewport.canvas_width() / viewport.width()),
            ToPixel(y * viewport.canvas_height() / viewport.height())};
}

Point ProjectVertex(const Viewport& viewport, Vec3 v) {
    assert(v.z > 0 && "ProjectVertex: the vertex must be in front of the camera");
    const double d = viewport.distance();
    Point projected = ViewportToCanvas(viewport, v.x * d / v.z, v.y * d / v.z);
    projected.inv_z = 1.0 / v.z;
    return projected;
}

Vec3 UnprojectVertex(const Viewport& viewport, int x, int y, double inv_z) {
    assert(inv_z > 0 && "UnprojectVertex: the depth must be in front of the camera");
    const double z = 1.0 / inv_z;
    const double d = viewport.distance();
    const Vec3 on_viewport = viewport.CanvasToViewport(x, y);
    return {on_viewport.x * z / d, on_viewport.y * z / d, z};
}

}  // namespace cgfs
