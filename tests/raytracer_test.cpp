#include <cgfs/raytracer.h>

#include <cgfs/canvas.h>
#include <cgfs/scene.h>
#include <cgfs/viewport.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <stdexcept>

#include "test_helpers.h"

using Catch::Matchers::WithinAbs;
using cgfs::AmbientLight;
using cgfs::Canvas;
using cgfs::Color;
using cgfs::DirectionalLight;
using cgfs::Hit;
using cgfs::IntersectRaySphere;
using cgfs::kInfinity;
using cgfs::kMatte;
using cgfs::kReflectionDepth;
using cgfs::kSurfaceEpsilon;
using cgfs::PointLight;
using cgfs::RayTracer;
using cgfs::Rgb8;
using cgfs::Scene;
using cgfs::Sphere;
using cgfs::Vec3;
using cgfs::Viewport;
using namespace cgfs::colors;

namespace {

const Vec3 kOrigin{};
const Vec3 kForward{0.0, 0.0, 1.0};
const Vec3 kUp{0.0, 1.0, 0.0};
const Sphere kUnitSphereAhead{.center = {0.0, 0.0, 5.0}, .radius = 1.0, .color = kRed};

}  // namespace

TEST_CASE("IntersectRaySphere: a miss returns two infinite roots", "[raytracer]") {
    const auto [t1, t2] = IntersectRaySphere(kOrigin, Vec3{0.0, 1.0, 0.0}, kUnitSphereAhead);
    CHECK(t1 == kInfinity);
    CHECK(t2 == kInfinity);
}

TEST_CASE("IntersectRaySphere: a hit returns the far root first", "[raytracer]") {
    // |t*(0,0,1) - (0,0,5)| = 1  ->  t = 4 (near) and t = 6 (far)
    const auto [t1, t2] = IntersectRaySphere(kOrigin, kForward, kUnitSphereAhead);
    CHECK(t1 == 6.0);
    CHECK(t2 == 4.0);
}

TEST_CASE("IntersectRaySphere: t is measured in units of the direction", "[raytracer]") {
    const auto [t1, t2] = IntersectRaySphere(kOrigin, Vec3{0.0, 0.0, 2.0}, kUnitSphereAhead);
    CHECK(t1 == 3.0);
    CHECK(t2 == 2.0);
}

TEST_CASE("IntersectRaySphere: a tangent ray returns two equal roots", "[raytracer]") {
    const Sphere sphere{.center = {0.0, 1.0, 5.0}, .radius = 1.0, .color = kRed};
    const auto [t1, t2] = IntersectRaySphere(kOrigin, kForward, sphere);
    CHECK(t1 == 5.0);
    CHECK(t2 == 5.0);
}

TEST_CASE("IntersectRaySphere: from inside the sphere one root is behind the ray", "[raytracer]") {
    const Sphere sphere{.center = {0.0, 0.0, 0.0}, .radius = 1.0, .color = kRed};
    const auto [t1, t2] = IntersectRaySphere(kOrigin, kForward, sphere);
    CHECK(t1 == 1.0);
    CHECK(t2 == -1.0);
}

TEST_CASE("ComputeLighting: ambient light adds its intensity everywhere", "[raytracer]") {
    const Scene scene{.lights = {AmbientLight{.intensity = 0.3}}};
    const Viewport viewport(4, 4);
    const RayTracer tracer(scene, viewport);
    CHECK(tracer.ComputeLighting(Vec3{}, kUp, kUp, kMatte) == 0.3);
    CHECK(tracer.ComputeLighting(Vec3{7.0, -2.0, 3.0}, -kForward, -kForward, kMatte) == 0.3);
    CHECK(tracer.ComputeLighting(Vec3{}, kUp, kUp, 500.0) == 0.3);  // shininess: no effect
}

TEST_CASE("ComputeLighting: a point light straight above lights at full intensity",
          "[raytracer]") {
    const Scene scene{.lights = {PointLight{.intensity = 0.8, .position = {0.0, 5.0, 0.0}}}};
    const Viewport viewport(4, 4);
    const RayTracer tracer(scene, viewport);
    CHECK(tracer.ComputeLighting(Vec3{}, kUp, kUp, kMatte) == 0.8);
}

TEST_CASE("ComputeLighting: a light behind the surface contributes nothing", "[raytracer]") {
    const Scene scene{.lights = {PointLight{.intensity = 0.8, .position = {0.0, 5.0, 0.0}},
                                 DirectionalLight{.intensity = 0.5, .direction = {0.0, 1.0, 0.0}}}};
    const Viewport viewport(4, 4);
    const RayTracer tracer(scene, viewport);
    CHECK(tracer.ComputeLighting(Vec3{}, -kUp, -kUp, kMatte) == 0.0);
    const Vec3 sideways{1.0, 0.0, 0.0};
    CHECK(tracer.ComputeLighting(Vec3{}, sideways, sideways, kMatte) == 0.0);  // <N, L> = 0
}

TEST_CASE("ComputeLighting: intensity follows the cosine of the angle", "[raytracer]") {
    const Scene scene{.lights = {DirectionalLight{.intensity = 1.0, .direction = {0.0, 1.0, 0.0}}}};
    const Viewport viewport(4, 4);
    const RayTracer tracer(scene, viewport);
    const Vec3 tilted{0.0, 0.5, std::sqrt(3.0) / 2};  // 60 degrees away from the light
    CHECK_THAT(tracer.ComputeLighting(Vec3{}, tilted, tilted, kMatte), WithinAbs(0.5, 1e-12));
}

TEST_CASE("ComputeLighting: does not depend on the length of the light direction",
          "[raytracer]") {
    const Scene scene{.lights = {DirectionalLight{.intensity = 0.7, .direction = {0.0, 10.0, 0.0}},
                                 PointLight{.intensity = 0.3, .position = {0.0, 0.001, 0.0}}}};
    const Viewport viewport(4, 4);
    const RayTracer tracer(scene, viewport);
    CHECK_THAT(tracer.ComputeLighting(Vec3{}, kUp, kUp, kMatte), WithinAbs(1.0, 1e-12));
}

// The specular tests share one setup: a surface facing up, lit by a directional light
// from 45 degrees, L = (1, 1, 0). The light mirrors off toward R = (-1, 1, 0), and the
// diffuse term alone is cos 45 = 1 / sqrt(2).
namespace {

const Scene kSpecularScene{
    .lights = {DirectionalLight{.intensity = 1.0, .direction = {1.0, 1.0, 0.0}}}};
const Vec3 kMirror{-1.0, 1.0, 0.0};
const double kDiffuseOnly = 1.0 / std::sqrt(2.0);

}  // namespace

TEST_CASE("ComputeLighting: a matte surface has no highlight from any direction",
          "[raytracer]") {
    const Viewport viewport(4, 4);
    const RayTracer tracer(kSpecularScene, viewport);
    CHECK_THAT(tracer.ComputeLighting(Vec3{}, kUp, kMirror, kMatte),
               WithinAbs(kDiffuseOnly, 1e-12));
    CHECK_THAT(tracer.ComputeLighting(Vec3{}, kUp, kUp, kMatte), WithinAbs(kDiffuseOnly, 1e-12));
}

TEST_CASE("ComputeLighting: the full highlight appears in the mirror direction",
          "[raytracer]") {
    const Viewport viewport(4, 4);
    const RayTracer tracer(kSpecularScene, viewport);
    // cos 0 = 1 to any power: the whole light intensity is added on top of the diffuse term.
    CHECK_THAT(tracer.ComputeLighting(Vec3{}, kUp, kMirror, 10.0),
               WithinAbs(kDiffuseOnly + 1.0, 1e-12));
    CHECK_THAT(tracer.ComputeLighting(Vec3{}, kUp, kMirror, 500.0),
               WithinAbs(kDiffuseOnly + 1.0, 1e-12));
}

TEST_CASE("ComputeLighting: the highlight fades as cos^s away from the mirror direction",
          "[raytracer]") {
    const Viewport viewport(4, 4);
    const RayTracer tracer(kSpecularScene, viewport);
    // Looking straight down the normal is 45 degrees off the mirror direction.
    CHECK_THAT(tracer.ComputeLighting(Vec3{}, kUp, kUp, 2.0),
               WithinAbs(kDiffuseOnly + 0.5, 1e-12));  // (1 / sqrt(2))^2
    CHECK_THAT(tracer.ComputeLighting(Vec3{}, kUp, kUp, 10.0),
               WithinAbs(kDiffuseOnly + 1.0 / 32, 1e-12));  // (1 / sqrt(2))^10
}

TEST_CASE("ComputeLighting: no highlight when the viewer is not on the reflected side",
          "[raytracer]") {
    const Viewport viewport(4, 4);
    const RayTracer tracer(kSpecularScene, viewport);
    // From the light's own side <R, V> < 0: only the diffuse term remains.
    CHECK_THAT(tracer.ComputeLighting(Vec3{}, kUp, Vec3{1.0, 0.5, 0.0}, 10.0),
               WithinAbs(kDiffuseOnly, 1e-12));
    CHECK_THAT(tracer.ComputeLighting(Vec3{}, kUp, -kMirror, 10.0),
               WithinAbs(kDiffuseOnly, 1e-12));
}

TEST_CASE("ComputeLighting: the highlight does not depend on the lengths of L and V",
          "[raytracer]") {
    const Scene scene{
        .lights = {DirectionalLight{.intensity = 1.0, .direction = {10.0, 10.0, 0.0}}}};
    const Viewport viewport(4, 4);
    const RayTracer tracer(scene, viewport);
    CHECK_THAT(tracer.ComputeLighting(Vec3{}, kUp, 0.25 * kMirror, 10.0),
               WithinAbs(kDiffuseOnly + 1.0, 1e-12));
    CHECK_THAT(tracer.ComputeLighting(Vec3{}, kUp, 3.0 * kUp, 2.0),
               WithinAbs(kDiffuseOnly + 0.5, 1e-12));
}

TEST_CASE("ComputeLighting: a point light makes a highlight too", "[raytracer]") {
    // The light sits at (1, 1, 0), so from the origin L = (1, 1, 0) as in the other tests.
    const Scene scene{.lights = {PointLight{.intensity = 0.5, .position = {1.0, 1.0, 0.0}}}};
    const Viewport viewport(4, 4);
    const RayTracer tracer(scene, viewport);
    CHECK_THAT(tracer.ComputeLighting(Vec3{}, kUp, kMirror, 10.0),
               WithinAbs(0.5 * (kDiffuseOnly + 1.0), 1e-12));
}

// The shadow tests share one setup: the lit point is the origin with the normal up, a
// point light of intensity 1 sits at (0, 10, 0), and a unit sphere is put in the way or not.
namespace {

const PointLight kLightAbove{.intensity = 1.0, .position = {0.0, 10.0, 0.0}};

Sphere UnitSphereAt(Vec3 center) {
    return {.center = center, .radius = 1.0, .color = kRed};
}

}  // namespace

TEST_CASE("ComputeLighting: a sphere between the point and a point light casts a shadow",
          "[raytracer]") {
    Scene scene{.lights = {kLightAbove}};
    const Viewport viewport(4, 4);
    const RayTracer tracer(scene, viewport);
    REQUIRE(tracer.ComputeLighting(kOrigin, kUp, kUp, kMatte) == 1.0);  // nothing in the way

    scene.spheres = {UnitSphereAt({0.0, 5.0, 0.0})};  // halfway up: t in [0.4, 0.6]
    CHECK(tracer.ComputeLighting(kOrigin, kUp, kUp, kMatte) == 0.0);
    CHECK(tracer.ComputeLighting(kOrigin, kUp, kUp, 500.0) == 0.0);  // no highlight either

    scene.spheres = {UnitSphereAt({0.0, 9.5, 0.0})};  // touching the light: t in [0.85, 1.05]
    CHECK(tracer.ComputeLighting(kOrigin, kUp, kUp, kMatte) == 0.0);

    scene.spheres = {UnitSphereAt({0.0, 0.5, 0.0})};  // the point is inside the blocker
    CHECK(tracer.ComputeLighting(kOrigin, kUp, kUp, kMatte) == 0.0);
}

TEST_CASE("ComputeLighting: a sphere behind the point or beyond a point light is no blocker",
          "[raytracer]") {
    Scene scene{.lights = {kLightAbove}};
    const Viewport viewport(4, 4);
    const RayTracer tracer(scene, viewport);

    scene.spheres = {UnitSphereAt({0.0, 15.0, 0.0})};  // past the light: t in [1.4, 1.6]
    CHECK(tracer.ComputeLighting(kOrigin, kUp, kUp, kMatte) == 1.0);
    scene.spheres = {UnitSphereAt({0.0, -5.0, 0.0})};  // below the surface: t < 0
    CHECK(tracer.ComputeLighting(kOrigin, kUp, kUp, kMatte) == 1.0);
    scene.spheres = {UnitSphereAt({3.0, 5.0, 0.0})};  // off to the side: a miss
    CHECK(tracer.ComputeLighting(kOrigin, kUp, kUp, kMatte) == 1.0);
}

TEST_CASE("ComputeLighting: a directional light is blocked at any distance", "[raytracer]") {
    Scene scene{.lights = {DirectionalLight{.intensity = 1.0, .direction = {0.0, 1.0, 0.0}}}};
    const Viewport viewport(4, 4);
    const RayTracer tracer(scene, viewport);
    REQUIRE(tracer.ComputeLighting(kOrigin, kUp, kUp, kMatte) == 1.0);

    scene.spheres = {UnitSphereAt({0.0, 1000.0, 0.0})};  // t = 999: no t_max for the sun
    CHECK(tracer.ComputeLighting(kOrigin, kUp, kUp, kMatte) == 0.0);
    scene.spheres = {UnitSphereAt({0.0, -1000.0, 0.0})};  // but still only ahead of the point
    CHECK(tracer.ComputeLighting(kOrigin, kUp, kUp, kMatte) == 1.0);
}

TEST_CASE("ComputeLighting: each light is shadowed on its own", "[raytracer]") {
    // A blocker straight above cuts off the light above it, not the one from the side.
    Scene scene{
        .spheres = {UnitSphereAt({0.0, 5.0, 0.0})},
        .lights = {AmbientLight{.intensity = 0.25}, kLightAbove,
                   DirectionalLight{.intensity = 0.5, .direction = {1.0, 1.0, 0.0}}},
    };
    const Viewport viewport(4, 4);
    const RayTracer tracer(scene, viewport);
    CHECK_THAT(tracer.ComputeLighting(kOrigin, kUp, kUp, kMatte),
               WithinAbs(0.25 + 0.5 / std::sqrt(2.0), 1e-12));
}

TEST_CASE("ComputeLighting: the sphere the point lies on does not shadow it", "[raytracer]") {
    // The lit point is the top of a sphere, so the shadow ray starts on that sphere's
    // surface and solves its equation at t = 0 (and once more behind the point).
    Scene scene{.spheres = {UnitSphereAt({0.0, -1.0, 0.0})}, .lights = {kLightAbove}};
    const Viewport viewport(4, 4);
    const RayTracer tracer(scene, viewport);
    CHECK(tracer.ComputeLighting(kOrigin, kUp, kUp, kMatte) == 1.0);

    // Also when the light grazes the surface from 60 degrees off the normal.
    scene.lights = {PointLight{.intensity = 1.0, .position = {5.0 * std::sqrt(3.0), 5.0, 0.0}}};
    CHECK_THAT(tracer.ComputeLighting(kOrigin, kUp, kUp, kMatte), WithinAbs(0.5, 1e-12));

    // The same sphere does shadow a point that is not on it.
    scene.lights = {kLightAbove};
    CHECK(tracer.ComputeLighting(Vec3{0.0, -3.0, 0.0}, kUp, kUp, kMatte) == 0.0);
}

TEST_CASE("ComputeLighting: a shadow ray starts kSurfaceEpsilon along L", "[raytracer]") {
    // Along L = (0, 10, 0) the epsilon is 0.01 units. A sphere that lies below the point
    // and whose top pokes just short of that is skipped; one that reaches just past it
    // counts.
    Scene scene{.lights = {kLightAbove}};
    const Viewport viewport(4, 4);
    const RayTracer tracer(scene, viewport);
    const double epsilon_y = kSurfaceEpsilon * 10.0;

    scene.spheres = {UnitSphereAt({0.0, -1.0 + epsilon_y - 1e-6, 0.0})};  // top at t < 0.001
    CHECK(tracer.ComputeLighting(kOrigin, kUp, kUp, kMatte) == 1.0);
    scene.spheres = {UnitSphereAt({0.0, -1.0 + epsilon_y + 1e-6, 0.0})};  // top at t > 0.001
    CHECK(tracer.ComputeLighting(kOrigin, kUp, kUp, kMatte) == 0.0);
}

TEST_CASE("ClosestIntersection: the closest sphere in range wins", "[raytracer]") {
    const Viewport viewport(4, 4);
    Scene scene{.spheres = {{.center = {0.0, 0.0, 10.0}, .radius = 1.0, .color = kBlue},
                            {.center = {0.0, 0.0, 5.0}, .radius = 1.0, .color = kRed}}};
    const RayTracer tracer(scene, viewport);

    Hit hit = tracer.ClosestIntersection(kOrigin, kForward, 1.0, kInfinity);
    CHECK(hit.sphere == &scene.spheres[1]);  // the red: its near side at t = 4
    CHECK(hit.t == 4.0);

    hit = tracer.ClosestIntersection(kOrigin, kForward, 4.5, kInfinity);  // t_min inside the red
    CHECK(hit.sphere == &scene.spheres[1]);  // its far side at t = 6 still counts
    CHECK(hit.t == 6.0);

    hit = tracer.ClosestIntersection(kOrigin, kForward, 7.0, kInfinity);  // t_min past the red
    CHECK(hit.sphere == &scene.spheres[0]);
    CHECK(hit.t == 9.0);

    std::swap(scene.spheres[0], scene.spheres[1]);  // order in the scene does not matter
    hit = tracer.ClosestIntersection(kOrigin, kForward, 1.0, kInfinity);
    CHECK(hit.sphere == &scene.spheres[0]);
    CHECK(hit.t == 4.0);
}

TEST_CASE("ClosestIntersection: a miss has no sphere and an infinite t", "[raytracer]") {
    const Viewport viewport(4, 4);
    Scene scene{.spheres = {kUnitSphereAhead}};  // at (0, 0, 5): t = 4 and t = 6
    const RayTracer tracer(scene, viewport);

    auto check_miss = [&](Vec3 direction, double t_min, double t_max) {
        const Hit hit = tracer.ClosestIntersection(kOrigin, direction, t_min, t_max);
        CHECK(hit.sphere == nullptr);
        CHECK(hit.t == kInfinity);
    };
    check_miss(kUp, 1.0, kInfinity);        // the ray passes by
    check_miss(-kForward, 1.0, kInfinity);  // the sphere is behind the ray
    check_miss(kForward, 1.0, 3.0);         // t_max stops short of it
    check_miss(kForward, 7.0, kInfinity);   // t_min starts past it

    scene.spheres.clear();  // nothing to hit at all
    check_miss(kForward, 1.0, kInfinity);
}

TEST_CASE("ClosestIntersection: the range bounds are inclusive", "[raytracer]") {
    const Viewport viewport(4, 4);
    const Scene scene{.spheres = {kUnitSphereAhead}};  // at (0, 0, 5): t = 4 and t = 6
    const RayTracer tracer(scene, viewport);

    CHECK(tracer.ClosestIntersection(kOrigin, kForward, 4.0, kInfinity).t == 4.0);
    CHECK(tracer.ClosestIntersection(kOrigin, kForward, 1.0, 4.0).t == 4.0);
    CHECK(tracer.ClosestIntersection(kOrigin, kForward, 6.0, kInfinity).t == 6.0);
}

TEST_CASE("TraceRay: the closest sphere in range wins", "[raytracer]") {
    const Viewport viewport(4, 4);
    Scene scene{
        .spheres = {{.center = {0.0, 0.0, 10.0}, .radius = 1.0, .color = kBlue},
                    {.center = {0.0, 0.0, 5.0}, .radius = 1.0, .color = kRed}},
        .lights = {AmbientLight{.intensity = 1.0}},
        .background_color = kBlack,
    };
    const RayTracer tracer(scene, viewport);

    CHECK(tracer.TraceRay(kOrigin, kForward, 1.0, kInfinity) == kRed);
    CHECK(tracer.TraceRay(kOrigin, Vec3{0.0, 1.0, 0.0}, 1.0, kInfinity) == kBlack);  // miss
    CHECK(tracer.TraceRay(kOrigin, kForward, 7.0, kInfinity) == kBlue);  // t_min skips the red
    CHECK(tracer.TraceRay(kOrigin, kForward, 1.0, 3.0) == kBlack);       // t_max skips both

    std::swap(scene.spheres[0], scene.spheres[1]);  // order in the scene does not matter
    CHECK(tracer.TraceRay(kOrigin, kForward, 1.0, kInfinity) == kRed);
}

TEST_CASE("TraceRay: shades the hit point with the lights", "[raytracer]") {
    const Viewport viewport(4, 4);
    Scene scene{.spheres = {kUnitSphereAhead}};
    REQUIRE(scene.spheres[0].specular == kMatte);  // the default: diffuse only
    const RayTracer tracer(scene, viewport);

    // A point light at the camera hits the front of the sphere head-on: full intensity.
    scene.lights = {PointLight{.intensity = 1.0, .position = kOrigin}};
    CHECK(tracer.TraceRay(kOrigin, kForward, 1.0, kInfinity) == kRed);

    // Half intensity halves the color.
    scene.lights = {PointLight{.intensity = 0.5, .position = kOrigin}};
    CHECK(tracer.TraceRay(kOrigin, kForward, 1.0, kInfinity) == Color{127.5, 0.0, 0.0});

    // A light from behind the sphere does not reach the front: black.
    scene.lights = {DirectionalLight{.intensity = 1.0, .direction = kForward}};
    CHECK(tracer.TraceRay(kOrigin, kForward, 1.0, kInfinity) == Color{0.0, 0.0, 0.0});
}

TEST_CASE("TraceRay: a shiny sphere adds the highlight seen from the ray's origin",
          "[raytracer]") {
    const Viewport viewport(4, 4);
    Scene scene{.spheres = {kUnitSphereAhead}};
    scene.spheres[0].specular = 500.0;
    const RayTracer tracer(scene, viewport);

    // A point light at the camera mirrors straight back into the camera: the diffuse
    // and the specular term each contribute the full intensity.
    scene.lights = {PointLight{.intensity = 0.5, .position = kOrigin}};
    CHECK(tracer.TraceRay(kOrigin, kForward, 1.0, kInfinity) == kRed);
    scene.lights = {PointLight{.intensity = 1.0, .position = kOrigin}};
    CHECK(tracer.TraceRay(kOrigin, kForward, 1.0, kInfinity) == Color{510.0, 0.0, 0.0});

    // A light from 45 degrees to the side mirrors off 45 degrees past the camera, so the
    // highlight is cos(45)^s: it depends on the sphere's own shininess.
    scene.lights = {DirectionalLight{.intensity = 1.0, .direction = {-1.0, 0.0, -1.0}}};
    scene.spheres[0].specular = 2.0;
    CHECK_THAT(tracer.TraceRay(kOrigin, kForward, 1.0, kInfinity).r,
               WithinAbs(255.0 * (1.0 / std::sqrt(2.0) + 0.5), 1e-9));
    scene.spheres[0].specular = kMatte;
    CHECK_THAT(tracer.TraceRay(kOrigin, kForward, 1.0, kInfinity).r,
               WithinAbs(255.0 / std::sqrt(2.0), 1e-9));
}

TEST_CASE("TraceRay: a sphere in the light's way leaves the hit point in shadow",
          "[raytracer]") {
    const Viewport viewport(4, 4);
    // The camera sees the front of the red sphere at (0, 0, 4). A point light behind the
    // camera at (0, 0, -6) faces that point head-on and, with the ambient light, lights
    // it fully.
    Scene scene{
        .spheres = {kUnitSphereAhead},
        .lights = {AmbientLight{.intensity = 0.25},
                   PointLight{.intensity = 0.75, .position = {0.0, 0.0, -6.0}}},
    };
    const RayTracer tracer(scene, viewport);
    REQUIRE(tracer.TraceRay(kOrigin, kForward, 1.0, kInfinity) == kRed);

    // A sphere between the point and the light (behind the camera, so not in the picture)
    // leaves only the ambient light: the shadow ray meets it at t = 0.6 and 0.8.
    scene.spheres.push_back({.center = {0.0, 0.0, -3.0}, .radius = 1.0, .color = kBlue});
    CHECK(tracer.TraceRay(kOrigin, kForward, 1.0, kInfinity) == Color{255.0 * 0.25, 0.0, 0.0});

    // The same sphere beyond the light (t = 1.2 and 1.4) changes nothing.
    scene.spheres.back().center = {0.0, 0.0, -9.0};
    CHECK(tracer.TraceRay(kOrigin, kForward, 1.0, kInfinity) == kRed);
}

// The reflection tests use ambient light only, so every local color is exact and the
// result depends on the geometry of the bounces alone.
namespace {

/// Two half-mirrors facing each other along the Z axis: the red sphere ahead of the
/// camera and a green one behind it. A ray down the axis bounces between them, picking
/// up half of the color it sees at each bounce, until its depth runs out:
///   depth 0: red
///   depth 1: red/2 + green/2
///   depth 2: red/2 + (green/2 + red/2)/2
///   depth 3: red/2 + (green/2 + (red/2 + green/2)/2)/2
Scene TwoMirrorsScene() {
    return {
        .spheres =
            {{.center = {0.0, 0.0, 5.0}, .radius = 1.0, .color = kRed, .reflective = 0.5},
             {.center = {0.0, 0.0, -5.0}, .radius = 1.0, .color = kGreen, .reflective = 0.5}},
        .lights = {AmbientLight{.intensity = 1.0}},
        .background_color = kBlack,
    };
}

}  // namespace

TEST_CASE("TraceRay: a surface with reflective 0 ignores the recursion depth", "[raytracer]") {
    const Viewport viewport(4, 4);
    const Scene scene{.spheres = {kUnitSphereAhead},
                      .lights = {AmbientLight{.intensity = 1.0}},
                      .background_color = kBlue};
    REQUIRE(scene.spheres[0].reflective == 0.0);  // the default
    const RayTracer tracer(scene, viewport);
    CHECK(tracer.TraceRay(kOrigin, kForward, 1.0, kInfinity, 0) == kRed);
    CHECK(tracer.TraceRay(kOrigin, kForward, 1.0, kInfinity, 3) == kRed);
    CHECK(tracer.TraceRay(kOrigin, kForward, 1.0, kInfinity) == kRed);
}

TEST_CASE("TraceRay: a reflective sphere mixes in what it sees in the mirror direction",
          "[raytracer]") {
    const Viewport viewport(4, 4);
    // The camera ray hits the front of the sphere head-on and bounces straight back,
    // where there is nothing but the background.
    Scene scene{.spheres = {kUnitSphereAhead},
                .lights = {AmbientLight{.intensity = 1.0}},
                .background_color = kBlue};
    const RayTracer tracer(scene, viewport);

    scene.spheres[0].reflective = 0.5;
    CHECK(tracer.TraceRay(kOrigin, kForward, 1.0, kInfinity, 1) == Color{127.5, 0.0, 127.5});
    scene.spheres[0].reflective = 0.25;
    CHECK(tracer.TraceRay(kOrigin, kForward, 1.0, kInfinity, 1) == Color{191.25, 0.0, 63.75});
    scene.spheres[0].reflective = 1.0;  // a perfect mirror shows only the reflection
    CHECK(tracer.TraceRay(kOrigin, kForward, 1.0, kInfinity, 1) == kBlue);

    // With no depth left there is no reflection: the local color alone.
    CHECK(tracer.TraceRay(kOrigin, kForward, 1.0, kInfinity, 0) == kRed);
}

TEST_CASE("TraceRay: the reflected ray obeys the mirror law", "[raytracer]") {
    const Viewport viewport(4, 4);
    // A ray from (2, 1, 0) along (-1, -1, 0) meets the unit sphere at the origin in
    // (1, 0, 0), where the normal is +X, at 45 degrees: it bounces off along (1, -1, 0).
    // Mirroring the ray's own direction instead of the view vector would send it along
    // (-1, 1, 0), into the sphere, and the reflection would show the sphere itself.
    Scene scene{
        .spheres = {{.center = kOrigin, .radius = 1.0, .color = kRed, .reflective = 0.5}},
        .lights = {AmbientLight{.intensity = 1.0}},
        .background_color = kBlack,
    };
    const RayTracer tracer(scene, viewport);
    const Vec3 start{2.0, 1.0, 0.0};
    const Vec3 toward{-1.0, -1.0, 0.0};
    REQUIRE(tracer.ClosestIntersection(start, toward, 0.5, kInfinity).t == 1.0);
    CHECK(tracer.TraceRay(start, toward, 0.5, kInfinity, 1) == Color{127.5, 0.0, 0.0});

    // A green sphere placed on the bounced ray shows up in the mirror.
    scene.spheres.push_back({.center = {3.0, -2.0, 0.0}, .radius = 1.0, .color = kGreen});
    CHECK(tracer.TraceRay(start, toward, 0.5, kInfinity, 1) == Color{127.5, 127.5, 0.0});
}

TEST_CASE("TraceRay: the recursion depth limits how many times a ray bounces", "[raytracer]") {
    const Viewport viewport(4, 4);
    const Scene scene = TwoMirrorsScene();
    const RayTracer tracer(scene, viewport);
    const auto trace = [&](int depth) {
        return tracer.TraceRay(kOrigin, kForward, 1.0, kInfinity, depth);
    };
    CHECK(trace(0) == Color{255.0, 0.0, 0.0});
    CHECK(trace(1) == Color{127.5, 127.5, 0.0});
    CHECK(trace(2) == Color{191.25, 63.75, 0.0});
    CHECK(trace(3) == Color{159.375, 95.625, 0.0});
    CHECK(trace(-1) == trace(0));  // never bounces

    // Each bounce starts on the sphere it leaves, which solves the intersection equation
    // at t = 0 there; had that root counted, the red sphere would reflect itself and
    // depth 1 would be red/2 + red/2 = red.
}

TEST_CASE("Render: camera rays bounce kReflectionDepth = 3 times", "[raytracer]") {
    static_assert(kReflectionDepth == 3, "the book's value");
    // The center pixel of the canvas looks straight down the Z axis, like the ray above.
    const Scene scene = TwoMirrorsScene();
    Canvas canvas(4, 4, kWhite);
    const Viewport viewport(canvas);
    RayTracer(scene, viewport).Render(canvas);
    CHECK(canvas.GetPixel(0, 0) == Rgb8{159, 95, 0});  // Color{159.375, 95.625, 0} truncated
}

TEST_CASE("Render: draws the scene through the viewport", "[raytracer]") {
    const Scene scene{
        .spheres = {kUnitSphereAhead},
        .lights = {AmbientLight{.intensity = 1.0}},
        .background_color = kBlue,
    };
    Canvas canvas(4, 4, kWhite);  // the canvas background must not show through
    const Viewport viewport(canvas);
    RayTracer(scene, viewport).Render(canvas);

    CHECK(canvas.GetPixel(0, 0) == Rgb8{255, 0, 0});     // straight ahead: the sphere
    CHECK(canvas.GetPixel(-2, -2) == Rgb8{0, 0, 255});   // corner: scene background
    CHECK(canvas.GetPixel(1, 1) == Rgb8{0, 0, 255});
}

TEST_CASE("Render: covers -width/2 <= x < width/2 and -height/2 <= y < height/2", "[raytracer]") {
    // For odd sizes the loops stop one short of the last column and row.
    const Scene scene{.background_color = kBlue};
    Canvas canvas(3, 3, kWhite);
    RayTracer(scene, Viewport(canvas)).Render(canvas);

    const auto data = canvas.Data();
    const Rgb8 blue{0, 0, 255};
    const Rgb8 white{255, 255, 255};
    auto pixel = [&](int row, int column) {
        const auto i = static_cast<std::size_t>(row * 3 + column) * 3;
        return Rgb8{data[i], data[i + 1], data[i + 2]};
    };
    CHECK(pixel(0, 0) == blue);
    CHECK(pixel(1, 1) == blue);
    CHECK(pixel(0, 2) == white);  // last column untouched
    CHECK(pixel(2, 0) == white);  // last row untouched
}

TEST_CASE("Render: rejects a canvas the viewport was not built for", "[raytracer]") {
    const Scene scene;
    Canvas canvas(8, 8);
    CHECK_THROWS_AS(RayTracer(scene, Viewport(4, 4)).Render(canvas), std::invalid_argument);
}
