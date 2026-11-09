#include <cgfs/camera/camera.h>
#include <cgfs/math/mat4.h>
#include <cgfs/math/vec3.h>

#include <catch2/catch_test_macros.hpp>
#include <cmath>

#include "support/test_helpers.h"

using cgfs::Camera;
using cgfs::MakeCameraMatrix;
using cgfs::MakeOYRotationMatrix;
using cgfs::MakeTranslationMatrix;
using cgfs::Mat4;
using cgfs::Transposed;
using cgfs::Vec3;

namespace {

constexpr double kEpsilon = 1e-12;

bool Near(Vec3 a, Vec3 b) {
    return std::abs(a.x - b.x) < kEpsilon && std::abs(a.y - b.y) < kEpsilon &&
           std::abs(a.z - b.z) < kEpsilon;
}

}  // namespace

TEST_CASE("Camera: the default camera is at the origin looking along +Z, its matrix the identity",
          "[camera]") {
    const Camera camera;
    CHECK(camera.position == Vec3{});
    CHECK(camera.orientation == Mat4::Identity());
    CHECK(MakeCameraMatrix(camera.position, camera.orientation) == Mat4::Identity());
}

TEST_CASE("MakeCameraMatrix: a moved camera moves the world the opposite way", "[camera]") {
    const Mat4 m = MakeCameraMatrix({1.0, 2.0, 3.0}, Mat4::Identity());
    CHECK(m * Vec3{1.0, 2.0, 3.0} == Vec3{});               // the camera's own place is the origin
    CHECK(m * Vec3{1.0, 2.0, 8.0} == Vec3{0.0, 0.0, 5.0});  // 5 ahead of it stays 5 ahead
    CHECK(m * Vec3{} == Vec3{-1.0, -2.0, -3.0});
}

TEST_CASE("MakeCameraMatrix: a turned camera turns the world back", "[camera]") {
    // Turned 90 degrees the camera looks along -X: orientation * +Z.
    const Mat4 orientation = MakeOYRotationMatrix(90.0);
    CHECK(Near(orientation * Vec3{0.0, 0.0, 1.0}, {-1.0, 0.0, 0.0}));
    const Mat4 m = MakeCameraMatrix({}, orientation);
    CHECK(Near(m * Vec3{-5.0, 0.0, 0.0}, {0.0, 0.0, 5.0}));  // what it looks at is straight ahead
    CHECK(Near(m * Vec3{0.0, 0.0, 5.0}, {5.0, 0.0, 0.0}));   // what was ahead is now to its right
    CHECK(Near(m * Vec3{0.0, 2.0, 0.0}, {0.0, 2.0, 0.0}));   // up stays up
}

TEST_CASE("MakeCameraMatrix: the translation comes first, then the inverse rotation", "[camera]") {
    // The book's demo camera. A point d ahead along the camera's own line of sight,
    // position + orientation * (0, 0, d), must land on (0, 0, d), and the camera's own
    // axes on the world's.
    const Camera camera{.position = {-3.0, 1.0, 2.0}, .orientation = MakeOYRotationMatrix(-30.0)};
    const Mat4 m = MakeCameraMatrix(camera.position, camera.orientation);
    CHECK(Near(m * camera.position, {}));
    CHECK(Near(m * (camera.position + camera.orientation * Vec3{0.0, 0.0, 5.0}), {0.0, 0.0, 5.0}));
    CHECK(Near(m * (camera.position + camera.orientation * Vec3{1.0, 0.0, 0.0}), {1.0, 0.0, 0.0}));
    CHECK(Near(m * (camera.position + camera.orientation * Vec3{0.0, 1.0, 0.0}), {0.0, 1.0, 0.0}));
    CHECK(m == Transposed(camera.orientation) * MakeTranslationMatrix(-camera.position));
}
