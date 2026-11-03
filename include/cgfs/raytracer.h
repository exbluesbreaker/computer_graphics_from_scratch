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

/// The closest sphere a ray meets, as found by RayTracer::ClosestIntersection. `sphere`
/// points into the scene; it is null when the ray hits nothing, and then t is +inf.
struct Hit {
    const Sphere* sphere = nullptr;
    double t = kInfinity;
};

/// Where a ray cast from a point on a surface starts, in units of its direction: the
/// shadow rays toward the lights and the reflected rays. The point lies on a sphere, so
/// that sphere solves the intersection equation at t = 0; starting a little past it keeps
/// a surface from shadowing or reflecting itself.
inline constexpr double kSurfaceEpsilon = 0.001;

/// How many times a camera ray may bounce off reflective surfaces: the recursion depth
/// Render passes to TraceRay.
inline constexpr int kReflectionDepth = 3;

/// A minimal ray tracer: one ray per canvas pixel, spheres only, diffuse and specular
/// lighting with shadows and reflections.
///
/// The camera sits at the origin and looks through the viewport. For each pixel we cast
/// a ray from the camera through the matching viewport point, find the closest sphere it
/// hits, shade that point with the scene's lights that can see it, and mix in what a
/// reflective surface shows of the rest of the scene.
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

    /// The first sphere along origin + t * direction with t in [t_min, t_max], and the t
    /// where the ray meets it. Both roots of every sphere are candidates, so a ray that
    /// starts inside a sphere hits its far side. Shared by TraceRay (the rays from the
    /// camera) and ComputeLighting (the shadow rays toward the lights).
    [[nodiscard]] Hit ClosestIntersection(Vec3 origin, Vec3 direction, double t_min,
                                          double t_max) const;

    /// The color seen along the ray origin + t * direction for t in [t_min, t_max]:
    /// the shaded closest sphere, or the scene's background color.
    ///
    /// For a sphere with reflective r > 0 the shading is the local color only when
    /// `recursion_depth` is 0; otherwise the ray bounces off the hit point P in the mirror
    /// direction R = Reflect(-D, N), what it sees there is traced with one level less of
    /// depth, starting kSurfaceEpsilon along R so the sphere does not reflect itself, and
    /// the result is local_color * (1 - r) + reflected_color * r.
    [[nodiscard]] Color TraceRay(Vec3 origin, Vec3 direction, double t_min, double t_max,
                                 int recursion_depth = kReflectionDepth) const;

    /// Total light intensity leaving `point` (unit `normal`) toward the viewer, where
    /// `view` points from the point back to the viewer and `specular` is the surface's
    /// shininess exponent s (kMatte: no highlight). For each point or directional light
    /// with direction L and intensity I that is not blocked:
    ///   i += I * <N, L> / (|N| |L|)          diffuse, when <N, L> > 0
    ///   i += I * (<R, V> / (|R| |V|))^s      specular, when s != kMatte and <R, V> > 0,
    ///                                        with R = 2 N <N, L> - L the mirror of L
    /// plus the intensity of every ambient light.
    ///
    /// A light is blocked, and the point is in its shadow, when a sphere lies on the
    /// segment from the point toward it: ClosestIntersection(P, L, kShadowEpsilon, t_max)
    /// finds one, with t_max = 1 for a point light (L = position - P reaches exactly to
    /// the light) and +inf for a directional light. Ambient light is never blocked.
    [[nodiscard]] double ComputeLighting(Vec3 point, Vec3 normal, Vec3 view,
                                         double specular) const;

private:
    const Scene& scene_;
    const Viewport& viewport_;
};

}  // namespace cgfs
