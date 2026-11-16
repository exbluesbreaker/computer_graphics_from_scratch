#pragma once

#include <cgfs/math/vec3.h>

namespace cgfs {

/// A plane in 3D: the points P with
///
///     <N, P> + D = 0
///
/// `normal` is N, a unit vector perpendicular to the plane, and `d` is the book's D: minus
/// the signed distance from the origin to the plane along N. The plane z = 1 is
/// N = (0, 0, 1), D = -1.
///
/// A plane cuts space in two, and the normal says which half is which: the half N points
/// into is "in front" of the plane and the other "behind" it. Clipping keeps what is in
/// front, so a clipping plane's normal points into the volume that is kept.
///
/// Precondition wherever a distance is meant: |normal| = 1. Plain data, not checked.
struct Plane {
    Vec3 normal;
    double d = 0.0;

    bool operator==(const Plane&) const = default;
};

/// The signed distance from the plane to `vertex`: <N, vertex> + D, the left side of the
/// plane's equation. Zero on the plane, positive in front of it (the side the normal
/// points to) and negative behind it, and for a unit normal its size is the distance
/// itself; the book's SignedDistance.
[[nodiscard]] constexpr double SignedDistance(const Plane& plane, Vec3 vertex) {
    return Dot(plane.normal, vertex) + plane.d;
}

/// Where the segment AB crosses the plane, as the fraction t of the way from A to B: the
/// crossing point is Q = A + t (B - A).
///
/// A point of the segment is P = A + t (B - A). Putting it into the plane's equation,
/// <N, A + t (B - A)> + D = 0, and solving for t gives the book's
///
///     t = (-D - <N, A>) / <N, B - A>
///
/// The same t serves every value that varies linearly along the segment: an attribute
/// that is `a` at A and `b` at B is `a + t (b - a)` at Q, which is how a clipped triangle
/// gets the intensity of its new corners. t is in [0, 1] when A and B are on opposite
/// sides of the plane, 0 when A is on it and 1 when B is; outside [0, 1] the line through
/// A and B crosses the plane beyond the segment.
///
/// Precondition: <N, B - A> != 0, that is, AB is not parallel to the plane. It holds
/// whenever A and B are at different signed distances.
[[nodiscard]] constexpr double IntersectSegmentPlane(Vec3 a, Vec3 b, const Plane& plane) {
    return (-plane.d - Dot(plane.normal, a)) / Dot(plane.normal, b - a);
}

}  // namespace cgfs
