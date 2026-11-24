#pragma once

#include <cgfs/math/vec3.h>

namespace cgfs {

/// A normal of the triangle with the corners A, B and C: a vector perpendicular to the
/// plane the three lie in,
///
///     N = (B - A) x (C - A)
///
/// the cross product of the two sides that leave A, the book's V1 x V2.
///
/// A plane has two sides and a perpendicular can be taken toward either; the order of
/// the corners says which. In the book's coordinates (x to the right, y up, z away from
/// the viewer) N points toward whoever sees the corners A, B, C run clockwise. A, B, C
/// and B, C, A and C, A, B are one order and give one normal; A, C, B is the other order
/// and gives the opposite one. A model that lists the corners of every triangle
/// clockwise as seen from outside the object therefore has normals that point out of it,
/// which is what back-face culling asks (see culling.h) and what lighting a face needs.
///
/// The length of N is twice the triangle's area, so it is the zero vector for a triangle
/// without area. It is not normalized here: which way it points can be asked without,
/// and whoever needs a unit normal calls Normalized.
[[nodiscard]] constexpr Vec3 TriangleNormal(Vec3 a, Vec3 b, Vec3 c) { return Cross(b - a, c - a); }

}  // namespace cgfs
