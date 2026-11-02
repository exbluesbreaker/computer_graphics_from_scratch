#pragma once

#include <cgfs/color.h>
#include <cgfs/vec3.h>

#include <variant>
#include <vector>

namespace cgfs {

struct Sphere {
    Vec3 center;
    double radius = 1.0;
    Color color;
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
