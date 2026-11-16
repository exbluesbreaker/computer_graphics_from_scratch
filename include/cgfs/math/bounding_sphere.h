#pragma once

#include <cgfs/math/vec3.h>

#include <algorithm>
#include <limits>
#include <span>

namespace cgfs {

/// A sphere that contains an object: every point of the object is at most `radius` from
/// `center`.
///
/// It stands in for the object when a quick answer is enough. Clipping asks on which side
/// of a plane an object lies, and for a sphere that is one signed distance: the sphere is
/// entirely in front of the plane when its center is more than a radius in front,
/// entirely behind when it is more than a radius behind, and only in between does the
/// object itself have to be looked at, triangle by triangle.
///
/// The default is the sphere that contains everything, an infinite radius: always a
/// correct bound, and one that never saves any work. MakeBoundingSphere computes a tight
/// one.
struct BoundingSphere {
    Vec3 center;
    double radius = std::numeric_limits<double>::infinity();

    bool operator==(const BoundingSphere&) const = default;
};

/// A bounding sphere of `points`, the book's approximation: the center is the average of
/// the points and the radius the distance from it to the point farthest away.
///
/// Every point is inside by construction. The sphere is not always the smallest one (a
/// few points bunched together pull the average toward them, and the far ones then need
/// a larger radius), but it is exact for a shape that is symmetric about its center,
/// such as a cube or a box, and cheap for any shape. For no points at all the result is
/// the origin with radius 0.
[[nodiscard]] inline BoundingSphere MakeBoundingSphere(std::span<const Vec3> points) {
    if (points.empty()) {
        return {.center = {}, .radius = 0.0};
    }
    Vec3 sum;
    for (const Vec3 point : points) {
        sum += point;
    }
    const Vec3 center = sum / static_cast<double>(points.size());
    double radius = 0.0;
    for (const Vec3 point : points) {
        radius = std::max(radius, Length(point - center));
    }
    return {.center = center, .radius = radius};
}

}  // namespace cgfs
