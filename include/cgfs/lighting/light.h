#pragma once

#include <cgfs/math/mat4.h>
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

    bool operator==(const AmbientLight&) const = default;
};

/// Light radiating from a single position in space.
struct PointLight {
    double intensity = 0.0;
    Vec3 position;

    bool operator==(const PointLight&) const = default;
};

/// Light from infinitely far away; all rays share one direction (pointing toward the light).
struct DirectionalLight {
    double intensity = 0.0;
    Vec3 direction;

    bool operator==(const DirectionalLight&) const = default;
};

/// The closed set of light types. Visit it with std::visit.
using Light = std::variant<AmbientLight, PointLight, DirectionalLight>;

/// One function object made of several lambdas, the usual way to hand std::visit a case
/// per kind of light:
///
///     std::visit(Overloaded{[](const AmbientLight& l) { ... },
///                           [](const PointLight& l) { ... },
///                           [](const DirectionalLight& l) { ... }},
///                light);
///
/// A kind without a case does not compile, so a new kind of light cannot be forgotten.
template <class... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};

/// The light under a transform: the same light in other coordinates.
///
/// A point light has a position, which is transformed as a point, `transform * position`.
/// A directional light has a direction, which is transformed as a direction
/// (TransformDirection): a rotation turns it and a translation does not touch it, the
/// light being infinitely far away. Ambient light has neither and is returned as it is.
/// The intensity is the same in every case.
///
/// With a camera matrix as the transform this takes a light of the world into the
/// camera's coordinates, where the rasterizer lights its triangles (see RenderScene).
/// Precondition: `transform` is affine. A scaling in it would scale a direction, which
/// the illumination equation does not mind.
[[nodiscard]] Light TransformLight(const Light& light, const Mat4& transform);

}  // namespace cgfs
