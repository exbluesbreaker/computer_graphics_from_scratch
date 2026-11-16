#include <cgfs/rasterizer/clipping.h>

#include <array>
#include <cassert>
#include <cstddef>
#include <limits>
#include <utility>

namespace cgfs {

namespace {

/// One corner of a triangle: which vertex it is, and the triangle's intensity there.
struct Corner {
    std::size_t v = 0;
    double h = 1.0;
};

Corner CornerOf(const Triangle& triangle, std::size_t k) { return {triangle.v[k], triangle.h[k]}; }

/// The new corner where the side from `front`, a corner in front of the plane, to
/// `behind`, a corner behind it or on it, crosses the plane: the book's
/// Intersection(AB, plane). The crossing point is appended to `vertices`, and the
/// corner's intensity is interpolated by the same fraction t as its position.
Corner Intersection(Corner front, Corner behind, const Plane& plane, std::vector<Vec3>& vertices) {
    const Vec3 a = vertices[front.v];
    const Vec3 b = vertices[behind.v];
    const double t = IntersectSegmentPlane(a, b, plane);
    vertices.push_back(a + t * (b - a));
    return {vertices.size() - 1, front.h + t * (behind.h - front.h)};
}

/// The triangle with the corners a, b and c, in that order, in `color`.
Triangle MakeTriangle(Corner a, Corner b, Corner c, Color color) {
    return {.v = {a.v, b.v, c.v}, .color = color, .h = {a.h, b.h, c.h}};
}

/// Remove the vertices that no triangle names and renumber the triangles to match. The
/// vertices that stay keep the order in which the triangles first name them.
void RemoveUnusedVertices(Model& instance) {
    constexpr std::size_t kUnused = std::numeric_limits<std::size_t>::max();
    std::vector<std::size_t> new_index(instance.vertices.size(), kUnused);
    std::vector<Vec3> used_vertices;
    for (Triangle& triangle : instance.triangles) {
        for (std::size_t& index : triangle.v) {
            if (new_index[index] == kUnused) {
                new_index[index] = used_vertices.size();
                used_vertices.push_back(instance.vertices[index]);
            }
            index = new_index[index];
        }
    }
    instance.vertices = std::move(used_vertices);
}

}  // namespace

void ClipTriangle(const Triangle& triangle, const Plane& plane, std::vector<Vec3>& vertices,
                  std::vector<Triangle>& clipped_triangles) {
    assert(triangle.v[0] < vertices.size() && triangle.v[1] < vertices.size() &&
           triangle.v[2] < vertices.size() && "ClipTriangle: a vertex index is out of range");
    const std::array<double, 3> d = {SignedDistance(plane, vertices[triangle.v[0]]),
                                     SignedDistance(plane, vertices[triangle.v[1]]),
                                     SignedDistance(plane, vertices[triangle.v[2]])};
    const int positive = (d[0] > 0.0 ? 1 : 0) + (d[1] > 0.0 ? 1 : 0) + (d[2] > 0.0 ? 1 : 0);
    const int negative = (d[0] < 0.0 ? 1 : 0) + (d[1] < 0.0 ? 1 : 0) + (d[2] < 0.0 ? 1 : 0);

    if (negative == 0) {
        // In front of the plane, or touching it from the front: the triangle itself.
        clipped_triangles.push_back(triangle);
    } else if (positive == 0) {
        // Behind the plane, or touching it from behind: nothing.
    } else if (positive == 1) {
        // Let A be the corner with a positive distance; B and C follow it around the
        // triangle, so that A B C is the triangle with its corners rotated.
        const std::size_t k = d[0] > 0.0 ? 0 : (d[1] > 0.0 ? 1 : 2);
        const Corner a = CornerOf(triangle, k);
        const Corner b = CornerOf(triangle, (k + 1) % 3);
        const Corner c = CornerOf(triangle, (k + 2) % 3);
        const Corner b_prime = Intersection(a, b, plane, vertices);
        const Corner c_prime = Intersection(a, c, plane, vertices);
        clipped_triangles.push_back(MakeTriangle(a, b_prime, c_prime, triangle.color));
    } else {
        // Two corners in front and so the third behind. Let C be the corner with a
        // negative distance; A and B follow it around the triangle.
        const std::size_t k = d[0] < 0.0 ? 0 : (d[1] < 0.0 ? 1 : 2);
        const Corner c = CornerOf(triangle, k);
        const Corner a = CornerOf(triangle, (k + 1) % 3);
        const Corner b = CornerOf(triangle, (k + 2) % 3);
        const Corner a_prime = Intersection(a, c, plane, vertices);
        const Corner b_prime = Intersection(b, c, plane, vertices);
        clipped_triangles.push_back(MakeTriangle(a, b, a_prime, triangle.color));
        clipped_triangles.push_back(MakeTriangle(a_prime, b, b_prime, triangle.color));
    }
}

std::vector<Triangle> ClipTrianglesAgainstPlane(std::span<const Triangle> triangles,
                                                const Plane& plane, std::vector<Vec3>& vertices) {
    std::vector<Triangle> clipped_triangles;
    clipped_triangles.reserve(triangles.size());
    for (const Triangle& triangle : triangles) {
        ClipTriangle(triangle, plane, vertices, clipped_triangles);
    }
    return clipped_triangles;
}

std::optional<Model> ClipInstanceAgainstPlane(Model instance, const Plane& plane) {
    const double d = SignedDistance(plane, instance.bounding_sphere.center);
    const double r = instance.bounding_sphere.radius;
    if (d > r) {
        return instance;
    }
    if (d < -r) {
        return std::nullopt;
    }
    instance.triangles = ClipTrianglesAgainstPlane(instance.triangles, plane, instance.vertices);
    RemoveUnusedVertices(instance);
    return instance;
}

std::optional<Model> ClipInstance(Model instance, std::span<const Plane> planes) {
    for (const Plane& plane : planes) {
        std::optional<Model> clipped_instance =
            ClipInstanceAgainstPlane(std::move(instance), plane);
        if (!clipped_instance) {
            return std::nullopt;
        }
        instance = std::move(*clipped_instance);
    }
    return instance;
}

std::vector<Model> ClipScene(std::vector<Model> scene, std::span<const Plane> planes) {
    std::vector<Model> clipped_instances;
    clipped_instances.reserve(scene.size());
    for (Model& instance : scene) {
        std::optional<Model> clipped_instance = ClipInstance(std::move(instance), planes);
        if (clipped_instance) {
            clipped_instances.push_back(std::move(*clipped_instance));
        }
    }
    return clipped_instances;
}

}  // namespace cgfs
