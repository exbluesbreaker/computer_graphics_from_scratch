#pragma once

#include <cgfs/scene.h>

namespace cgfs::demo {

/// Three spheres lit by full ambient light only, which gives flat colors.
[[nodiscard]] Scene FlatScene();

/// The same spheres on a huge yellow "floor" sphere, lit by ambient, point and
/// directional lights. Every surface is matte.
[[nodiscard]] Scene DiffuseScene();

/// The diffuse scene with a shininess on every sphere, so the lights show up as
/// specular highlights. The project's reference image.
[[nodiscard]] Scene SpecularScene();

}  // namespace cgfs::demo
