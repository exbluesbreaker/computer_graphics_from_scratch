#include <cgfs/lighting/illumination.h>
#include <cgfs/lighting/light.h>
#include <cgfs/math/mat4.h>
#include <cgfs/math/vec3.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <vector>

#include "support/test_helpers.h"

using Catch::Matchers::WithinAbs;
using cgfs::AmbientLight;
using cgfs::ComputeIllumination;
using cgfs::DiffuseReflection;
using cgfs::DirectionalLight;
using cgfs::kMatte;
using cgfs::Light;
using cgfs::PointLight;
using cgfs::SpecularReflection;
using cgfs::Vec3;

namespace {

const Vec3 kUp{0.0, 1.0, 0.0};

// The specular tests share one setup: a surface facing up, lit from 45 degrees along
// L = (1, 1, 0). The light mirrors off toward R = (-1, 1, 0).
const Vec3 kFromTheSide{1.0, 1.0, 0.0};
const Vec3 kMirror{-1.0, 1.0, 0.0};

}  // namespace

TEST_CASE("kMatte is the book's -1", "[lighting]") { CHECK(kMatte == -1.0); }

TEST_CASE("DiffuseReflection: a light straight above the surface arrives at full intensity",
          "[lighting]") {
    CHECK(DiffuseReflection(0.8, kUp, kUp) == 0.8);
    CHECK(DiffuseReflection(1.0, Vec3{0.0, 0.0, -1.0}, Vec3{0.0, 0.0, -1.0}) == 1.0);
}

TEST_CASE("DiffuseReflection: the intensity follows the cosine of the angle", "[lighting]") {
    const Vec3 tilted{0.0, 0.5, std::sqrt(3.0) / 2};  // 60 degrees away from the normal
    CHECK_THAT(DiffuseReflection(1.0, kUp, tilted), WithinAbs(0.5, 1e-12));
    CHECK_THAT(DiffuseReflection(0.6, kUp, kFromTheSide), WithinAbs(0.6 / std::sqrt(2.0), 1e-12));
}

TEST_CASE("DiffuseReflection: a light behind the surface, or grazing it, adds nothing",
          "[lighting]") {
    CHECK(DiffuseReflection(0.8, kUp, -kUp) == 0.0);
    CHECK(DiffuseReflection(0.8, kUp, Vec3{1.0, -0.001, 0.0}) == 0.0);
    CHECK(DiffuseReflection(0.8, kUp, Vec3{1.0, 0.0, 0.0}) == 0.0);  // <N, L> = 0
}

TEST_CASE("DiffuseReflection: neither vector has to be normalized", "[lighting]") {
    CHECK_THAT(DiffuseReflection(0.7, kUp, 10.0 * kUp), WithinAbs(0.7, 1e-12));
    CHECK_THAT(DiffuseReflection(0.7, 4.0 * kUp, 0.001 * kUp), WithinAbs(0.7, 1e-12));
    CHECK_THAT(DiffuseReflection(1.0, 3.0 * kUp, 5.0 * kFromTheSide),
               WithinAbs(1.0 / std::sqrt(2.0), 1e-12));
}

TEST_CASE("SpecularReflection: the full intensity is seen in the mirror direction", "[lighting]") {
    // cos 0 = 1 to any power.
    CHECK_THAT(SpecularReflection(1.0, kUp, kFromTheSide, kMirror, 10.0), WithinAbs(1.0, 1e-12));
    CHECK_THAT(SpecularReflection(0.5, kUp, kFromTheSide, kMirror, 500.0), WithinAbs(0.5, 1e-12));
    // A light straight above mirrors straight back up.
    CHECK_THAT(SpecularReflection(0.8, kUp, kUp, kUp, 50.0), WithinAbs(0.8, 1e-12));
}

TEST_CASE("SpecularReflection: the highlight fades as cos^s away from the mirror direction",
          "[lighting]") {
    // Looking straight down the normal is 45 degrees off the mirror direction.
    CHECK_THAT(SpecularReflection(1.0, kUp, kFromTheSide, kUp, 2.0),
               WithinAbs(0.5, 1e-12));  // (1 / sqrt(2))^2
    CHECK_THAT(SpecularReflection(1.0, kUp, kFromTheSide, kUp, 10.0),
               WithinAbs(1.0 / 32, 1e-12));  // (1 / sqrt(2))^10
    // A larger exponent gives a smaller highlight: less of it is left 45 degrees away.
    CHECK(SpecularReflection(1.0, kUp, kFromTheSide, kUp, 50.0) <
          SpecularReflection(1.0, kUp, kFromTheSide, kUp, 10.0));
}

TEST_CASE(
    "SpecularReflection: nothing is seen from more than 90 degrees off the mirror "
    "direction",
    "[lighting]") {
    // From the light's own side <R, V> < 0.
    CHECK(SpecularReflection(1.0, kUp, kFromTheSide, Vec3{1.0, 0.5, 0.0}, 10.0) == 0.0);
    CHECK(SpecularReflection(1.0, kUp, kFromTheSide, -kMirror, 10.0) == 0.0);
    CHECK(SpecularReflection(1.0, kUp, kFromTheSide, kFromTheSide, 10.0) == 0.0);  // <R, V> = 0
}

TEST_CASE("SpecularReflection: the lengths of L and V do not matter", "[lighting]") {
    CHECK_THAT(SpecularReflection(1.0, kUp, 10.0 * kFromTheSide, 0.25 * kMirror, 10.0),
               WithinAbs(1.0, 1e-12));
    CHECK_THAT(SpecularReflection(1.0, kUp, 10.0 * kFromTheSide, 3.0 * kUp, 2.0),
               WithinAbs(0.5, 1e-12));
}

TEST_CASE("SpecularReflection: the term is not gated on the light being in front of the surface",
          "[lighting]") {
    // The book's equation: only the diffuse term asks for <N, L> > 0. A light below the
    // surface mirrors to below it, where a viewer could still be asked about.
    const Vec3 from_below{1.0, -1.0, 0.0};
    const Vec3 mirror_below{-1.0, -1.0, 0.0};
    CHECK(DiffuseReflection(1.0, kUp, from_below) == 0.0);
    CHECK_THAT(SpecularReflection(1.0, kUp, from_below, mirror_below, 10.0), WithinAbs(1.0, 1e-12));
}

TEST_CASE("ComputeIllumination: ambient light adds its intensity everywhere", "[lighting]") {
    const std::vector<Light> lights = {AmbientLight{.intensity = 0.3}};
    CHECK(ComputeIllumination(Vec3{}, kUp, kUp, kMatte, lights) == 0.3);
    CHECK(ComputeIllumination(Vec3{7.0, -2.0, 3.0}, -kUp, kFromTheSide, kMatte, lights) == 0.3);
    CHECK(ComputeIllumination(Vec3{}, kUp, kUp, 500.0, lights) == 0.3);  // shininess: no effect
}

TEST_CASE("ComputeIllumination: without lights it is dark", "[lighting]") {
    CHECK(ComputeIllumination(Vec3{}, kUp, kUp, kMatte, {}) == 0.0);
    CHECK(ComputeIllumination(Vec3{}, kUp, kUp, 10.0, {}) == 0.0);
}

TEST_CASE("ComputeIllumination: the direction to a point light is from the point to the light",
          "[lighting]") {
    const std::vector<Light> lights = {PointLight{.intensity = 0.8, .position = {0.0, 5.0, 0.0}}};
    // Straight above the origin: full intensity there.
    CHECK(ComputeIllumination(Vec3{}, kUp, kUp, kMatte, lights) == 0.8);
    // From a point 5 to the side the light is 45 degrees up.
    CHECK_THAT(ComputeIllumination(Vec3{5.0, 0.0, 0.0}, kUp, kUp, kMatte, lights),
               WithinAbs(0.8 / std::sqrt(2.0), 1e-12));
    // A point above the light has it behind its surface.
    CHECK(ComputeIllumination(Vec3{0.0, 6.0, 0.0}, kUp, kUp, kMatte, lights) == 0.0);
}

TEST_CASE("ComputeIllumination: a directional light arrives from the same direction everywhere",
          "[lighting]") {
    const std::vector<Light> lights = {
        DirectionalLight{.intensity = 0.5, .direction = {0.0, 10.0, 0.0}}};
    CHECK_THAT(ComputeIllumination(Vec3{}, kUp, kUp, kMatte, lights), WithinAbs(0.5, 1e-12));
    CHECK_THAT(ComputeIllumination(Vec3{100.0, 200.0, -300.0}, kUp, kUp, kMatte, lights),
               WithinAbs(0.5, 1e-12));
    CHECK(ComputeIllumination(Vec3{}, -kUp, kUp, kMatte, lights) == 0.0);  // facing away
}

TEST_CASE("ComputeIllumination: the lights add up, and past 1", "[lighting]") {
    const std::vector<Light> lights = {
        AmbientLight{.intensity = 0.2},
        PointLight{.intensity = 0.6, .position = {0.0, 2.0, 0.0}},
        DirectionalLight{.intensity = 0.5, .direction = {0.0, 1.0, 0.0}},
    };
    // Nothing is clamped here: the canvas does that to the color.
    CHECK_THAT(ComputeIllumination(Vec3{}, kUp, kUp, kMatte, lights), WithinAbs(1.3, 1e-12));
}

TEST_CASE(
    "ComputeIllumination: a shiny surface adds the highlight of every light, a matte one "
    "none",
    "[lighting]") {
    const std::vector<Light> lights = {
        DirectionalLight{.intensity = 1.0, .direction = kFromTheSide}};
    const double diffuse = 1.0 / std::sqrt(2.0);
    CHECK_THAT(ComputeIllumination(Vec3{}, kUp, kMirror, kMatte, lights),
               WithinAbs(diffuse, 1e-12));
    CHECK_THAT(ComputeIllumination(Vec3{}, kUp, kMirror, 10.0, lights),
               WithinAbs(diffuse + 1.0, 1e-12));
    // 45 degrees off the mirror direction: (1 / sqrt(2))^10.
    CHECK_THAT(ComputeIllumination(Vec3{}, kUp, kUp, 10.0, lights),
               WithinAbs(diffuse + 1.0 / 32, 1e-12));

    // A point light at (1, 1, 0) is the same L from the origin.
    const std::vector<Light> lamp = {PointLight{.intensity = 0.5, .position = {1.0, 1.0, 0.0}}};
    CHECK_THAT(ComputeIllumination(Vec3{}, kUp, kMirror, 10.0, lamp),
               WithinAbs(0.5 * (diffuse + 1.0), 1e-12));
}

TEST_CASE("ComputeIllumination: nothing is in the way of a light", "[lighting]") {
    // The sum has no scene to ask: a light on the far side of anything still arrives.
    // The ray tracer, which has one, sums the terms itself (RayTracer::ComputeLighting).
    const std::vector<Light> lights = {PointLight{.intensity = 1.0, .position = {0.0, 1e6, 0.0}}};
    CHECK_THAT(ComputeIllumination(Vec3{}, kUp, kUp, kMatte, lights), WithinAbs(1.0, 1e-12));
}

TEST_CASE("ComputeIllumination: the light at a point does not depend on the coordinates",
          "[lighting]") {
    // A point, its normal, the direction to the viewer and three lights, in the world's
    // coordinates and then in those of a camera that stands elsewhere and looks another
    // way: points and positions transformed as points, the normal and the directions as
    // directions.
    const Vec3 point{1.0, 2.0, 3.0};
    const Vec3 normal = cgfs::Normalized({1.0, 2.0, -2.0});
    const Vec3 viewer{-4.0, 6.0, -5.0};
    const std::vector<Light> lights = {
        AmbientLight{.intensity = 0.1},
        PointLight{.intensity = 0.6, .position = {3.0, 7.0, -2.0}},
        DirectionalLight{.intensity = 0.3, .direction = {-1.0, 3.0, -2.0}},
    };
    const double in_the_world = ComputeIllumination(point, normal, viewer - point, 20.0, lights);
    CHECK(in_the_world > 0.3);  // the lights do reach the point

    const cgfs::Mat4 to_camera =
        cgfs::MakeOYRotationMatrix(-70.0) * cgfs::MakeTranslationMatrix({5.0, -1.0, 2.0});
    std::vector<Light> moved_lights;
    for (const Light& light : lights) {
        moved_lights.push_back(cgfs::TransformLight(light, to_camera));
    }
    const Vec3 moved_point = to_camera * point;
    const double in_the_camera =
        ComputeIllumination(moved_point, cgfs::TransformDirection(to_camera, normal),
                            to_camera * viewer - moved_point, 20.0, moved_lights);
    CHECK_THAT(in_the_camera, WithinAbs(in_the_world, 1e-12));
}
