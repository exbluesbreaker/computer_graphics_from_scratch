#include <cgfs/lighting/illumination.h>
#include <cgfs/math/triangle_normal.h>
#include <cgfs/math/vec3.h>
#include <cgfs/rasterizer/object.h>
#include <cgfs/rasterizer/point.h>
#include <cgfs/rasterizer/projection.h>
#include <cgfs/rasterizer/shading.h>
#include <cgfs/rasterizer/triangle.h>

#include <array>
#include <cassert>
#include <cstddef>
#include <vector>

namespace cgfs {

namespace {

/// The normal of the surface at corner `k` of the triangle: the one the triangle states
/// there, as it is, or `triangle_normal`, the triangle's own, where it states none.
Vec3 CornerNormal(const Triangle& triangle, std::size_t k, Vec3 triangle_normal) {
    const Vec3 stated = triangle.normals[k];
    return stated == Vec3{} ? triangle_normal : stated;
}

/// The light at `point` of a surface with the unit normal `normal`, as the camera sees
/// it: in the camera's coordinates the camera is the origin, so the direction from the
/// point to the viewer is minus the point.
double Illumination(Vec3 point, Vec3 normal, double specular, std::span<const Light> lights) {
    const Vec3 view = -point;
    return ComputeIllumination(point, normal, view, specular, lights);
}

/// Draw one triangle of the object under the lights: the book's RenderTriangle of the
/// Shading chapter, with a branch for each of its pictures.
void RenderTriangle(Canvas& canvas, const Viewport& viewport, const Triangle& triangle,
                    std::span<const Vec3> vertices, std::span<const Point> projected,
                    double specular, std::span<const Light> lights, ShadingModel shading_model,
                    DepthBuffer* depth_buffer) {
    assert(triangle.v[0] < vertices.size() && triangle.v[1] < vertices.size() &&
           triangle.v[2] < vertices.size() && "RenderObject: a vertex index is out of range");
    const std::array<Vec3, 3> v = {vertices[triangle.v[0]], vertices[triangle.v[1]],
                                   vertices[triangle.v[2]]};
    std::array<Point, 3> p = {projected[triangle.v[0]], projected[triangle.v[1]],
                              projected[triangle.v[2]]};

    // The triangle's own normal, from its corners in the order the model lists them,
    // which is what makes it point out of the object.
    const Vec3 normal = TriangleNormal(v[0], v[1], v[2]);
    if (normal == Vec3{}) {
        return;  // no area: nothing faces anywhere, and there is nothing to light
    }
    const Vec3 triangle_normal = Normalized(normal);

    switch (shading_model) {
        case ShadingModel::kFlat: {
            // Flat shading: compute the light once, at the center, for the whole triangle.
            const Vec3 center = (v[0] + v[1] + v[2]) / 3.0;
            const double intensity = Illumination(center, triangle_normal, specular, lights);
            DrawFilledTriangle(canvas, p[0], p[1], p[2], triangle.color * intensity, depth_buffer);
            break;
        }
        case ShadingModel::kGouraudFaceted: {
            // Gouraud shading as the chapter first shows it: compute the light at the
            // corners, each with the triangle's own normal, and interpolate it.
            for (std::size_t k = 0; k < 3; ++k) {
                p[k].h = Illumination(v[k], triangle_normal, specular, lights);
            }
            DrawShadedTriangle(canvas, p[0], p[1], p[2], triangle.color, depth_buffer);
            break;
        }
        case ShadingModel::kGouraud: {
            // Gouraud shading with the model's normals: the same, each corner with the
            // normal of the surface there. A corner that the clipping made has a normal
            // shorter than 1.
            for (std::size_t k = 0; k < 3; ++k) {
                const Vec3 corner_normal = Normalized(CornerNormal(triangle, k, triangle_normal));
                p[k].h = Illumination(v[k], corner_normal, specular, lights);
            }
            DrawShadedTriangle(canvas, p[0], p[1], p[2], triangle.color, depth_buffer);
            break;
        }
        case ShadingModel::kPhong: {
            // Phong shading: interpolate the normals, and compute the light at every pixel.
            // The normals go in as they are and are normalized pixel by pixel.
            for (std::size_t k = 0; k < 3; ++k) {
                p[k].normal = CornerNormal(triangle, k, triangle_normal);
            }
            DrawPhongTriangle(canvas, viewport, p[0], p[1], p[2], triangle.color, specular, lights,
                              depth_buffer);
            break;
        }
    }
}

}  // namespace

void RenderObject(Canvas& canvas, const Viewport& viewport, const Model& object,
                  std::span<const Light> lights, ShadingModel shading_model,
                  DepthBuffer* depth_buffer) {
    std::vector<Point> projected;
    projected.reserve(object.vertices.size());
    for (const Vec3 vertex : object.vertices) {
        projected.push_back(ProjectVertex(viewport, vertex));
    }
    for (const Triangle& triangle : object.triangles) {
        RenderTriangle(canvas, viewport, triangle, object.vertices, projected, object.specular,
                       lights, shading_model, depth_buffer);
    }
}

}  // namespace cgfs
