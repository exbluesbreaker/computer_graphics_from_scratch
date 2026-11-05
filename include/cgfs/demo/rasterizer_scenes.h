#pragma once

#include <cgfs/camera/viewport.h>
#include <cgfs/image/canvas.h>

namespace cgfs::demo {

// The rasterized demo pictures. Each draws straight into the canvas it is given, so the
// app and the golden tests draw exactly the same thing.

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

/// The rasterizer's third picture, made of shaded triangles: on the left a green
/// triangle in the proportions of the book's demo, with the intensities 0.3, 0.1 and 1
/// at its vertices, scaled to the canvas height; on the right a glowing color wheel of
/// 24 wedges from a bright center to the dim rim of a rectangle, which share their edges
/// and shade continuously across them. Clears the canvas to a dark background first;
/// integer geometry only, like DrawLinesScene.
void DrawShadedTrianglesScene(Canvas& canvas);

/// The rasterizer's first 3D picture, the book's: the twelve edges of a cube with
/// corners at x = -2 and -1, y = -0.5 and 0.5, z = 5 and 6, each vertex projected onto
/// the canvas with ProjectVertex through `viewport` and each edge drawn with DrawLine;
/// the front face blue, the back face red and the four edges from front to back green.
/// The back face comes out smaller than the front one, which is perspective. Clears the
/// canvas to a dark background first. The one rasterized picture that depends on the
/// viewport, so render_demo's --distance zooms it.
void DrawCubeScene(Canvas& canvas, const Viewport& viewport);

/// The rasterizer's first picture of objects made of triangles, each a vertex list and a
/// triangle list drawn with RenderObject: on the left the book's cube, eight corners at
/// x, y, z = ±1 moved to x -= 1.5, z += 7 and twelve triangles in six colors, two per
/// face (red at the back, green on the right, blue in front, yellow on the left, purple
/// on top, cyan underneath); on the right a square pyramid of the same height moved the
/// other way (x += 1.5), five vertices and six triangles: a white base of two triangles
/// and four sides in four more colors. Every vertex is projected once with ProjectVertex
/// through `viewport` and every triangle outlined with DrawWireframeTriangle, so the
/// faces show their diagonals and nothing is hidden; an edge shared by two triangles
/// shows the color of the one drawn later. Clears the canvas to a dark background first;
/// depends on the viewport like the cube, so render_demo's --distance zooms it.
void DrawObjectsScene(Canvas& canvas, const Viewport& viewport);

/// The coordinate axes through the origin, a dot at the origin and a colored dot in
/// each corner (red top-left, green top-right, blue bottom-left, black bottom-right): a
/// quick visual check of the canvas's coordinate system. Drawn with PutPixel alone, on
/// whatever background the canvas already has.
void DrawAxesScene(Canvas& canvas);

}  // namespace cgfs::demo
