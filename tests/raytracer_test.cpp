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
using cgfs::IntersectRaySphere;
using cgfs::kInfinity;
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
    CHECK(tracer.ComputeLighting(Vec3{}, Vec3{0.0, 1.0, 0.0}) == 0.3);
    CHECK(tracer.ComputeLighting(Vec3{7.0, -2.0, 3.0}, Vec3{0.0, 0.0, -1.0}) == 0.3);
}

TEST_CASE("ComputeLighting: a point light straight above lights at full intensity",
          "[raytracer]") {
    const Scene scene{.lights = {PointLight{.intensity = 0.8, .position = {0.0, 5.0, 0.0}}}};
    const Viewport viewport(4, 4);
    const RayTracer tracer(scene, viewport);
    CHECK(tracer.ComputeLighting(Vec3{}, Vec3{0.0, 1.0, 0.0}) == 0.8);
}

TEST_CASE("ComputeLighting: a light behind the surface contributes nothing", "[raytracer]") {
    const Scene scene{.lights = {PointLight{.intensity = 0.8, .position = {0.0, 5.0, 0.0}},
                                 DirectionalLight{.intensity = 0.5, .direction = {0.0, 1.0, 0.0}}}};
    const Viewport viewport(4, 4);
    const RayTracer tracer(scene, viewport);
    CHECK(tracer.ComputeLighting(Vec3{}, Vec3{0.0, -1.0, 0.0}) == 0.0);
    CHECK(tracer.ComputeLighting(Vec3{}, Vec3{1.0, 0.0, 0.0}) == 0.0);  // grazing: <N, L> = 0
}

TEST_CASE("ComputeLighting: intensity follows the cosine of the angle", "[raytracer]") {
    const Scene scene{.lights = {DirectionalLight{.intensity = 1.0, .direction = {0.0, 1.0, 0.0}}}};
    const Viewport viewport(4, 4);
    const RayTracer tracer(scene, viewport);
    const Vec3 tilted{0.0, 0.5, std::sqrt(3.0) / 2};  // 60 degrees away from the light
    CHECK_THAT(tracer.ComputeLighting(Vec3{}, tilted), WithinAbs(0.5, 1e-12));
}

TEST_CASE("ComputeLighting: does not depend on the length of the light direction",
          "[raytracer]") {
    const Scene scene{.lights = {DirectionalLight{.intensity = 0.7, .direction = {0.0, 10.0, 0.0}},
                                 PointLight{.intensity = 0.3, .position = {0.0, 0.001, 0.0}}}};
    const Viewport viewport(4, 4);
    const RayTracer tracer(scene, viewport);
    CHECK_THAT(tracer.ComputeLighting(Vec3{}, Vec3{0.0, 1.0, 0.0}), WithinAbs(1.0, 1e-12));
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
