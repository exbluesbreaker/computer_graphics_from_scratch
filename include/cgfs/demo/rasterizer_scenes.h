#pragma once

#include <cgfs/camera/viewport.h>
#include <cgfs/image/canvas.h>
#include <cgfs/rasterizer/shading.h>

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
/// of the one drawn later. Wireframes with nothing hidden, like the objects picture:
/// drawn without back-face culling. Clears the canvas to a dark background first; depends
/// on the viewport, so render_demo's --distance zooms it.
void DrawInstancesScene(Canvas& canvas, const Viewport& viewport);

/// The chapter's closing picture, two cubes seen through a camera that has moved and
/// turned, drawn with RenderScene: the cube model stored once and two instances with a
/// model transform each, the first scaled to 0.75 at (-1.5, 0, 7), the second turned 195
/// degrees about Y at (1.25, 2.5, 7.5), the instances and the camera of the book's demo.
/// The camera stands at (-3, 1, 2), to the left of the cubes and a little above the
/// first, and is turned -30 degrees about Y, that is 30 degrees to its right, toward
/// them. Every vertex goes through one matrix, the camera matrix times the instance's
/// model matrix, and then through ProjectVertex as before; the projection is not part of
/// the matrix. Wireframes with nothing hidden, drawn without back-face culling; clears
/// the canvas to a dark background first; depends on the viewport, so render_demo's
/// --distance zooms it.
void DrawCameraScene(Canvas& canvas, const Viewport& viewport);

/// The picture of the clipping chapter, and the first with shaded solids: a city of
/// boxes and pyramids on a checkerboard floor, seen by a camera that stands in it, at
/// its corner, 2.5 above the ground, turned 42 degrees to its right to look down the
/// avenue that crosses it. The scene reaches all around the camera and through each of
/// the five planes of its clipping volume, MakeClippingPlanes of `viewport`: the floor
/// runs on under and behind it, high-rises leave the picture at the top and a plinth at
/// the bottom, a lantern an arm's length away has the near corner of its cap cut off by
/// the near plane, so that the inside of the cap shows, and one high-rise stands behind
/// the camera and is discarded whole. RenderScene clips the scene in 3D between the
/// transforms and the projection; without that step this scene could not be drawn at
/// all, since ProjectVertex cannot take a vertex behind the camera.
///
/// The solids are drawn with DrawShadedTriangle from intensities stored at the corners
/// of the models' triangles: a shade per face, darker toward the ground, and a floor
/// that fades with distance. A cut triangle gets the intensity of its new corners by
/// the same interpolation as their position. The scene is painted in list order,
/// without a depth buffer and without back-face culling, so the picture is composed for
/// it: every solid stands ahead of the camera and to its right, lists the faces turned
/// away from there first, and the instances are listed from far to near. The back faces
/// are what shows inside the lantern's cap. Clears the canvas to a dark background
/// first; depends on the viewport, so render_demo's --distance zooms it, and moves the
/// near plane out with the viewport.
void DrawClippingScene(Canvas& canvas, const Viewport& viewport);

/// The picture of hidden-surface removal: fifteen instances of the cube of the objects
/// picture, six faces in six colors, drawn with RenderScene and DrawFilledTriangle
/// through a DepthBuffer, with the back faces culled. The scene is composed so that
/// painting it in list order gets it wrong, within every cube and between cubes; the two
/// halves of the chapter make the order irrelevant, back-face culling within a cube and
/// the depth buffer, which lets a pixel be set only by something nearer than what it
/// shows, everywhere.
///
/// In the middle a wall of 3 x 3 cubes faces the camera, 3.5 apart at z = 12: the same
/// cube from nine directions, each listed back, right, front, left, top, bottom. Every
/// one shows what a cube shows from outside, three faces at most: the blue front and,
/// off the axis, the side and the top or bottom turned toward the camera, the cube
/// straight ahead a plain blue square. The left, the top and the bottom are listed
/// after the front and, where they are turned away, would be painted over it as a
/// yellow, a purple and a cyan band. They are back faces and are not drawn at all: of
/// the wall's 54 faces 21 are, and they do not overlap, a cube being convex.
///
/// On each side of the wall a row of three cubes leads away from the camera and
/// outward, from (7, -2, 12) in steps of (5.5, 0, 6), mirrored on the left, and is
/// listed from the nearest cube to the farthest. Painted in that order every cube would
/// cover the one in front of it, front face over front face, which culling cannot help;
/// through the depth buffer each is hidden where the nearer one stands. That is the
/// fault between objects instead of within one, and the reason the buffer belongs to
/// the picture and not to an object.
///
/// Culling leaves 78 of the 180 triangles to draw. It also settles what the depth buffer
/// alone left open: two faces that share an edge are equally deep along it, and a face
/// turned away could win the edge's pixels from the face in front of it, a line one
/// pixel wide in the wrong color (the spec says where). A face that is not drawn wins
/// nothing.
///
/// The default camera and no rotation: plain arithmetic, no trigonometry. Clears the
/// canvas to a dark background first; depends on the viewport, so render_demo's
/// --distance zooms it.
void DrawFilledCubesScene(Canvas& canvas, const Viewport& viewport);

/// The picture of the Shading chapter, the first under lights: a floor with five solids
/// standing on it and three lamps hanging low over three tiles, lit by a little ambient
/// light and a sun besides, drawn with RenderScene through a depth buffer and shaded with
/// `shading_model`. It is one scene to be drawn four times, the four pictures of the
/// book's chapter, and every object in it is there for something that one shading model
/// gets right and the one before it does not:
///
/// - The cube has flat faces. Flat shading leaves a faint seam across each face a lamp
///   lights, its two triangles a shade apart, the one in the book's own figure; lit at
///   the corners, from faceted Gouraud shading on, the two triangles agree where they
///   meet and the cube is right.
/// - The matte ball has few triangles, 128, and the normals of a sphere at their
///   corners. It is faceted until those normals are read: round from Gouraud shading
///   on.
/// - The cylinder is round one way and flat at its ends, twelve sides and two caps.
///   Its sides are strips until Gouraud shading, which rounds them and keeps the rim a
///   sharp edge, a vertex there having the side's normal in the side's triangles and
///   none in the cap's. Its highlight, a streak down its side, is smeared over a whole
///   strip until Phong shading.
/// - The coarse shiny ball is the matte one with highlights, which are smaller than its
///   triangles: Gouraud shading rounds it and loses them, a dim smear being all the
///   corners catch, and Phong shading shows four, one per lamp and the sun's.
/// - The fine shiny ball has nine times the triangles, 1152, each smaller than a
///   highlight. Under Gouraud shading it has its highlights, a little soft: more
///   triangles are the other way to mend what Gouraud shading gets wrong, at their
///   price.
/// - The three tiles are alike, two triangles each, and over the middle of each hangs a
///   lamp: 0.9 above the left one, 0.4 above the middle one and 0.18 above the right
///   one. The nearer a lamp is to its tile, the more light the middle of the tile gets
///   and the less its corners do, where the light arrives at a slant. So every shading
///   model that computes the light at the corners, or once per triangle, shows the
///   tiles darker from left to right, the darkest under the nearest lamp, which is the
///   wrong way round (the book's Figure 13-7). Phong shading gives each its pool of
///   light, smaller and sharper the lower the lamp (Figure 13-10).
/// - The floor is one face of two triangles under all of it: two tones with a seam
///   under flat shading, evenly dull after that, and lit around the tiles, where the
///   lamps are, under Phong shading alone.
///
/// What stays wrong under all four is what no shading can mend, the corners in the
/// outlines of the balls and the cylinder.
///
/// A small ball marks each lamp: its color is far beyond the scale, so it shows the same
/// glowing yellow whatever the lights and the shading make of it. The tiles' color is
/// beyond the scale too, by less, which shows them as a longer exposure would. The
/// camera stands at (0, 6.6, -9.05) and looks down on the stage at the angle of the
/// 3-4-5 triangle, about 36.9 degrees, by a rotation matrix written out by hand; the
/// balls are octahedra subdivided and pushed out to a sphere, and the cylinder's outline
/// is a regular 12-gon, whose corners need a square root and no more. So nothing in the
/// picture's geometry calls cos or sin. At distance 1 on a 16:9 canvas the floor,
/// 13.2 x 9, is all in view and no triangle is cut by the camera's clipping planes,
/// which would have its pieces lit one by one. Clears the canvas to a dark background
/// first; depends on the viewport, so render_demo's --distance zooms it.
void DrawShadingScene(Canvas& canvas, const Viewport& viewport, ShadingModel shading_model);

/// The coordinate axes through the origin, a dot at the origin and a colored dot in
/// each corner (red top-left, green top-right, blue bottom-left, black bottom-right): a
/// quick visual check of the canvas's coordinate system. Drawn with PutPixel alone, on
/// whatever background the canvas already has.
void DrawAxesScene(Canvas& canvas);

}  // namespace cgfs::demo
