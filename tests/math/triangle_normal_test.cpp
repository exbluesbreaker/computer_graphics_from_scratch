#include <cgfs/math/triangle_normal.h>
#include <cgfs/math/vec3.h>

#include <catch2/catch_test_macros.hpp>
#include <cmath>

#include "support/test_helpers.h"

using cgfs::Dot;
using cgfs::Length;
using cgfs::TriangleNormal;
using cgfs::Vec3;

namespace {

// A triangle 5 ahead of a viewer at the origin who looks along +Z with x to the right
// and y up: top, bottom right, bottom left is clockwise as that viewer sees it.
constexpr Vec3 kTop{0.0, 1.0, 5.0};
constexpr Vec3 kBottomRight{1.0, -1.0, 5.0};
constexpr Vec3 kBottomLeft{-1.0, -1.0, 5.0};

}  // namespace

TEST_CASE(
    "TriangleNormal: corners that run clockwise for a viewer give a normal toward the "
    "viewer",
    "[triangle_normal]") {
    CHECK(TriangleNormal(kTop, kBottomRight, kBottomLeft) == Vec3{0.0, 0.0, -4.0});
    // The same three points the other way round: the same plane, the opposite normal.
    CHECK(TriangleNormal(kTop, kBottomLeft, kBottomRight) == Vec3{0.0, 0.0, 4.0});
}

TEST_CASE("TriangleNormal: the corner the list starts from does not matter", "[triangle_normal]") {
    const Vec3 normal = TriangleNormal(kTop, kBottomRight, kBottomLeft);
    CHECK(TriangleNormal(kBottomRight, kBottomLeft, kTop) == normal);
    CHECK(TriangleNormal(kBottomLeft, kTop, kBottomRight) == normal);
}

TEST_CASE("TriangleNormal: perpendicular to the sides and as long as twice the area",
          "[triangle_normal]") {
    // A right triangle with the legs 3 and 4 in a tilted plane: its area is 6.
    const Vec3 a{1.0, 2.0, 3.0};
    const Vec3 b = a + Vec3{0.0, 3.0, 0.0};
    const Vec3 c = a + Vec3{2.4, 0.0, 3.2};  // a leg of length 4
    const Vec3 normal = TriangleNormal(a, b, c);
    CHECK(Dot(normal, b - a) == 0.0);
    CHECK(std::abs(Dot(normal, c - a)) < 1e-12);
    CHECK(std::abs(Dot(normal, c - b)) < 1e-12);
    CHECK(std::abs(Length(normal) - 12.0) < 1e-12);
}

TEST_CASE("TriangleNormal: a floor listed clockwise as seen from above faces up",
          "[triangle_normal]") {
    // Near left, far left, far right, with z away from the viewer.
    CHECK(TriangleNormal({-1.0, 0.0, -1.0}, {-1.0, 0.0, 1.0}, {1.0, 0.0, 1.0}) ==
          Vec3{0.0, 4.0, 0.0});
}

TEST_CASE("TriangleNormal: a triangle without area has no normal", "[triangle_normal]") {
    CHECK(TriangleNormal({0.0, 0.0, 1.0}, {1.0, 1.0, 2.0}, {2.0, 2.0, 3.0}) == Vec3{});  // a line
    CHECK(TriangleNormal(kTop, kTop, kBottomLeft) == Vec3{});
    static_assert(TriangleNormal(kTop, kBottomRight, kBottomLeft).z == -4.0,
                  "usable in a constant expression");
}
