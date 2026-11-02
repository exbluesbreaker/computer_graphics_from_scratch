#pragma once

#include <cgfs/scene.h>

namespace cgfs::demo {

/// Three spheres lit by full ambient light only, which gives flat colors.
[[nodiscard]] Scene FlatScene();

/// The same spheres on a huge yellow "floor" sphere, lit by ambient, point and
/// directional lights. The project's reference image.
[[nodiscard]] Scene DiffuseScene();

}  // namespace cgfs::demo
