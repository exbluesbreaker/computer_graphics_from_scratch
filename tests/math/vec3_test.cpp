#include <cgfs/math/vec3.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "support/test_helpers.h"

using Catch::Matchers::WithinAbs;
using cgfs::Cross;
using cgfs::Dot;
using cgfs::Length;
using cgfs::Normalized;
using cgfs::Reflect;
using cgfs::Vec3;

TEST_CASE("Vec3 default-constructs to zero", "[vec3]") {
    CHECK(Vec3{} == Vec3{0.0, 0.0, 0.0});
}

TEST_CASE("Vec3 arithmetic", "[vec3]") {
    const Vec3 a{1.0, 2.0, 3.0};
    const Vec3 b{4.0, 5.0, 6.0};

    CHECK(a + b == Vec3{5.0, 7.0, 9.0});
    CHECK(b - a == Vec3{3.0, 3.0, 3.0});
    CHECK(-a == Vec3{-1.0, -2.0, -3.0});
    CHECK(a * 2.0 == Vec3{2.0, 4.0, 6.0});
    CHECK(2.0 * a == Vec3{2.0, 4.0, 6.0});
    CHECK(b / 2.0 == Vec3{2.0, 2.5, 3.0});
}

TEST_CASE("Vec3 compound assignment", "[vec3]") {
    Vec3 v{1.0, 2.0, 3.0};
    v += Vec3{1.0, 1.0, 1.0};
    CHECK(v == Vec3{2.0, 3.0, 4.0});
    v -= Vec3{2.0, 2.0, 2.0};
    CHECK(v == Vec3{0.0, 1.0, 2.0});
    v *= 3.0;
    CHECK(v == Vec3{0.0, 3.0, 6.0});
    v /= 3.0;
    CHECK(v == Vec3{0.0, 1.0, 2.0});
}

TEST_CASE("Dot product", "[vec3]") {
    CHECK(Dot(Vec3{1.0, 2.0, 3.0}, Vec3{4.0, 5.0, 6.0}) == 32.0);
    CHECK(Dot(Vec3{1.0, 0.0, 0.0}, Vec3{0.0, 1.0, 0.0}) == 0.0);
    static_assert(Dot(Vec3{1.0, 2.0, 3.0}, Vec3{1.0, 2.0, 3.0}) == 14.0, "Dot is constexpr");
}

TEST_CASE("Cross product", "[vec3]") {
    const Vec3 x{1.0, 0.0, 0.0};
    const Vec3 y{0.0, 1.0, 0.0};
    const Vec3 z{0.0, 0.0, 1.0};
    CHECK(Cross(x, y) == z);
    CHECK(Cross(y, x) == -z);
    CHECK(Cross(x, x) == Vec3{});
}

TEST_CASE("Length and Normalized", "[vec3]") {
    CHECK(Length(Vec3{3.0, 4.0, 0.0}) == 5.0);
    CHECK(Length(Vec3{}) == 0.0);
    CHECK(Normalized(Vec3{0.0, 0.0, 5.0}) == Vec3{0.0, 0.0, 1.0});
    CHECK_THAT(Length(Normalized(Vec3{1.0, 2.0, 3.0})), WithinAbs(1.0, 1e-12));
}

TEST_CASE("Reflect mirrors a vector about the normal", "[vec3]") {
    const Vec3 up{0.0, 1.0, 0.0};
    CHECK(Reflect(Vec3{1.0, 1.0, 0.0}, up) == Vec3{-1.0, 1.0, 0.0});  // 45 degrees in, 45 out
    CHECK(Reflect(up, up) == up);                                      // along the normal
    CHECK(Reflect(Vec3{1.0, 0.0, 0.0}, up) == Vec3{-1.0, 0.0, 0.0});  // grazing: reversed
    CHECK(Reflect(Vec3{3.0, 2.0, -1.0}, up) == Vec3{-3.0, 2.0, 1.0});
    static_assert(Reflect(Vec3{1.0, 1.0, 0.0}, Vec3{0.0, 1.0, 0.0}) == Vec3{-1.0, 1.0, 0.0},
                  "Reflect is constexpr");
}
