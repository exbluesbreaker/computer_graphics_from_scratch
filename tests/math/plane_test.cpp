#include <cgfs/math/plane.h>
#include <cgfs/math/vec3.h>

#include <catch2/catch_test_macros.hpp>
#include <cmath>

#include "support/test_helpers.h"

using cgfs::IntersectSegmentPlane;
using cgfs::Plane;
using cgfs::SignedDistance;
using cgfs::Vec3;

namespace {

constexpr double kEpsilon = 1e-12;

bool Near(double a, double b) { return std::abs(a - b) < kEpsilon; }

// The book's near plane for a viewport at distance 1: z = 1, with +Z in front.
constexpr Plane kNear{.normal = {0.0, 0.0, 1.0}, .d = -1.0};

}  // namespace

TEST_CASE("SignedDistance: positive in front of the plane, negative behind it, zero on it",
          "[plane]") {
    CHECK(SignedDistance(kNear, {0.0, 0.0, 3.0}) == 2.0);
    // Sliding along the plane changes nothing.
    CHECK(SignedDistance(kNear, {5.0, -7.0, 3.0}) == 2.0);
    CHECK(SignedDistance(kNear, {5.0, -7.0, 1.0}) == 0.0);
    CHECK(SignedDistance(kNear, {0.0, 0.0, -2.0}) == -3.0);
    // The origin is at D: the plane z = 1 is 1 in front of it.
    CHECK(SignedDistance(kNear, {}) == kNear.d);
}

TEST_CASE("SignedDistance: the normal says which side is the front", "[plane]") {
    // The same plane z = 1 with its normal the other way round: the sides are exchanged.
    const Plane facing_back{.normal = {0.0, 0.0, -1.0}, .d = 1.0};
    CHECK(SignedDistance(facing_back, {0.0, 0.0, 3.0}) == -2.0);
    CHECK(SignedDistance(facing_back, {0.0, 0.0, 1.0}) == 0.0);
    CHECK(SignedDistance(facing_back, {0.0, 0.0, -2.0}) == 3.0);
}

TEST_CASE("SignedDistance: a slanted plane through the origin", "[plane]") {
    // The unit normal (0.6, 0, 0.8), from the 3-4-5 triangle.
    const Plane plane{.normal = {0.6, 0.0, 0.8}, .d = 0.0};
    CHECK(Near(SignedDistance(plane, {0.6, 0.0, 0.8}), 1.0));   // one unit along the normal
    CHECK(Near(SignedDistance(plane, {3.0, 0.0, 4.0}), 5.0));   // five units along it
    CHECK(Near(SignedDistance(plane, {-0.8, 9.0, 0.6}), 0.0));  // perpendicular to it: on the plane
    CHECK(Near(SignedDistance(plane, {-3.0, 0.0, -4.0}), -5.0));
}

TEST_CASE("SignedDistance: is a distance only for a unit normal", "[plane]") {
    // The plane z = 1 written with a normal of length 2: the same points are on it, and
    // on the same sides, but the value is twice the distance.
    const Plane doubled{.normal = {0.0, 0.0, 2.0}, .d = -2.0};
    CHECK(SignedDistance(doubled, {0.0, 0.0, 1.0}) == 0.0);
    CHECK(SignedDistance(doubled, {0.0, 0.0, 3.0}) == 4.0);
    CHECK(SignedDistance(doubled, {0.0, 0.0, -2.0}) == -6.0);
}

TEST_CASE("IntersectSegmentPlane: the fraction of the way from A to B", "[plane]") {
    // From z = 0 to z = 4 the plane z = 1 is a quarter of the way.
    const Vec3 a{0.0, 0.0, 0.0};
    const Vec3 b{0.0, 0.0, 4.0};
    CHECK(IntersectSegmentPlane(a, b, kNear) == 0.25);
    CHECK(IntersectSegmentPlane(b, a, kNear) == 0.75);  // and three quarters from the other end
}

TEST_CASE("IntersectSegmentPlane: A + t (B - A) lies on the plane", "[plane]") {
    const Vec3 a{2.0, -2.0, 3.0};
    const Vec3 b{-2.0, 6.0, -1.0};
    const double t = IntersectSegmentPlane(a, b, kNear);
    CHECK(t == 0.5);
    const Vec3 q = a + t * (b - a);
    CHECK(q == Vec3{0.0, 2.0, 1.0});
    CHECK(SignedDistance(kNear, q) == 0.0);

    // A slanted plane and a segment that crosses it at no round fraction.
    const Plane slanted{.normal = {0.6, 0.0, 0.8}, .d = -2.0};
    const Vec3 c{1.0, 2.0, 5.0};
    const Vec3 e{-3.0, 1.0, -2.0};
    const double u = IntersectSegmentPlane(c, e, slanted);
    CHECK(u > 0.0);
    CHECK(u < 1.0);
    CHECK(Near(SignedDistance(slanted, c + u * (e - c)), 0.0));
}

TEST_CASE("IntersectSegmentPlane: 0 at A, 1 at B, and outside [0, 1] beyond the segment",
          "[plane]") {
    CHECK(IntersectSegmentPlane({0.0, 0.0, 1.0}, {0.0, 0.0, 5.0}, kNear) == 0.0);  // A on the plane
    CHECK(IntersectSegmentPlane({0.0, 0.0, 5.0}, {0.0, 0.0, 1.0}, kNear) == 1.0);  // B on the plane
    // Both ends in front: the line crosses the plane before A, or past B.
    CHECK(IntersectSegmentPlane({0.0, 0.0, 2.0}, {0.0, 0.0, 3.0}, kNear) == -1.0);
    CHECK(IntersectSegmentPlane({0.0, 0.0, 3.0}, {0.0, 0.0, 2.0}, kNear) == 2.0);
}

TEST_CASE("Plane: the functions are usable in constant expressions", "[plane]") {
    static_assert(SignedDistance(kNear, Vec3{0.0, 0.0, 3.0}) == 2.0);
    static_assert(IntersectSegmentPlane(Vec3{}, Vec3{0.0, 0.0, 4.0}, kNear) == 0.25);
    SUCCEED();
}
