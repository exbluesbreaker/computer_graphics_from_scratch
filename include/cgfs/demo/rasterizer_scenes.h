#pragma once

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

/// The coordinate axes through the origin, a dot at the origin and a colored dot in
/// each corner (red top-left, green top-right, blue bottom-left, black bottom-right): a
/// quick visual check of the canvas's coordinate system. Drawn with PutPixel alone, on
/// whatever background the canvas already has.
void DrawAxesScene(Canvas& canvas);

}  // namespace cgfs::demo
