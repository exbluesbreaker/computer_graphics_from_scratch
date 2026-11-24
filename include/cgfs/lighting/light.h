#pragma once

#include <cgfs/math/vec3.h>

#include <variant>

namespace cgfs {

// The lights of a scene: the three kinds of the book's Light chapter. Plain data, in
// whatever coordinates the scene they light is in, and shared by both renderers: the ray
// tracer and the rasterizer compute the light at a point from the same lights by the same
// equation (see illumination.h).

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

}  // namespace cgfs
