#include <cgfs/math/bounding_sphere.h>
#include <cgfs/math/vec3.h>

#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <vector>

#include "support/test_helpers.h"

using cgfs::BoundingSphere;
using cgfs::Length;
using cgfs::MakeBoundingSphere;
using cgfs::Vec3;

namespace {

constexpr double kEpsilon = 1e-12;

// Every point is inside the sphere or on it.
bool ContainsAll(const BoundingSphere& sphere, const std::vector<Vec3>& points) {
    for (const Vec3 point : points) {
        if (Length(point - sphere.center) > sphere.radius + kEpsilon) {
            return false;
        }
    }
    return true;
}

}  // namespace

TEST_CASE("BoundingSphere: the default sphere contains everything", "[bounding_sphere]") {
    const BoundingSphere sphere;
    CHECK(sphere.center == Vec3{});
    CHECK(std::isinf(sphere.radius));
    CHECK(sphere.radius > 0.0);
    CHECK(ContainsAll(sphere, {{1e300, 0.0, 0.0}, {0.0, -1e300, 5.0}}));
}

TEST_CASE("MakeBoundingSphere: the book's cube has the origin and sqrt(3)", "[bounding_sphere]") {
    const std::vector<Vec3> cube = {{1.0, 1.0, 1.0},    {-1.0, 1.0, 1.0}, {-1.0, -1.0, 1.0},
                                    {1.0, -1.0, 1.0},   {1.0, 1.0, -1.0}, {-1.0, 1.0, -1.0},
                                    {-1.0, -1.0, -1.0}, {1.0, -1.0, -1.0}};
    const BoundingSphere sphere = MakeBoundingSphere(cube);
    CHECK(sphere.center == Vec3{});
    CHECK(sphere.radius == std::sqrt(3.0));
}

TEST_CASE(
    "MakeBoundingSphere: the center is the average and the radius reaches the farthest "
    "point",
    "[bounding_sphere]") {
    // A box that is not at the origin: 2 x 6 x 2 with its center at (3, 3, 3).
    const std::vector<Vec3> box = {{2.0, 0.0, 2.0}, {4.0, 0.0, 2.0}, {4.0, 0.0, 4.0},
                                   {2.0, 0.0, 4.0}, {2.0, 6.0, 2.0}, {4.0, 6.0, 2.0},
                                   {4.0, 6.0, 4.0}, {2.0, 6.0, 4.0}};
    const BoundingSphere sphere = MakeBoundingSphere(box);
    CHECK(sphere.center == Vec3{3.0, 3.0, 3.0});
    CHECK(sphere.radius == std::sqrt(11.0));  // half the diagonal: sqrt(1 + 9 + 1)
    CHECK(ContainsAll(sphere, box));
}

TEST_CASE("MakeBoundingSphere: contains every point, though not always as tightly as possible",
          "[bounding_sphere]") {
    // Three points bunched at the origin pull the average toward them: the center is
    // (1, 0, 0) and the radius 3, where the smallest sphere has (2, 0, 0) and 2.
    const std::vector<Vec3> lopsided = {{}, {}, {}, {4.0, 0.0, 0.0}};
    const BoundingSphere sphere = MakeBoundingSphere(lopsided);
    CHECK(sphere.center == Vec3{1.0, 0.0, 0.0});
    CHECK(sphere.radius == 3.0);
    CHECK(ContainsAll(sphere, lopsided));

    const std::vector<Vec3> scattered = {
        {0.3, -1.7, 2.9}, {-4.1, 0.2, 0.6}, {2.2, 2.2, -3.5}, {0.0, 5.5, 1.1}, {-0.9, -0.4, -0.2}};
    CHECK(ContainsAll(MakeBoundingSphere(scattered), scattered));
}

TEST_CASE("MakeBoundingSphere: one point is a sphere of radius 0, no points the origin",
          "[bounding_sphere]") {
    const std::vector<Vec3> one = {{1.0, 2.0, 3.0}};
    CHECK(MakeBoundingSphere(one) == BoundingSphere{.center = {1.0, 2.0, 3.0}, .radius = 0.0});
    CHECK(MakeBoundingSphere({}) == BoundingSphere{.center = {}, .radius = 0.0});
}
