#include <cgfs/rasterizer/culling.h>

#include <cassert>
#include <vector>

namespace cgfs {

namespace {

/// A normal of the triangle A B C: the cross product of the two sides that leave A, the
/// book's V1 x V2. It is as long as twice the triangle's area, which does not matter to
/// a test that asks only which way it points.
Vec3 TriangleNormal(Vec3 a, Vec3 b, Vec3 c) { return Cross(b - a, c - a); }

}  // namespace

bool IsBackFacing(Vec3 a, Vec3 b, Vec3 c, Vec3 camera_position) {
    const Vec3 normal = TriangleNormal(a, b, c);
    const Vec3 to_camera = camera_position - a;
    return Dot(normal, to_camera) <= 0.0;
}

Model CullBackFaces(Model object, Vec3 camera_position) {
    const std::vector<Vec3>& vertices = object.vertices;
    std::erase_if(object.triangles, [&](const Triangle& triangle) {
        assert(triangle.v[0] < vertices.size() && triangle.v[1] < vertices.size() &&
               triangle.v[2] < vertices.size() && "CullBackFaces: a vertex index is out of range");
        return IsBackFacing(vertices[triangle.v[0]], vertices[triangle.v[1]],
                            vertices[triangle.v[2]], camera_position);
    });
    return object;
}

}  // namespace cgfs
