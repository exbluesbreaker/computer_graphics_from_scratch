#include <cgfs/rasterizer/triangle.h>

#include <cgfs/rasterizer/interpolate.h>
#include <cgfs/rasterizer/line.h>

#include <cstddef>
#include <utility>
#include <vector>

namespace cgfs {

namespace {

/// Sort three vertices so that p0.y <= p1.y <= p2.y, with the book's three conditional
/// swaps. A vertex carries its h along, so the intensities stay with their points.
void SortByY(Point& p0, Point& p1, Point& p2) {
    if (p1.y < p0.y) {
        std::swap(p1, p0);
    }
    if (p2.y < p0.y) {
        std::swap(p2, p0);
    }
    if (p2.y < p1.y) {
        std::swap(p2, p1);
    }
}

/// The two short sides joined into one list the length of the long side's: `v01`
/// without its last value (the row of p1, which `v12` starts with) followed by `v12`.
/// Precondition: `v01` is not empty, which Interpolate guarantees.
std::vector<double> Concatenate(const std::vector<double>& v01, const std::vector<double>& v12) {
    std::vector<double> v012;
    v012.reserve(v01.size() - 1 + v12.size());
    v012.insert(v012.end(), v01.begin(), v01.end() - 1);
    v012.insert(v012.end(), v12.begin(), v12.end());
    return v012;
}

}  // namespace

void DrawWireframeTriangle(Canvas& canvas, Point p0, Point p1, Point p2, Color color) {
    DrawLine(canvas, p0, p1, color);
    DrawLine(canvas, p1, p2, color);
    DrawLine(canvas, p2, p0, color);
}

void DrawFilledTriangle(Canvas& canvas, Point p0, Point p1, Point p2, Color color) {
    // Sort the points so that y0 <= y1 <= y2.
    SortByY(p0, p1, p2);

    // Compute the x coordinates of the triangle edges, one value per row.
    const std::vector<double> x01 = Interpolate(p0.y, p0.x, p1.y, p1.x);
    const std::vector<double> x12 = Interpolate(p1.y, p1.x, p2.y, p2.x);
    const std::vector<double> x02 = Interpolate(p0.y, p0.x, p2.y, p2.x);

    // Concatenate the short sides. Both end up with the row of p1, so drop one copy; the
    // result has one x per row, like the long side.
    const std::vector<double> x012 = Concatenate(x01, x12);

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

void DrawShadedTriangle(Canvas& canvas, Point p0, Point p1, Point p2, Color color) {
    // Sort the points so that y0 <= y1 <= y2.
    SortByY(p0, p1, p2);

    // Compute the x coordinates and h values of the triangle edges, one of each per row.
    const std::vector<double> x01 = Interpolate(p0.y, p0.x, p1.y, p1.x);
    const std::vector<double> h01 = Interpolate(p0.y, p0.h, p1.y, p1.h);
    const std::vector<double> x12 = Interpolate(p1.y, p1.x, p2.y, p2.x);
    const std::vector<double> h12 = Interpolate(p1.y, p1.h, p2.y, p2.h);
    const std::vector<double> x02 = Interpolate(p0.y, p0.x, p2.y, p2.x);
    const std::vector<double> h02 = Interpolate(p0.y, p0.h, p2.y, p2.h);

    // Concatenate the short sides, the h lists exactly like the x lists.
    const std::vector<double> x012 = Concatenate(x01, x12);
    const std::vector<double> h012 = Concatenate(h01, h12);

    // Determine which is left and which is right, as for the filled triangle; the h
    // lists follow their x lists.
    const std::size_t m = x012.size() / 2;
    const bool long_side_is_left = x02[m] < x012[m];
    const std::vector<double>& x_left = long_side_is_left ? x02 : x012;
    const std::vector<double>& h_left = long_side_is_left ? h02 : h012;
    const std::vector<double>& x_right = long_side_is_left ? x012 : x02;
    const std::vector<double>& h_right = long_side_is_left ? h012 : h02;

    // Draw the horizontal segments, each with h interpolated from its left end to its
    // right end over the pixels it covers.
    for (int y = p0.y; y <= p2.y; ++y) {
        const auto row = static_cast<std::size_t>(y - p0.y);
        const int x_l = ToPixel(x_left[row]);
        const int x_r = ToPixel(x_right[row]);
        if (x_r < x_l) {
            // The row has no pixels. This happens at a vertex row when the two running
            // sums for x land on either side of the vertex, and in the two-row case the
            // middle-row rule cannot decide (see the spec); the filled triangle's inner
            // loop then runs zero times, and Interpolate's precondition would fail here.
            continue;
        }
        const std::vector<double> h_segment = Interpolate(x_l, h_left[row], x_r, h_right[row]);
        for (int x = x_l; x <= x_r; ++x) {
            const Color shaded_color = color * h_segment[static_cast<std::size_t>(x - x_l)];
            canvas.PutPixel(x, y, shaded_color);
        }
    }
}

}  // namespace cgfs
