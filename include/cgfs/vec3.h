#pragma once

#include <cmath>

namespace cgfs {

/// A point or a direction in 3D space.
///
/// Plain value type: three doubles and the usual arithmetic. Free functions provide
/// the dot and cross products, the length and normalization.
struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    [[nodiscard]] constexpr Vec3 operator-() const { return {-x, -y, -z}; }

    constexpr Vec3& operator+=(Vec3 o) {
        x += o.x;
        y += o.y;
        z += o.z;
        return *this;
    }
    constexpr Vec3& operator-=(Vec3 o) {
        x -= o.x;
        y -= o.y;
        z -= o.z;
        return *this;
    }
    constexpr Vec3& operator*=(double s) {
        x *= s;
        y *= s;
        z *= s;
        return *this;
    }
    constexpr Vec3& operator/=(double s) {
        x /= s;
        y /= s;
        z /= s;
        return *this;
    }

    bool operator==(const Vec3&) const = default;
};

[[nodiscard]] constexpr Vec3 operator+(Vec3 a, Vec3 b) { return a += b; }
[[nodiscard]] constexpr Vec3 operator-(Vec3 a, Vec3 b) { return a -= b; }
[[nodiscard]] constexpr Vec3 operator*(Vec3 v, double s) { return v *= s; }
[[nodiscard]] constexpr Vec3 operator*(double s, Vec3 v) { return v *= s; }
[[nodiscard]] constexpr Vec3 operator/(Vec3 v, double s) { return v /= s; }

/// <a, b>: the sum of the products of matching components.
[[nodiscard]] constexpr double Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

/// a x b: the vector perpendicular to both a and b.
[[nodiscard]] constexpr Vec3 Cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

/// |v| = sqrt(<v, v>).
[[nodiscard]] inline double Length(Vec3 v) { return std::sqrt(Dot(v, v)); }

/// v scaled to unit length. Precondition: Length(v) > 0.
[[nodiscard]] inline Vec3 Normalized(Vec3 v) { return v / Length(v); }

}  // namespace cgfs
