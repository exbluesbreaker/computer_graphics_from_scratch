#include <cgfs/rasterizer/triangle.h>

#include <cgfs/lighting/illumination.h>
#include <cgfs/math/vec3.h>
#include <cgfs/rasterizer/interpolate.h>
#include <cgfs/rasterizer/line.h>
#include <cgfs/rasterizer/projection.h>

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

/// One quantity of a triangle along its sides, a value per row: along the long side
/// p0 p2, and along the two short sides p0 p1 and p1 p2 joined into one list.
struct SideValues {
    std::vector<double> long_side;    // the book's v02
    std::vector<double> short_sides;  // the book's v012
};

/// A quantity that is v0, v1 and v2 at the vertices of a triangle, sorted so that
/// y0 <= y1 <= y2, interpolated along its three sides: the book's EdgeInterpolate, which
/// its Shading chapter introduces when the quantities become too many to write out.
SideValues EdgeInterpolate(int y0, double v0, int y1, double v1, int y2, double v2) {
    return {.long_side = Interpolate(y0, v0, y2, v2),
            .short_sides = Concatenate(Interpolate(y0, v0, y1, v1), Interpolate(y1, v1, y2, v2))};
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

void DrawPhongTriangle(Canvas& canvas, const Viewport& viewport, Point p0, Point p1, Point p2,
                       Color color, double specular, std::span<const Light> lights,
                       DepthBuffer* depth_buffer) {
    assert(Fits(depth_buffer, canvas) && "DrawPhongTriangle: a depth buffer of another size");

    // Sort the points so that y0 <= y1 <= y2. A vertex carries its normal along.
    SortByY(p0, p1, p2);

    // Compute the x coordinates, the 1/z and the three components of the normal of the
    // triangle edges, one of each per row.
    const SideValues x_sides = EdgeInterpolate(p0.y, p0.x, p1.y, p1.x, p2.y, p2.x);
    const SideValues iz_sides = EdgeInterpolate(p0.y, p0.inv_z, p1.y, p1.inv_z, p2.y, p2.inv_z);
    const SideValues nx_sides =
        EdgeInterpolate(p0.y, p0.normal.x, p1.y, p1.normal.x, p2.y, p2.normal.x);
    const SideValues ny_sides =
        EdgeInterpolate(p0.y, p0.normal.y, p1.y, p1.normal.y, p2.y, p2.normal.y);
    const SideValues nz_sides =
        EdgeInterpolate(p0.y, p0.normal.z, p1.y, p1.normal.z, p2.y, p2.normal.z);

    // Determine which is left and which is right, as for the filled triangle; every
    // other quantity follows x.
    const std::size_t m = x_sides.short_sides.size() / 2;
    const bool long_side_is_left = x_sides.long_side[m] < x_sides.short_sides[m];
    const auto left = [&](const SideValues& values) -> const std::vector<double>& {
        return long_side_is_left ? values.long_side : values.short_sides;
    };
    const auto right = [&](const SideValues& values) -> const std::vector<double>& {
        return long_side_is_left ? values.short_sides : values.long_side;
    };

    // Draw the horizontal segments, each with 1/z and the normal interpolated from its
    // left end to its right end over the pixels it covers. A pixel that is to be set
    // gets the light of the point of the triangle it shows.
    for (int y = p0.y; y <= p2.y; ++y) {
        const auto row = static_cast<std::size_t>(y - p0.y);
        const int x_l = ToPixel(left(x_sides)[row]);
        const int x_r = ToPixel(right(x_sides)[row]);
        if (x_r < x_l) {
            // A row without pixels, as in the filled triangle.
            continue;
        }
        const std::vector<double> iz_segment =
            Interpolate(x_l, left(iz_sides)[row], x_r, right(iz_sides)[row]);
        const std::vector<double> nx_segment =
            Interpolate(x_l, left(nx_sides)[row], x_r, right(nx_sides)[row]);
        const std::vector<double> ny_segment =
            Interpolate(x_l, left(ny_sides)[row], x_r, right(ny_sides)[row]);
        const std::vector<double> nz_segment =
            Interpolate(x_l, left(nz_sides)[row], x_r, right(nz_sides)[row]);
        for (int x = x_l; x <= x_r; ++x) {
            const auto i = static_cast<std::size_t>(x - x_l);
            const double inv_z = iz_segment[i];
            if (depth_buffer == nullptr || depth_buffer->UpdateIfCloser(x, y, inv_z)) {
                const Vec3 point = UnprojectVertex(viewport, x, y, inv_z);
                const Vec3 normal = Normalized({nx_segment[i], ny_segment[i], nz_segment[i]});
                const Vec3 view = -point;  // toward the camera, the origin of its coordinates
                canvas.PutPixel(x, y,
                                color * ComputeIllumination(point, normal, view, specular, lights));
            }
        }
    }
}

}  // namespace cgfs
