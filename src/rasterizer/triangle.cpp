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

/// The two texture coordinates of a triangle along its sides, each in the form in which
/// it is interpolated (see Carried).
struct TextureSides {
    SideValues u;
    SideValues v;
};

/// What is interpolated in the place of a texture coordinate of a vertex whose 1/z is
/// `inv_z`: the coordinate itself for linear mapping, and the coordinate divided by z
/// for perspective-correct mapping, which is the quantity that changes by equal steps
/// over the picture of a triangle.
double Carried(double coordinate, double inv_z, TextureMapping texture_mapping) {
    return texture_mapping == TextureMapping::kPerspectiveCorrect ? coordinate * inv_z : coordinate;
}

/// The texture coordinates of a triangle, sorted so that y0 <= y1 <= y2, interpolated
/// along its three sides in the form `texture_mapping` carries them in.
TextureSides TextureEdgeInterpolate(const Point& p0, const Point& p1, const Point& p2,
                                    TextureMapping texture_mapping) {
    const auto carried_along = [&](double c0, double c1, double c2) {
        return EdgeInterpolate(p0.y, Carried(c0, p0.inv_z, texture_mapping), p1.y,
                               Carried(c1, p1.inv_z, texture_mapping), p2.y,
                               Carried(c2, p2.inv_z, texture_mapping));
    };
    return {.u = carried_along(p0.uv.u, p1.uv.u, p2.uv.u),
            .v = carried_along(p0.uv.v, p1.uv.v, p2.uv.v)};
}

/// The color of the texture that a pixel shows, from what was interpolated to it: u and
/// v themselves under linear mapping, and u/z and v/z under perspective-correct mapping,
/// where dividing by the pixel's 1/z gives u and v back. The filter makes the color of
/// the texels around that point.
Color TexelAt(const Texture& texture, double carried_u, double carried_v, double inv_z,
              TextureMapping texture_mapping, TextureFilter texture_filter) {
    if (texture_mapping == TextureMapping::kPerspectiveCorrect) {
        return texture.GetTexel(carried_u / inv_z, carried_v / inv_z, texture_filter);
    }
    return texture.GetTexel(carried_u, carried_v, texture_filter);
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

void DrawTexturedTriangle(Canvas& canvas, Point p0, Point p1, Point p2, const Texture& texture,
                          TextureMapping texture_mapping, DepthBuffer* depth_buffer,
                          TextureFilter texture_filter) {
    assert(Fits(depth_buffer, canvas) && "DrawTexturedTriangle: a depth buffer of another size");
    assert((texture_mapping == TextureMapping::kLinear ||
            (p0.inv_z > 0.0 && p1.inv_z > 0.0 && p2.inv_z > 0.0)) &&
           "DrawTexturedTriangle: perspective-correct mapping needs the depth of every point");

    // Sort the points so that y0 <= y1 <= y2. A vertex carries its texture coordinates
    // along.
    SortByY(p0, p1, p2);

    // Compute the x coordinates, the h values, the 1/z and the two texture coordinates
    // of the triangle edges, one of each per row. The texture coordinates are u and v or
    // u/z and v/z, as the mapping says.
    const SideValues x_sides = EdgeInterpolate(p0.y, p0.x, p1.y, p1.x, p2.y, p2.x);
    const SideValues h_sides = EdgeInterpolate(p0.y, p0.h, p1.y, p1.h, p2.y, p2.h);
    const SideValues iz_sides = EdgeInterpolate(p0.y, p0.inv_z, p1.y, p1.inv_z, p2.y, p2.inv_z);
    const TextureSides uv_sides = TextureEdgeInterpolate(p0, p1, p2, texture_mapping);

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

    // Draw the horizontal segments, each with h, 1/z and the texture coordinates
    // interpolated from its left end to its right end over the pixels it covers. A pixel
    // that is to be set gets the texel it shows, times its intensity.
    for (int y = p0.y; y <= p2.y; ++y) {
        const auto row = static_cast<std::size_t>(y - p0.y);
        const int x_l = ToPixel(left(x_sides)[row]);
        const int x_r = ToPixel(right(x_sides)[row]);
        if (x_r < x_l) {
            // A row without pixels, as in the filled triangle.
            continue;
        }
        const std::vector<double> h_segment =
            Interpolate(x_l, left(h_sides)[row], x_r, right(h_sides)[row]);
        const std::vector<double> iz_segment =
            Interpolate(x_l, left(iz_sides)[row], x_r, right(iz_sides)[row]);
        const std::vector<double> u_segment =
            Interpolate(x_l, left(uv_sides.u)[row], x_r, right(uv_sides.u)[row]);
        const std::vector<double> v_segment =
            Interpolate(x_l, left(uv_sides.v)[row], x_r, right(uv_sides.v)[row]);
        for (int x = x_l; x <= x_r; ++x) {
            const auto i = static_cast<std::size_t>(x - x_l);
            const double inv_z = iz_segment[i];
            if (depth_buffer == nullptr || depth_buffer->UpdateIfCloser(x, y, inv_z)) {
                const Color texel = TexelAt(texture, u_segment[i], v_segment[i], inv_z,
                                            texture_mapping, texture_filter);
                canvas.PutPixel(x, y, texel * h_segment[i]);
            }
        }
    }
}

void DrawPhongTriangle(Canvas& canvas, const Viewport& viewport, Point p0, Point p1, Point p2,
                       Color color, double specular, std::span<const Light> lights,
                       DepthBuffer* depth_buffer, const Texture* texture,
                       TextureMapping texture_mapping, TextureFilter texture_filter) {
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

    // A triangle with a texture takes its two texture coordinates along as well, in the
    // form the mapping carries them in.
    const bool textured = texture != nullptr;
    const TextureSides uv_sides =
        textured ? TextureEdgeInterpolate(p0, p1, p2, texture_mapping) : TextureSides{};

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
        const std::vector<double> u_segment =
            textured ? Interpolate(x_l, left(uv_sides.u)[row], x_r, right(uv_sides.u)[row])
                     : std::vector<double>{};
        const std::vector<double> v_segment =
            textured ? Interpolate(x_l, left(uv_sides.v)[row], x_r, right(uv_sides.v)[row])
                     : std::vector<double>{};
        for (int x = x_l; x <= x_r; ++x) {
            const auto i = static_cast<std::size_t>(x - x_l);
            const double inv_z = iz_segment[i];
            if (depth_buffer == nullptr || depth_buffer->UpdateIfCloser(x, y, inv_z)) {
                const Vec3 point = UnprojectVertex(viewport, x, y, inv_z);
                const Vec3 normal = Normalized({nx_segment[i], ny_segment[i], nz_segment[i]});
                const Vec3 view = -point;  // toward the camera, the origin of its coordinates
                // The surface's own color at this pixel: the triangle's, or its texture's.
                const Color surface = textured ? TexelAt(*texture, u_segment[i], v_segment[i],
                                                         inv_z, texture_mapping, texture_filter)
                                               : color;
                canvas.PutPixel(
                    x, y, surface * ComputeIllumination(point, normal, view, specular, lights));
            }
        }
    }
}

}  // namespace cgfs
