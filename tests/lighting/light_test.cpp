#include <cgfs/camera/camera.h>
#include <cgfs/lighting/light.h>
#include <cgfs/math/mat4.h>
#include <cgfs/math/vec3.h>

#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <variant>

#include "support/test_helpers.h"

using cgfs::AmbientLight;
using cgfs::DirectionalLight;
using cgfs::Light;
using cgfs::MakeCameraMatrix;
using cgfs::MakeOYRotationMatrix;
using cgfs::MakeScalingMatrix;
using cgfs::MakeTranslationMatrix;
using cgfs::Mat4;
using cgfs::PointLight;
using cgfs::TransformLight;
using cgfs::Vec3;

namespace {

bool Near(Vec3 a, Vec3 b) {
    return std::abs(a.x - b.x) < 1e-12 && std::abs(a.y - b.y) < 1e-12 &&
           std::abs(a.z - b.z) < 1e-12;
}

const Mat4 kMoved = MakeTranslationMatrix({1.0, 2.0, 3.0});
const Mat4 kQuarterTurn = MakeOYRotationMatrix(90.0);  // +X to +Z, +Z to -X

}  // namespace

TEST_CASE("Light: two lights are equal when they are the same kind with the same values",
          "[lighting]") {
    CHECK(Light{AmbientLight{.intensity = 0.2}} == Light{AmbientLight{.intensity = 0.2}});
    CHECK_FALSE(Light{AmbientLight{.intensity = 0.2}} == Light{AmbientLight{.intensity = 0.3}});
    CHECK_FALSE(Light{PointLight{.intensity = 0.2}} == Light{DirectionalLight{.intensity = 0.2}});
    CHECK_FALSE(Light{PointLight{.intensity = 0.2, .position = {1.0, 0.0, 0.0}}} ==
                Light{PointLight{.intensity = 0.2, .position = {0.0, 1.0, 0.0}}});
}

TEST_CASE("TransformLight: ambient light is the same in all coordinates", "[lighting]") {
    const Light ambient = AmbientLight{.intensity = 0.2};
    CHECK(TransformLight(ambient, kMoved) == ambient);
    CHECK(TransformLight(ambient, kMoved * kQuarterTurn * MakeScalingMatrix(3.0)) == ambient);
}

TEST_CASE("TransformLight: a point light's position is transformed as a point", "[lighting]") {
    const Light lamp = PointLight{.intensity = 0.6, .position = {4.0, 5.0, 6.0}};
    CHECK(TransformLight(lamp, kMoved) ==
          Light{PointLight{.intensity = 0.6, .position = {5.0, 7.0, 9.0}}});
    CHECK(TransformLight(lamp, Mat4::Identity()) == lamp);

    const Light turned = TransformLight(lamp, kQuarterTurn);
    REQUIRE(std::holds_alternative<PointLight>(turned));
    CHECK(Near(std::get<PointLight>(turned).position, {-6.0, 5.0, 4.0}));
    CHECK(std::get<PointLight>(turned).intensity == 0.6);
}

TEST_CASE("TransformLight: a directional light is turned and not moved", "[lighting]") {
    const Light sun = DirectionalLight{.intensity = 0.3, .direction = {1.0, 4.0, 4.0}};
    // The light is infinitely far away: wherever the scene is moved to, it comes from
    // the same direction.
    CHECK(TransformLight(sun, kMoved) == sun);

    const Light turned = TransformLight(sun, kMoved * kQuarterTurn);
    REQUIRE(std::holds_alternative<DirectionalLight>(turned));
    CHECK(Near(std::get<DirectionalLight>(turned).direction, {-4.0, 4.0, 1.0}));
    CHECK(std::get<DirectionalLight>(turned).intensity == 0.3);
}

TEST_CASE("TransformLight: a camera matrix takes a light into the camera's coordinates",
          "[lighting]") {
    // A camera at (4, 5, 6) turned 90 degrees to its left looks along -X.
    const Mat4 orientation = MakeOYRotationMatrix(90.0);
    const Vec3 position{4.0, 5.0, 6.0};
    const Mat4 camera_matrix = MakeCameraMatrix(position, orientation);

    // A lamp where the camera stands is at the origin of the camera's coordinates, and
    // one 2 ahead of the camera is 2 along its +Z.
    const Light at_camera =
        TransformLight(PointLight{.intensity = 1.0, .position = position}, camera_matrix);
    CHECK(Near(std::get<PointLight>(at_camera).position, {0.0, 0.0, 0.0}));
    const Light ahead = TransformLight(
        PointLight{.intensity = 1.0, .position = position + Vec3{-2.0, 0.0, 0.0}}, camera_matrix);
    CHECK(Near(std::get<PointLight>(ahead).position, {0.0, 0.0, 2.0}));

    // A sun the camera looks straight at, in the world's -X, is straight ahead of it,
    // wherever the camera stands.
    const Light sun = TransformLight(
        DirectionalLight{.intensity = 1.0, .direction = {-1.0, 0.0, 0.0}}, camera_matrix);
    CHECK(Near(std::get<DirectionalLight>(sun).direction, {0.0, 0.0, 1.0}));
}
