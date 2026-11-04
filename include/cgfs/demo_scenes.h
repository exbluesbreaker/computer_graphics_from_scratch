#pragma once

#include <cgfs/canvas.h>
#include <cgfs/scene.h>

namespace cgfs::demo {

// The ray-traced scenes return a Scene for a RayTracer to render; the rasterized ones
// draw straight into a canvas.

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

/// The rasterizer's first picture, made of nothing but DrawLine calls: a dim pair of
/// axes, "string art" envelopes in the four corners (straight lines whose ends slide
/// along the two edges and together trace a curve) and a sunburst of colored lines from
/// the center to a rectangle around it. The lines cover every slope and both endpoint
/// orders. Clears the canvas to a dark background first; the geometry is integer
/// arithmetic only, so the picture is identical on every platform.
void DrawLinesScene(Canvas& canvas);

/// The rasterizer's second picture, made of filled triangles: a big triangle in every
/// corner, each filled and then outlined the way the book's demo does it (a flat top, a
/// flat bottom, a vertical edge and a vertex outside the canvas among them), and in the
/// middle a color wheel of 24 wedges between the origin and the rim of a rectangle,
/// which share their edges and tile without gaps. Clears the canvas to a dark background
/// first; integer geometry only, like DrawLinesScene.
void DrawTrianglesScene(Canvas& canvas);

}  // namespace cgfs::demo
