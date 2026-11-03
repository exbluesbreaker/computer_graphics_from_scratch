#pragma once

#include <cgfs/scene.h>

namespace cgfs::demo {

/// Three spheres lit by full ambient light only, which gives flat colors.
[[nodiscard]] Scene FlatScene();

/// The same spheres on a huge yellow "floor" sphere, lit by ambient, point and
/// directional lights. Every surface is matte.
[[nodiscard]] Scene DiffuseScene();

/// The diffuse scene with a shininess on every sphere, so the lights show up as
/// specular highlights.
[[nodiscard]] Scene SpecularScene();

/// The specular scene with a reflectivity on every sphere, so they mirror each other and
/// the floor. The book's final ray-traced scene and the project's reference image.
[[nodiscard]] Scene ReflectiveScene();

/// A chrome ball on a dark polished floor under a night sky, ringed by six colored
/// spheres of different finishes, with three marbles close to the camera. Two point
/// lights throw crossing shadows and highlights. Nothing new, only more of it.
[[nodiscard]] Scene MirrorBallScene();

}  // namespace cgfs::demo
