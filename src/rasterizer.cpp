#include <cgfs/rasterizer.h>

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <utility>

namespace cgfs {

namespace {

/// The pixel an interpolated coordinate falls in: truncated toward zero, the book's
/// convention. -0.5 and 0.5 both land on pixel 0.
int ToPixel(double coordinate) { return static_cast<int>(coordinate); }

}  // namespace

std::vector<double> Interpolate(int i0, double d0, int i1, double d1) {
    assert(i0 <= i1 && "Interpolate: the independent variable must not decrease");
    if (i0 == i1) {
        return {d0};
    }
    if (i0 > i1) {
        return {};
    }

    std::vector<double> values;
    values.reserve(static_cast<std::size_t>(i1 - i0) + 1);
    const double a = (d1 - d0) / static_cast<double>(i1 - i0);
    double d = d0;
    for (int i = i0; i <= i1; ++i) {
        values.push_back(d);
        d += a;
    }
    return values;
}

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
