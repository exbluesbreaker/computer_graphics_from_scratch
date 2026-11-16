#include <cgfs/camera/clipping_planes.h>
#include <cgfs/math/vec3.h>

namespace cgfs {

std::vector<Plane> MakeClippingPlanes(const Viewport& viewport) {
    const double d = viewport.distance();
    // Half the viewport and half a pixel (the header says why).
    const double half_width = viewport.width() / 2.0 * (1.0 + 1.0 / viewport.canvas_width());
    const double half_height = viewport.height() / 2.0 * (1.0 + 1.0 / viewport.canvas_height());
    return {
        {.normal = {0.0, 0.0, 1.0}, .d = -d},                      // near
        {.normal = Normalized({d, 0.0, half_width}), .d = 0.0},    // left
        {.normal = Normalized({-d, 0.0, half_width}), .d = 0.0},   // right
        {.normal = Normalized({0.0, d, half_height}), .d = 0.0},   // bottom
        {.normal = Normalized({0.0, -d, half_height}), .d = 0.0},  // top
    };
}

}  // namespace cgfs
