#pragma once

#include <cgfs/math/vec3.h>

#include <cmath>
#include <numbers>

namespace cgfs {

/// A 4 x 4 matrix of doubles, `m[row][col]`: the transforms of the rasterizer's 3D
/// pipeline, in homogeneous coordinates.
///
/// A 3D point (x, y, z) is written as the column (x, y, z, 1). The fourth coordinate
/// makes a translation, which is not linear in (x, y, z), a matrix like a rotation or a
/// scaling, so a chain of transforms collapses into one matrix product that is applied
/// once per vertex. Every matrix this project builds is affine, with (0, 0, 0, 1) as its
/// bottom row, so a transformed point keeps w = 1. The perspective projection is
/// deliberately not one of them (see ProjectVertex): the book keeps the projection a
/// separate step after the transforms, so that work which is not a matrix transform,
/// clipping first of all, fits in between.
///
/// Plain value type, all zeros by default; Identity() is the matrix that changes nothing.
struct Mat4 {
    double m[4][4]{};

    [[nodiscard]] static constexpr Mat4 Identity() {
        return {{{1.0, 0.0, 0.0, 0.0},
                 {0.0, 1.0, 0.0, 0.0},
                 {0.0, 0.0, 1.0, 0.0},
                 {0.0, 0.0, 0.0, 1.0}}};
    }

    bool operator==(const Mat4&) const = default;
};

/// The matrix product A B: the transform that applies B first and then A. A chain of
/// transforms therefore reads right to left: `translation * rotation * scaling` scales,
/// then rotates, then translates.
[[nodiscard]] constexpr Mat4 operator*(const Mat4& a, const Mat4& b) {
    Mat4 product;
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            for (int k = 0; k < 4; ++k) {
                product.m[row][col] += a.m[row][k] * b.m[k][col];
            }
        }
    }
    return product;
}

/// M applied to the point p: M (p.x, p.y, p.z, 1) = (x, y, z, w), returned as the
/// Cartesian point (x / w, y / w, z / w), the way back from homogeneous coordinates. For
/// an affine M (bottom row 0 0 0 1) w is exactly 1 and the division changes nothing.
/// Precondition: w != 0.
[[nodiscard]] constexpr Vec3 operator*(const Mat4& m, Vec3 p) {
    // The last column is multiplied by the point's fourth coordinate, 1.
    const double x = m.m[0][0] * p.x + m.m[0][1] * p.y + m.m[0][2] * p.z + m.m[0][3];
    const double y = m.m[1][0] * p.x + m.m[1][1] * p.y + m.m[1][2] * p.z + m.m[1][3];
    const double z = m.m[2][0] * p.x + m.m[2][1] * p.y + m.m[2][2] * p.z + m.m[2][3];
    const double w = m.m[3][0] * p.x + m.m[3][1] * p.y + m.m[3][2] * p.z + m.m[3][3];
    return {x / w, y / w, z / w};
}

/// M with its rows and columns exchanged. A rotation matrix is orthonormal, so its
/// transpose is its inverse: the same rotation the other way.
[[nodiscard]] constexpr Mat4 Transposed(const Mat4& m) {
    Mat4 transposed;
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            transposed.m[row][col] = m.m[col][row];
        }
    }
    return transposed;
}

/// Scales every coordinate by `scale`, about the origin: diag(scale, scale, scale, 1).
[[nodiscard]] constexpr Mat4 MakeScalingMatrix(double scale) {
    return {{{scale, 0.0, 0.0, 0.0},
             {0.0, scale, 0.0, 0.0},
             {0.0, 0.0, scale, 0.0},
             {0.0, 0.0, 0.0, 1.0}}};
}

/// Moves every point by `translation`: the identity with (tx, ty, tz) as its last
/// column, so that M (x, y, z, 1) = (x + tx, y + ty, z + tz, 1). This is what the fourth
/// coordinate is for: the point's 1 picks up the column.
[[nodiscard]] constexpr Mat4 MakeTranslationMatrix(Vec3 translation) {
    return {{{1.0, 0.0, 0.0, translation.x},
             {0.0, 1.0, 0.0, translation.y},
             {0.0, 0.0, 1.0, translation.z},
             {0.0, 0.0, 0.0, 1.0}}};
}

/// Rotates about the Y axis by `degrees`, the book's demo matrix:
///
///     [ cos  0  -sin  0 ]
///     [  0   1    0   0 ]
///     [ sin  0   cos  0 ]
///     [  0   0    0   1 ]
///
/// so x' = x cos - z sin, y' = y and z' = x sin + z cos: a positive angle turns the +X
/// axis toward +Z and the +Z axis toward -X, and y does not change. In degrees, as in
/// the book's demos; the one rotation the chapter needs, the others would be built the
/// same way. Not constexpr: it calls cos and sin.
[[nodiscard]] inline Mat4 MakeOYRotationMatrix(double degrees) {
    const double radians = degrees * std::numbers::pi / 180.0;
    const double cosine = std::cos(radians);
    const double sine = std::sin(radians);
    return {{{cosine, 0.0, -sine, 0.0},
             {0.0, 1.0, 0.0, 0.0},
             {sine, 0.0, cosine, 0.0},
             {0.0, 0.0, 0.0, 1.0}}};
}

}  // namespace cgfs
