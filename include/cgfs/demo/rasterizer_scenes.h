#pragma once

#include <cgfs/camera/viewport.h>
#include <cgfs/image/canvas.h>
#include <cgfs/image/texture.h>
#include <cgfs/rasterizer/shading.h>
#include <cgfs/rasterizer/triangle.h>

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

/// The picture of the Textures chapter, the first that is painted: a crate on a floor
/// that runs to the horizon, both painted with `texture`, which is meant to be the crate
/// texture (see CrateTexture in textures.h), drawn with RenderScene under an ambient
/// light and a sun, through a depth buffer. The texture is carried across the triangles
/// by `texture_mapping` and looked up by `texture_filter`, and `texture` has mipmap
/// levels or has none: the scene is one to be drawn several times, the pictures of the
/// chapter as far as it is implemented.
///
/// The two objects are painted with one texture by one method. With the default,
/// nearest neighbor filtering, a pixel takes the color of the one texel its point of the
/// texture falls in. What tells the objects apart is the size the texture is drawn at:
///
/// - The crate, a cube of side 2 with the whole texture on every face, stands about 5
///   in front of the camera, turned to show two faces. At 1920 x 1080 a face is about
///   340 pixels wide and 400 to 580 high for the 512 x 512 texels on it: between one and
///   one and a half texels to a pixel. This is what the texture was made for, and the
///   crate looks like its texture.
/// - The floor is 48 x 48 tiles of 4 x 4, each with the whole texture on it, and reaches
///   from under the camera to 190 away, so it is drawn at every size. At the bottom of
///   the picture, less than 2 from the camera, a texel is 4 pixels wide and 3 high, and
///   the wood is made of blocks of one color: the texture is drawn larger than it is,
///   which is what bilinear filtering mends. Toward the horizon it is drawn smaller and
///   smaller: halfway up the picture a pixel covers 2 texels in depth, at 40 away 120,
///   and near the horizon more than a thousand, and each shows the one texel its point
///   happens to fall in. The boards between the tiles break up into dashes and dots and
///   the wood into noise, patterns that are not in the texture: what mipmapping mends.
///   In between, around the crate, the floor is about right.
///
/// With TextureFilter::kBilinear a pixel blends the four texels around its point, and the
/// first of the two is mended: the blocks at the bottom of the picture are gone, the
/// wood there running from one texel's color to the next. The crate is a little softer
/// and otherwise as it was. The floor toward the horizon is no better: four texels of
/// the hundreds under a pixel are as much of an accident as one, and the dashes and the
/// noise stay.
///
/// With a texture that has its mipmap levels (see Mipmapping) the second is mended: the
/// tiles toward the horizon are painted with the copies of the texture that suit their
/// size, in which a texel is the mean of the many a pixel stood for, and the noise gives
/// way to the wood's color, with the boards between the tiles as lines across it. The
/// price shows nearer by: a tile is painted with the copy that suits the direction it
/// is drawn smallest in, its depth, and so loses some of its sharpness across. The
/// crate, a texel and a half to a pixel across at 1920 x 1080, is just past the border
/// between the texture and its first copy and is painted with the copy at half the
/// size: its grain is smoother and a little softer. (DrawCheckerboardScene is the same
/// floor with a texture that shows all this more harshly.)
///
/// With TextureMapping::kLinear the picture is the chapter's first, and wrong: u and v
/// are interpolated over the picture of every triangle, so on every face that leans away
/// from the camera, which is all of them, the two triangles of the face are each
/// stretched their own way and the boards bend where the triangles meet: slightly on the
/// crate, as in the book's figure, and into zigzags on the floor tiles near the camera,
/// which lean the most. TextureMapping::kPerspectiveCorrect, the default, is the same
/// scene with the boards straight.
///
/// The floor is turned by the angle of the 5-12-13 triangle and the crate by that of the
/// 8-15-17 triangle, and the camera, at (0, 1.6, 0), looks down by that of the 7-24-25
/// triangle, about 16 degrees, all three by rotation matrices written out from those
/// numbers: nothing in the picture's geometry calls cos or sin. The camera has the
/// clipping planes of its viewport, which cut the floor: a cut tile shows what the whole
/// tile shows, its new corners having the texture coordinates of the tile at the cut.
/// The two lights give every face one light value, so the scene is drawn with flat
/// shading, which is then exact. Clears the canvas to a dark background first; depends
/// on the viewport, so render_demo's --distance zooms it.
void DrawTexturesScene(Canvas& canvas, const Viewport& viewport, const Texture& texture,
                       TextureMapping texture_mapping = TextureMapping::kPerspectiveCorrect,
                       TextureFilter texture_filter = TextureFilter::kNearestNeighbor);

/// The picture of mipmapping: the floor of the textures picture, under the same camera
/// and the same two lights, and on it a row of five cubes that runs away from the
/// camera, all painted with `texture`, which is meant to be the checkerboard (see
/// CheckerboardTexture in textures.h): the book's example of a texture that goes wrong
/// when it is drawn small. Whether it does is up to the texture. The picture is one to
/// be drawn twice, with a texture that has its mipmap levels and with one that has
/// not (see Mipmapping); nothing else differs between the two.
///
/// - The floor is 48 x 48 tiles of 4 x 4, each with the whole checkerboard on it, 64
///   squares a side, so a square is a sixteenth of a unit. Under the camera a square is
///   32 pixels wide at 1920 x 1080 and the floor is a checkerboard. About 10 from the
///   camera a square is one pixel in depth, and from there to the horizon a pixel
///   stands for more and more squares, black and white in equal parts, and ought to be
///   gray.
/// - The cubes are of side 2, with the whole checkerboard on every face, and stand 6, 12,
///   24, 48 and 96 from the camera, each twice as far as the one before and so half as
///   large in the picture: the book's object seen from farther and farther away.
///
/// Without mipmapping every pixel shows the one texel its point falls in, black or
/// white. Where the squares are smaller than the pixels, which squares the pixels of a
/// row hit has nothing to do with the squares and everything to do with how the two
/// grids happen to lie on each other: the far floor is rings and arcs of black and
/// white, a moire that is not in the texture, the faces of the third cube are speckled,
/// and those of the last two are striped.
///
/// With mipmapping every triangle is painted with the level of the texture that suits
/// the size it is drawn at (see MipmapLevel). The levels of the checkerboard are squares
/// of 2 x 2 texels, then of single texels, and from the third on one gray, the mean of
/// as many black texels as white ones. The tiles of the floor from about 12 away to the
/// horizon get such a level, and that part of the floor is the even gray it should be:
/// at 1920 x 1080 the top 130 rows of it or so. The cubes get, at that size, the levels
/// 0, 1, 2, 3 and 4, one more for every doubling of the distance: the first two are
/// checkered, the third is a checkerboard of single texels, and the last two are gray,
/// with their two faces told apart by the light. (The first cube is seen from the side,
/// and its narrow face, drawn smaller than the wide one, gets level 2.) A level that
/// still has squares shows, under nearest neighbor filtering, exactly what the texture
/// shows, a texel of it lying inside one square: the two pictures differ only where the
/// second is gray.
///
/// What mipmapping leaves, since a level is chosen for a whole triangle: a hard border
/// where the tiles change from checkered to gray, instead of a slow fading; near the
/// right edge of the picture one half of a tile, and the pieces the camera's clipping
/// plane leaves of the tile beside it, which come out just on the checkered side of
/// that border while the triangles around them come out on the gray side; and, in the
/// tiles just before the gray, a level of single texels drawn at about a texel to a
/// pixel, which under nearest neighbor filtering still makes a moire, though a weaker
/// one. Bilinear filtering on top of mipmapping softens the last; the first two are
/// what the book's trilinear filtering is for.
///
/// `texture_mapping` and `texture_filter` are as in the textures picture. The cubes are
/// turned by the angle of the 20-21-29 triangle, about 43.6 degrees, so that both faces
/// of a cube show at about the same width, by a matrix written out from those numbers;
/// like the textures picture this one calls neither cos nor sin. Drawn with flat
/// shading, through a depth buffer and with back-face culling. Clears the canvas to a
/// dark background first; depends on the viewport, so render_demo's --distance zooms it.
void DrawCheckerboardScene(Canvas& canvas, const Viewport& viewport, const Texture& texture,
                           TextureMapping texture_mapping = TextureMapping::kPerspectiveCorrect,
                           TextureFilter texture_filter = TextureFilter::kNearestNeighbor);

/// The coordinate axes through the origin, a dot at the origin and a colored dot in
/// each corner (red top-left, green top-right, blue bottom-left, black bottom-right): a
/// quick visual check of the canvas's coordinate system. Drawn with PutPixel alone, on
/// whatever background the canvas already has.
void DrawAxesScene(Canvas& canvas);

}  // namespace cgfs::demo
