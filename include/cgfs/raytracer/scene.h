#pragma once

#include <cgfs/image/color.h>
#include <cgfs/lighting/illumination.h>
#include <cgfs/lighting/light.h>
#include <cgfs/math/vec3.h>

#include <vector>

namespace cgfs {

struct Sphere {
    Vec3 center;
    double radius = 1.0;
    Color color;
    /// Shininess: the exponent s of the specular term (see SpecularReflection). A higher
    /// value gives a smaller, sharper highlight: 10 is somewhat shiny, 500 is shiny, 1000
    /// very shiny. kMatte disables the highlight.
    double specular = kMatte;
    /// How much of what the surface sees in the mirror direction shows in its color: 0 is
    /// the local shading alone, 1 a perfect mirror (see RayTracer::TraceRay).
    double reflective = 0.0;
};

/// Everything the ray tracer needs to know about the world. Plain data. The lights are
/// the ones of the lighting layer (see light.h), which the rasterizer shares.
struct Scene {
    std::vector<Sphere> spheres;
    std::vector<Light> lights;
    Color background_color = colors::kWhite;
};

}  // namespace cgfs
