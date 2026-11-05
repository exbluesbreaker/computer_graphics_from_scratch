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
    return ViewportToCanvas(viewport, v.x * d / v.z, v.y * d / v.z);
}

}  // namespace cgfs
