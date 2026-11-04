#include <cgfs/rasterizer.h>

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <iterator>
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

void DrawWireframeTriangle(Canvas& canvas, Point p0, Point p1, Point p2, Color color) {
    DrawLine(canvas, p0, p1, color);
    DrawLine(canvas, p1, p2, color);
    DrawLine(canvas, p2, p0, color);
}

void DrawFilledTriangle(Canvas& canvas, Point p0, Point p1, Point p2, Color color) {
    // Sort the points so that y0 <= y1 <= y2.
    if (p1.y < p0.y) {
        std::swap(p1, p0);
    }
    if (p2.y < p0.y) {
        std::swap(p2, p0);
    }
    if (p2.y < p1.y) {
        std::swap(p2, p1);
    }

    // Compute the x coordinates of the triangle edges, one value per row.
    std::vector<double> x01 = Interpolate(p0.y, p0.x, p1.y, p1.x);
    const std::vector<double> x12 = Interpolate(p1.y, p1.x, p2.y, p2.x);
    const std::vector<double> x02 = Interpolate(p0.y, p0.x, p2.y, p2.x);

    // Concatenate the short sides. Both end up with the row of p1, so drop one copy; the
    // result has one x per row, like the long side.
    x01.pop_back();
    std::vector<double> x012 = std::move(x01);
    x012.insert(x012.end(), x12.begin(), x12.end());

    // Determine which is left and which is right. The sides do not cross, so one row
    // decides for all; the middle row is the one that is sure to be away from the
    // vertices, where the sides meet.
    const std::size_t m = x012.size() / 2;
    const bool long_side_is_left = x02[m] < x012[m];
    const std::vector<double>& x_left = long_side_is_left ? x02 : x012;
    const std::vector<double>& x_right = long_side_is_left ? x012 : x02;

    // Draw the horizontal segments.
    for (int y = p0.y; y <= p2.y; ++y) {
        const auto row = static_cast<std::size_t>(y - p0.y);
        for (int x = ToPixel(x_left[row]); x <= ToPixel(x_right[row]); ++x) {
            canvas.PutPixel(x, y, color);
        }
    }
}

}  // namespace cgfs
