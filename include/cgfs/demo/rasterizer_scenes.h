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

/// The rasterizer's first scene of instances, drawn with RenderScene: the cube and the
/// pyramid of the objects picture are stored once each, as models, and sixteen instances
/// place them by position alone. A building is a stack of cubes with a pyramid for a
/// roof, standing on the ground 1.5 below the camera: a tower of three floors at x = 0,
/// z = 20, and a house of one floor on each side of the street, at x = -3.5 and 3.5 and
/// z = 14, 10 and 6, listed far to near so that where buildings overlap the near edges
/// show. The houses shrink toward the tower, which is perspective, and the roofs' white
/// base edges outline every eave, since an edge shared by two triangles shows the color
/// of the one drawn later. Wireframes with nothing hidden, like the objects picture.
/// Clears the canvas to a dark background first; depends on the viewport, so
/// render_demo's --distance zooms it.
void DrawInstancesScene(Canvas& canvas, const Viewport& viewport);

/// The chapter's closing picture, two cubes seen through a camera that has moved and
/// turned, drawn with RenderScene: the cube model stored once and two instances with a
/// model transform each, the first scaled to 0.75 at (-1.5, 0, 7), the second turned 195
/// degrees about Y at (1.25, 2.5, 7.5), the instances and the camera of the book's demo.
/// The camera stands at (-3, 1, 2), to the left of the cubes and a little above the
/// first, and is turned -30 degrees about Y, that is 30 degrees to its right, toward
/// them. Every vertex goes through one matrix, the camera matrix times the instance's
/// model matrix, and then through ProjectVertex as before; the projection is not part of
/// the matrix. Wireframes with nothing hidden; clears the canvas to a dark background
/// first; depends on the viewport, so render_demo's --distance zooms it.
void DrawCameraScene(Canvas& canvas, const Viewport& viewport);

/// The coordinate axes through the origin, a dot at the origin and a colored dot in
/// each corner (red top-left, green top-right, blue bottom-left, black bottom-right): a
/// quick visual check of the canvas's coordinate system. Drawn with PutPixel alone, on
/// whatever background the canvas already has.
void DrawAxesScene(Canvas& canvas);

}  // namespace cgfs::demo
