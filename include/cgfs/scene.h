#pragma once

#include <cgfs/color.h>
#include <cgfs/vec3.h>

#include <variant>
#include <vector>

namespace cgfs {

/// The `specular` value of a surface with no highlight at all. The book's convention:
/// -1 means "skip the specular term", any other value is the exponent.
inline constexpr double kMatte = -1.0;

struct Sphere {
    Vec3 center;
    double radius = 1.0;
    Color color;
    /// Shininess: the exponent s of the specular term (see RayTracer::ComputeLighting).
    /// A higher value gives a smaller, sharper highlight: 10 is somewhat shiny, 500 is
    /// shiny, 1000 very shiny. kMatte disables the highlight.
    double specular = kMatte;
    /// How much of what the surface sees in the mirror direction shows in its color: 0 is
    /// the local shading alone, 1 a perfect mirror (see RayTracer::TraceRay).
    double reflective = 0.0;
};

/// Light that comes from everywhere equally; brightens every point the same.
struct AmbientLight {
    double intensity = 0.0;
};

/// Light radiating from a single position in space.
struct PointLight {
    double intensity = 0.0;
    Vec3 position;
};

/// Light from infinitely far away; all rays share one direction (pointing toward the light).
struct DirectionalLight {
    double intensity = 0.0;
    Vec3 direction;
};

/// The closed set of light types. Visit it with std::visit.
using Light = std::variant<AmbientLight, PointLight, DirectionalLight>;

/// Everything the ray tracer needs to know about the world. Plain data.
struct Scene {
    std::vector<Sphere> spheres;
    std::vector<Light> lights;
    Color background_color = colors::kWhite;
};

}  // namespace cgfs
