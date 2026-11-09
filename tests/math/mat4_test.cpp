#include <cgfs/math/mat4.h>
#include <cgfs/math/vec3.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>

#include "support/test_helpers.h"

using Catch::Matchers::WithinAbs;
using cgfs::Length;
using cgfs::MakeOYRotationMatrix;
using cgfs::MakeScalingMatrix;
using cgfs::MakeTranslationMatrix;
using cgfs::Mat4;
using cgfs::Transposed;
using cgfs::Vec3;

namespace {

constexpr double kEpsilon = 1e-12;

// Component-wise comparisons within kEpsilon, for results that went through cos and sin.
bool Near(Vec3 a, Vec3 b) {
    return std::abs(a.x - b.x) < kEpsilon && std::abs(a.y - b.y) < kEpsilon &&
           std::abs(a.z - b.z) < kEpsilon;
}
bool Near(const Mat4& a, const Mat4& b) {
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            if (std::abs(a.m[row][col] - b.m[row][col]) >= kEpsilon) {
                return false;
            }
        }
    }
    return true;
}

// A matrix with every entry different, for the product and transpose tests.
constexpr Mat4 kCounting{{{1.0, 2.0, 3.0, 4.0},
                          {5.0, 6.0, 7.0, 8.0},
                          {9.0, 10.0, 11.0, 12.0},
                          {13.0, 14.0, 15.0, 16.0}}};

}  // namespace

TEST_CASE("Mat4: all zeros by default, and Identity changes nothing", "[mat4]") {
    const Mat4 zero;
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            CHECK(zero.m[row][col] == 0.0);
        }
    }
    CHECK(Mat4::Identity() * kCounting == kCounting);
    CHECK(kCounting * Mat4::Identity() == kCounting);
    CHECK(Mat4::Identity() * Vec3{1.0, 2.0, 3.0} == Vec3{1.0, 2.0, 3.0});
}

TEST_CASE("Mat4: the product is the matrix product, row by column", "[mat4]") {
    const Mat4 a{
        {{1.0, 0.0, 2.0, 0.0}, {0.0, 1.0, 0.0, 3.0}, {4.0, 0.0, 1.0, 0.0}, {0.0, 0.0, 0.0, 1.0}}};
    const Mat4 b{
        {{1.0, 2.0, 0.0, 0.0}, {0.0, 1.0, 0.0, 0.0}, {5.0, 0.0, 1.0, 0.0}, {0.0, 0.0, 7.0, 1.0}}};
    const Mat4 ab{
        {{11.0, 2.0, 2.0, 0.0}, {0.0, 1.0, 21.0, 3.0}, {9.0, 8.0, 1.0, 0.0}, {0.0, 0.0, 7.0, 1.0}}};
    CHECK(a * b == ab);
    CHECK(a * b != b * a);
}

TEST_CASE("Mat4: A * B applies B first, then A", "[mat4]") {
    const Mat4 scale = MakeScalingMatrix(2.0);
    const Mat4 move = MakeTranslationMatrix({1.0, 0.0, 0.0});
    const Vec3 p{1.0, 1.0, 1.0};
    CHECK((move * scale) * p == Vec3{3.0, 2.0, 2.0});  // scaled to (2, 2, 2), then moved
    CHECK((scale * move) * p == Vec3{4.0, 2.0, 2.0});  // moved to (2, 1, 1), then scaled
    CHECK((move * scale) * p == move * (scale * p));
}

TEST_CASE("Mat4: a point comes back from homogeneous coordinates divided by w", "[mat4]") {
    // Bottom row (0, 0, 0, 2): w = 2, so every coordinate halves.
    Mat4 doubled_w = Mat4::Identity();
    doubled_w.m[3][3] = 2.0;
    CHECK(doubled_w * Vec3{2.0, 4.0, 6.0} == Vec3{1.0, 2.0, 3.0});

    // Bottom row (0, 0, 1, 0): w = z, the chapter's perspective matrix. The pipeline
    // keeps the projection out of its matrices on purpose, but the arithmetic is there.
    Mat4 perspective = Mat4::Identity();
    perspective.m[3][2] = 1.0;
    perspective.m[3][3] = 0.0;
    CHECK(perspective * Vec3{2.0, 4.0, 4.0} == Vec3{0.5, 1.0, 1.0});
}

TEST_CASE("Mat4: Transposed exchanges rows and columns", "[mat4]") {
    const Mat4 transposed = Transposed(kCounting);
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            CHECK(transposed.m[row][col] == kCounting.m[col][row]);
        }
    }
    CHECK(Transposed(transposed) == kCounting);
    CHECK(Transposed(Mat4::Identity()) == Mat4::Identity());
}

TEST_CASE("MakeScalingMatrix scales about the origin", "[mat4]") {
    CHECK(MakeScalingMatrix(2.0) * Vec3{1.0, -2.0, 3.0} == Vec3{2.0, -4.0, 6.0});
    CHECK(MakeScalingMatrix(0.5) * Vec3{} == Vec3{});  // the origin stays put
    CHECK(MakeScalingMatrix(1.0) == Mat4::Identity());
}

TEST_CASE("MakeTranslationMatrix moves every point by the same vector", "[mat4]") {
    const Mat4 move = MakeTranslationMatrix({1.0, 2.0, 3.0});
    CHECK(move * Vec3{} == Vec3{1.0, 2.0, 3.0});
    CHECK(move * Vec3{-1.0, -2.0, -3.0} == Vec3{});
    CHECK(move.m[0][3] == 1.0);  // the vector sits in the last column
    CHECK(move.m[1][3] == 2.0);
    CHECK(move.m[2][3] == 3.0);
    CHECK(MakeTranslationMatrix({}) == Mat4::Identity());
}

TEST_CASE("MakeOYRotationMatrix turns +X toward +Z and +Z toward -X", "[mat4]") {
    const Mat4 quarter = MakeOYRotationMatrix(90.0);
    CHECK(Near(quarter * Vec3{1.0, 0.0, 0.0}, {0.0, 0.0, 1.0}));
    CHECK(Near(quarter * Vec3{0.0, 0.0, 1.0}, {-1.0, 0.0, 0.0}));
    CHECK(quarter * Vec3{0.0, 1.0, 0.0} == Vec3{0.0, 1.0, 0.0});  // y is untouched, exactly
    CHECK(Near(MakeOYRotationMatrix(-90.0) * Vec3{1.0, 0.0, 0.0}, {0.0, 0.0, -1.0}));
    CHECK(MakeOYRotationMatrix(0.0) == Mat4::Identity());  // cos 0 = 1 and sin 0 = 0 exactly
    CHECK(Near(MakeOYRotationMatrix(360.0), Mat4::Identity()));

    // 30 degrees, the book's camera angle, against cos and sin by hand.
    const Mat4 r30 = MakeOYRotationMatrix(30.0);
    CHECK(Near(r30 * Vec3{1.0, 0.0, 0.0}, {std::sqrt(3.0) / 2.0, 0.0, 0.5}));
    CHECK(Near(r30 * Vec3{0.0, 0.0, 1.0}, {-0.5, 0.0, std::sqrt(3.0) / 2.0}));
}

TEST_CASE("MakeOYRotationMatrix: rotations compose, keep lengths and invert by transposition",
          "[mat4]") {
    CHECK(
        Near(MakeOYRotationMatrix(30.0) * MakeOYRotationMatrix(60.0), MakeOYRotationMatrix(90.0)));
    const Mat4 r = MakeOYRotationMatrix(195.0);
    const Vec3 p{1.0, 2.0, 3.0};
    CHECK_THAT(Length(r * p), WithinAbs(Length(p), kEpsilon));
    CHECK(Near(Transposed(r) * r, Mat4::Identity()));
    CHECK(Near(Transposed(r), MakeOYRotationMatrix(-195.0)));
}
