#include <cgfs/rasterizer/line.h>

#include <cgfs/rasterizer/interpolate.h>

#include <cstddef>
#include <cstdlib>
#include <utility>
#include <vector>

namespace cgfs {

void DrawLine(Canvas& canvas, Point p0, Point p1, Color color) {
    if (std::abs(p1.x - p0.x) > std::abs(p1.y - p0.y)) {
        // Horizontal-ish: one pixel per column. Make sure x0 < x1.
        if (p0.x > p1.x) {
            std::swap(p0, p1);
        }
        const std::vector<double> ys = Interpolate(p0.x, p0.y, p1.x, p1.y);
        for (int x = p0.x; x <= p1.x; ++x) {
            canvas.PutPixel(x, ToPixel(ys[static_cast<std::size_t>(x - p0.x)]), color);
        }
    } else {
        // Vertical-ish: one pixel per row. Make sure y0 < y1.
        if (p0.y > p1.y) {
            std::swap(p0, p1);
        }
        const std::vector<double> xs = Interpolate(p0.y, p0.x, p1.y, p1.x);
        for (int y = p0.y; y <= p1.y; ++y) {
            canvas.PutPixel(ToPixel(xs[static_cast<std::size_t>(y - p0.y)]), y, color);
        }
    }
}

}  // namespace cgfs
