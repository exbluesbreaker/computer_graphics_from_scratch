#pragma once

#include <cgfs/canvas.h>
#include <cgfs/color.h>
#include <cgfs/scene.h>
#include <cgfs/vec3.h>
#include <cgfs/viewport.h>

#include <limits>

namespace cgfs {

inline constexpr double kInfinity = std::numeric_limits<double>::infinity();

/// The two solutions of the ray-sphere equation. Both are +inf when the ray misses.
struct SphereIntersection {
    double t1 = kInfinity;
    double t2 = kInfinity;
};

/// Solve |origin + t * direction - center|^2 = radius^2 for t.
///
/// With CO = origin - center the equation is the quadratic
///   <D, D> t^2 + 2 <CO, D> t + <CO, CO> - r^2 = 0.
/// t1 takes the + root and t2 the - root; a tangent ray gives two equal roots.
[[nodiscard]] SphereIntersection IntersectRaySphere(Vec3 origin, Vec3 direction,
                                                    const Sphere& sphere);

/// A minimal ray tracer: one ray per canvas pixel, spheres only, diffuse lighting.
///
/// The camera sits at the origin and looks through the viewport. For each pixel we cast
/// a ray from the camera through the matching viewport point, find the closest sphere it
/// hits, and shade that point with the scene's lights.
class RayTracer {
public:
    /// Keeps references: the scene and viewport must outlive the tracer.
    RayTracer(const Scene& scene, const Viewport& viewport) noexcept
        : scene_(scene), viewport_(viewport) {}

    /// Trace one ray per canvas pixel and write the result into the canvas.
    ///
    /// Covers the pixel range -width/2 <= x < width/2, -height/2 <= y < height/2.
    /// Throws std::invalid_argument when the canvas dimensions differ from the ones the
    /// viewport was built for.
    void Render(Canvas& canvas) const;

    /// The color seen along the ray origin + t * direction for t in [t_min, t_max]:
    /// the shaded closest sphere, or the scene's background color.
    [[nodiscard]] Color TraceRay(Vec3 origin, Vec3 direction, double t_min, double t_max) const;

    /// Total light intensity arriving at `point` with unit `normal` (diffuse model):
    ///   i = sum(ambient I) + sum(I * <N, L> / (|N| |L|)) over lights in front of the surface.
    [[nodiscard]] double ComputeLighting(Vec3 point, Vec3 normal) const;

private:
    const Scene& scene_;
    const Viewport& viewport_;
};

}  // namespace cgfs
