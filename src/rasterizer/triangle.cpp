#include <cgfs/rasterizer/triangle.h>

#include <cgfs/rasterizer/interpolate.h>
#include <cgfs/rasterizer/line.h>

#include <cassert>
#include <cstddef>
#include <utility>
#include <vector>

namespace cgfs {

namespace {

/// Sort three vertices so that p0.y <= p1.y <= p2.y, with the book's three conditional
/// swaps. A vertex carries its h and its 1/z along, so they stay with their points.
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

/// The 1/z of the pixels x_l..x_r of one row of a triangle, interpolated from the row's
/// left end to its right end; empty when there is no depth buffer to compare them with,
/// which spares the pictures drawn without one the work.
std::vector<double> RowDepths(const DepthBuffer* depth_buffer, int x_l, double inv_z_l, int x_r,
                              double inv_z_r) {
    if (depth_buffer == nullptr) {
        return {};
    }
    return Interpolate(x_l, inv_z_l, x_r, inv_z_r);
}

/// Whether `depth_buffer` can answer for the pixels of `canvas`: there is none, or it
/// has the canvas's size.
[[maybe_unused]] bool Fits(const DepthBuffer* depth_buffer, const Canvas& canvas) {
    if (depth_buffer == nullptr) {
        return true;
    }
    return depth_buffer->width() == canvas.width() && depth_buffer->height() == canvas.height();
}

}  // namespace

void DrawWireframeTriangle(Canvas& canvas, Point p0, Point p1, Point p2, Color color,
                           DepthBuffer* /*depth_buffer*/) {
    DrawLine(canvas, p0, p1, color);
    DrawLine(canvas, p1, p2, color);
    DrawLine(canvas, p2, p0, color);
}

void DrawFilledTriangle(Canvas& canvas, Point p0, Point p1, Point p2, Color color,
                        DepthBuffer* depth_buffer) {
    assert(Fits(depth_buffer, canvas) && "DrawFilledTriangle: a depth buffer of another size");

    // Sort the points so that y0 <= y1 <= y2.
    SortByY(p0, p1, p2);

    // Compute the x coordinates and the 1/z of the triangle edges, one of each per row.
    const std::vector<double> x01 = Interpolate(p0.y, p0.x, p1.y, p1.x);
    const std::vector<double> iz01 = Interpolate(p0.y, p0.inv_z, p1.y, p1.inv_z);
    const std::vector<double> x12 = Interpolate(p1.y, p1.x, p2.y, p2.x);
    const std::vector<double> iz12 = Interpolate(p1.y, p1.inv_z, p2.y, p2.inv_z);
    const std::vector<double> x02 = Interpolate(p0.y, p0.x, p2.y, p2.x);
    const std::vector<double> iz02 = Interpolate(p0.y, p0.inv_z, p2.y, p2.inv_z);

    // Concatenate the short sides. Both end up with the row of p1, so drop one copy; the
    // result has one x per row, like the long side. The 1/z lists exactly like the x
    // lists.
    const std::vector<double> x012 = Concatenate(x01, x12);
    const std::vector<double> iz012 = Concatenate(iz01, iz12);

    // Determine which is left and which is right. The sides do not cross, so one row
    // decides for all; the middle row is the one that is sure to be away from the
    // vertices, where the sides meet. The 1/z lists follow their x lists.
    const std::size_t m = x012.size() / 2;
    const bool long_side_is_left = x02[m] < x012[m];
    const std::vector<double>& x_left = long_side_is_left ? x02 : x012;
    const std::vector<double>& iz_left = long_side_is_left ? iz02 : iz012;
    const std::vector<double>& x_right = long_side_is_left ? x012 : x02;
    const std::vector<double>& iz_right = long_side_is_left ? iz012 : iz02;

    // Draw the horizontal segments, each with 1/z interpolated from its left end to its
    // right end over the pixels it covers. Without a depth buffer every pixel is set;
    // with one, only those where the triangle is the nearest thing so far.
    for (int y = p0.y; y <= p2.y; ++y) {
        const auto row = static_cast<std::size_t>(y - p0.y);
        const int x_l = ToPixel(x_left[row]);
        const int x_r = ToPixel(x_right[row]);
        if (x_r < x_l) {
            // The row has no pixels. This happens at a vertex row when the two running
            // sums for x land on either side of the vertex, and in the two-row case the
            // middle-row rule cannot decide (see the spec); Interpolate's precondition
            // would fail here.
            continue;
        }
        const std::vector<double> iz_segment =
            RowDepths(depth_buffer, x_l, iz_left[row], x_r, iz_right[row]);
        for (int x = x_l; x <= x_r; ++x) {
            const auto i = static_cast<std::size_t>(x - x_l);
            if (depth_buffer == nullptr || depth_buffer->UpdateIfCloser(x, y, iz_segment[i])) {
                canvas.PutPixel(x, y, color);
            }
        }
    }
}

void DrawShadedTriangle(Canvas& canvas, Point p0, Point p1, Point p2, Color color,
                        DepthBuffer* depth_buffer) {
    assert(Fits(depth_buffer, canvas) && "DrawShadedTriangle: a depth buffer of another size");

    // Sort the points so that y0 <= y1 <= y2.
    SortByY(p0, p1, p2);

    // Compute the x coordinates, the h values and the 1/z of the triangle edges, one of
    // each per row.
    const std::vector<double> x01 = Interpolate(p0.y, p0.x, p1.y, p1.x);
    const std::vector<double> h01 = Interpolate(p0.y, p0.h, p1.y, p1.h);
    const std::vector<double> iz01 = Interpolate(p0.y, p0.inv_z, p1.y, p1.inv_z);
    const std::vector<double> x12 = Interpolate(p1.y, p1.x, p2.y, p2.x);
    const std::vector<double> h12 = Interpolate(p1.y, p1.h, p2.y, p2.h);
    const std::vector<double> iz12 = Interpolate(p1.y, p1.inv_z, p2.y, p2.inv_z);
    const std::vector<double> x02 = Interpolate(p0.y, p0.x, p2.y, p2.x);
    const std::vector<double> h02 = Interpolate(p0.y, p0.h, p2.y, p2.h);
    const std::vector<double> iz02 = Interpolate(p0.y, p0.inv_z, p2.y, p2.inv_z);

    // Concatenate the short sides, the h and 1/z lists exactly like the x lists.
    const std::vector<double> x012 = Concatenate(x01, x12);
    const std::vector<double> h012 = Concatenate(h01, h12);
    const std::vector<double> iz012 = Concatenate(iz01, iz12);

    // Determine which is left and which is right, as for the filled triangle; the h and
    // 1/z lists follow their x lists.
    const std::size_t m = x012.size() / 2;
    const bool long_side_is_left = x02[m] < x012[m];
    const std::vector<double>& x_left = long_side_is_left ? x02 : x012;
    const std::vector<double>& h_left = long_side_is_left ? h02 : h012;
    const std::vector<double>& iz_left = long_side_is_left ? iz02 : iz012;
    const std::vector<double>& x_right = long_side_is_left ? x012 : x02;
    const std::vector<double>& h_right = long_side_is_left ? h012 : h02;
    const std::vector<double>& iz_right = long_side_is_left ? iz012 : iz02;

    // Draw the horizontal segments, each with h and 1/z interpolated from its left end
    // to its right end over the pixels it covers. Without a depth buffer every pixel is
    // set; with one, only those where the triangle is the nearest thing so far.
    for (int y = p0.y; y <= p2.y; ++y) {
        const auto row = static_cast<std::size_t>(y - p0.y);
        const int x_l = ToPixel(x_left[row]);
        const int x_r = ToPixel(x_right[row]);
        if (x_r < x_l) {
            // A row without pixels, as in the filled triangle.
            continue;
        }
        const std::vector<double> h_segment = Interpolate(x_l, h_left[row], x_r, h_right[row]);
        const std::vector<double> iz_segment =
            RowDepths(depth_buffer, x_l, iz_left[row], x_r, iz_right[row]);
        for (int x = x_l; x <= x_r; ++x) {
            const auto i = static_cast<std::size_t>(x - x_l);
            if (depth_buffer == nullptr || depth_buffer->UpdateIfCloser(x, y, iz_segment[i])) {
                canvas.PutPixel(x, y, color * h_segment[i]);
            }
        }
    }
}

}  // namespace cgfs
