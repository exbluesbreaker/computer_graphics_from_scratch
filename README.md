# Computer graphics from scratch

A project for accelerated education about computer graphics: a software 3D renderer
written from scratch in C++, chapter by chapter, based on the book
[*Computer Graphics from Scratch*](https://gabrielgambetta.com/computer-graphics-from-scratch/)
by Gabriel Gambetta. Every algorithm is implemented by hand and pinned by tests, so the
code can be read as a worked version of the book. No graphics libraries are involved:
vector math, the pixel canvas and the image writers are all part of the project.

It currently ray-traces spheres with ambient, point and directional lights, diffuse
shading, specular highlights, shadows and reflections, and writes the result to a PNG or
PPM file. The rasterizer, the book's second half, has started: it draws lines, filled
triangles and shaded triangles (an intensity per vertex, interpolated across the
triangle) with nothing but `PutPixel`, projects 3D vertices onto the canvas through the
same viewport the ray tracer looks through, draws objects given as a list of vertices and
a list of colored triangles that index them, as wireframes, filled or shaded, and renders
scenes in which such models are placed any number of times as instances, each scaled,
turned and moved by its own model transform, as seen by a camera that can stand anywhere
and look any way. The transforms are 4 x 4 matrices; the projection, as in the book,
deliberately is not, and between the two the scene is clipped in 3D against the planes
that bound what the camera can see, so the camera may stand in the middle of a scene.
A depth buffer, which keeps 1/z for every pixel, removes hidden surfaces: through it a
pixel shows the nearest triangle, in whatever order the scene is listed. Before that,
back-face culling removes the triangles that are turned away from the camera, about half
of a scene, without drawing them. And a scene can be lit: by the same lights and the same
illumination equation as the ray tracer's spheres, with the light computed once per
triangle (flat shading), at the corners of every triangle and interpolated (Gouraud
shading, with each triangle's own normal or with the normals of the model's surface) or
at every pixel from interpolated normals (Phong shading). And a model can be painted
with a texture: an image whose points are pinned to the corners of the triangles and
carried across them to every pixel, perspective-correct, so that a cube becomes a
wooden crate. The color of a pixel is that of the one texel its point falls in, nearest
neighbor filtering, the simplest rule there is, or, when asked for, a blend of the four
texels around the point, bilinear filtering, which smooths a texture that is drawn
larger than it is. Mipmapping, which the book goes on to for a texture drawn smaller
than it is, is the next step and is not there yet.
`docs/SPEC.md` describes the design, the architecture and the roadmap.

## Requirements

- CMake 3.25 or newer and a C++20 compiler. Developed with GCC 13 on Linux; Clang 16 or
  newer is expected to work.
- Ninja (recommended; the presets use it).
- Network access on the first configure: the test framework (Catch2 v3) is fetched
  unless a system-wide copy is installed.

## Build, test, render

```bash
cmake --preset dev          # Debug build with sanitizers, into build/dev
cmake --build --preset dev
ctest --preset dev
```

Use `--preset release` for an optimized build in `build/release`. Then:

```bash
./build/release/apps/render_demo                       # 1920x1080 reflective scene -> render.png
./build/release/apps/render_demo --scene flat --width 480 --height 270 --output flat.png
./build/release/apps/render_demo --distance 2 --output zoomed.ppm
./build/release/apps/render_demo --scene lines --output lines.png   # the rasterizer
./build/release/apps/render_demo --scene triangles --output triangles.png
./build/release/apps/render_demo --scene shaded --output shaded.png
./build/release/apps/render_demo --scene cube --distance 2 --output cube.png   # perspective
./build/release/apps/render_demo --scene objects --output objects.png   # objects made of triangles
./build/release/apps/render_demo --scene instances --output instances.png   # a scene of instances
./build/release/apps/render_demo --scene camera --output camera.png   # model and camera transforms
./build/release/apps/render_demo --scene clipping --output clipping.png   # clipping, shaded solids
./build/release/apps/render_demo --scene filled --output filled.png   # hidden-surface removal
./build/release/apps/render_demo --scene shading --shading flat --output flat.png   # shading:
./build/release/apps/render_demo --scene shading --shading gouraud-faceted --output faceted.png   # one
./build/release/apps/render_demo --scene shading --shading gouraud --output gouraud.png   # scene,
./build/release/apps/render_demo --scene shading --shading phong --output phong.png   # four ways
./build/release/apps/render_demo --scene textures --output textures.png   # a texture, at every size
./build/release/apps/render_demo --scene textures --texture-mapping linear --output bent.png
./build/release/apps/render_demo --scene textures --texture-filter bilinear --output smooth.png
./build/release/apps/render_demo --help
```

Every run prints what it drew and where it went, and on a line of its own how long the
drawing took:

```
rendered 'shading' (gouraud-faceted shading) at 1920x1080 -> faceted.png
render time: 27.60 ms
```

The time is the render alone, without writing the file, to a hundredth of a
millisecond; it is a single run, so compare several before trusting a small difference.
The first line names the shading model for the `shading` scene, the one picture it
changes, so the four can be told apart: at 1920 x 1080 they take about 23 ms (flat),
27 ms (Gouraud, in either form) and 74 ms (Phong) on the development machine. For the
`textures` scene it names the texture mapping and the texture filter instead; that
picture takes about 62 ms under either mapping with nearest neighbor filtering and
about 71 ms with bilinear filtering, and its texture is read before the clock starts.

Scenes: `reflective` (three shiny spheres on a shiny yellow floor with ambient, point and
directional lights; the lights show up as highlights, the spheres cast shadows and mirror
each other and the floor), `specular` (the same scene without reflections), `diffuse`
(the same scene with matte surfaces), `flat` (the spheres under full ambient light, so no
shading and no shadows), `mirrorball` (a chrome ball on a dark polished floor under a
dusk sky, ringed by colored spheres of different finishes, with marbles in front and
spheres floating above; two point lights throw crossing shadows), `lines` (the
rasterizer's first picture: a sunburst of colored lines and string-art envelopes in the
corners, drawn with `DrawLine`), `triangles` (the rasterizer's second: outlined
triangles in the corners and a color wheel of filled wedges, drawn with
`DrawFilledTriangle` and `DrawWireframeTriangle`), `shaded` (the rasterizer's third: a
green triangle shaded like the book's demo and a glowing color wheel, drawn with
`DrawShadedTriangle`), `cube` (the rasterizer's first 3D picture: the wireframe of the
book's cube, its corners projected with `ProjectVertex` and its edges drawn with
`DrawLine`, the front face blue and the back face red), `objects` (the first picture of
objects made of triangles: the book's cube, eight vertices and twelve colored triangles,
and a square pyramid, each a vertex list and a triangle list that `RenderObject` projects
and outlines), `instances` (the first scene of instances: the same cube and pyramid stored
once each as models and placed sixteen times by position, a street of houses leading to a
tower, drawn with `RenderScene`), `camera` (the chapter's closing picture: two cubes, one
scaled and one turned by their model transforms, seen from a camera that has moved and
turned, every vertex going through one matrix before `ProjectVertex`), `clipping` (the
picture of the clipping chapter and the first with shaded solids: a city of boxes and
pyramids on a checkerboard floor, seen by a camera standing in it; the floor runs on
under and behind the camera, high-rises leave the picture at the top, and a lantern an
arm's length away is cut open by the near plane, all of it clipped by `ClipScene` between
the transforms and the projection), `filled` (the picture of hidden-surface removal:
fifteen copies of the book's six-colored cube drawn with `DrawFilledTriangle` through a
`DepthBuffer`, a wall of 3 x 3 and a row on each side, listed so that painting them in
order would put the back of every cube over its front and every cube over the one in
front of it; back-face culling removes the faces a cube turns away, 102 of the 180
triangles, and through the depth buffer each cube shows the three faces it should),
`shading` (the picture of the Shading chapter, described below), `textures` (the
picture of the Textures chapter as far as it is implemented: a wooden crate on a floor
paved with the same texture, also described below),
`axes` (the coordinate axes and corner dots, no ray tracing). A larger `--distance`
narrows the field of view of the ray-traced scenes, the cube, the objects, the
instances, the camera scene, the clipping scene, the filled cubes, the shading scene and
the textures scene.

### Shading: one scene, four ways

`--shading flat|gouraud-faceted|gouraud|phong` chooses how the `shading` scene is lit
(Phong when it is not given; the other scenes do not read it). These are the four
pictures of the book's chapter, in its order: flat shading, Gouraud shading before the
model has normals, Gouraud shading with them, and Phong shading.

The scene is a floor with five solids on it and, in front of them, three lamps hanging
low over three tiles, under a little ambient light and a sun; a small yellow ball marks
each lamp. Every object is there for something that one shading model gets right and
the one before it does not:

| object | `flat` | `gouraud-faceted` | `gouraud` | `phong` |
|---|---|---|---|---|
| blue cube: flat faces | faint seam across each lit face | right | right | right |
| green ball: matte, 128 triangles | faceted | faceted | right | right |
| orange cylinder: round sides, flat ends, shiny | strips | strips | round, sharp rim; highlight smeared | right: a streak of highlight |
| red ball: shiny, 128 triangles | faceted | faceted | round, highlights lost | right: four highlights |
| purple ball: shiny, 1152 triangles | fine facets | fine facets | nearly right: highlights a little soft | right |
| three tiles, lamps at 0.9, 0.4 and 0.18 above them | a seam across each; darker the lower the lamp | no seam; darker the lower the lamp | the same | right: a pool of light on each, sharper the lower the lamp |
| floor: one face of two triangles | two tones, a seam from corner to corner | evenly dull | evenly dull | lit around the tiles |

The modes compute the light once per triangle (`flat`), at the corners of every
triangle with that triangle's own normal (`gouraud-faceted`), at the corners with the
normals of the model's surface (`gouraud`), and at every pixel from interpolated normals
(`phong`).

The tiles show the artifact the book warns of (its Figure 13-7). The three are alike and
only the height of the lamp over each differs. The nearer a lamp is to its tile, the
more light the middle of the tile gets, and the less its four corners do, since the
light reaches them at more and more of a slant. Gouraud shading computes the light at
the corners and interpolates, so the tile under the nearest lamp comes out the darkest
of the three, which is the wrong way round; only Phong shading, which lights every
pixel, finds the bright spot under each lamp. The two shiny balls show the other way to
mend Gouraud shading: with nine times the triangles, each smaller than a highlight, the
purple ball keeps the highlights the red one loses.

What stays wrong under all four is the outline of the balls and the cylinder, which
shows the corners of their triangles: shading changes how a surface is lit, not its
shape.

The two Gouraud modes are one algorithm with two answers to "which normal does a corner
have". Faceted Gouraud shading gives every corner the normal of its own triangle, so the
triangles that meet at a vertex of a ball each light it differently and the facets stay.
Gouraud shading reads the normals the model states, and a ball states those of a sphere.
For a model that states none, like the cube, the tiles and the floor, the two are the
same picture.

In code, a scene is lit by handing `RenderScene` a list of lights and a `ShadingModel`
in place of a triangle primitive:

```cpp
const std::vector<cgfs::Light> lights = {
    cgfs::AmbientLight{.intensity = 0.15},
    cgfs::DirectionalLight{.intensity = 0.2, .direction = {-1.0, 2.0, -1.5}},
    cgfs::PointLight{.intensity = 0.45, .position = {0.0, 0.42, -2.95}},
};
cgfs::DepthBuffer depth_buffer(canvas);
cgfs::RenderScene(canvas, viewport, models, instances, camera, lights,
                  cgfs::ShadingModel::kGouraud, &depth_buffer);
```

The lights are in the world's coordinates. A model's triangles may state a normal at
each corner, the normal of the curved surface they stand for, which is what lets Gouraud
and Phong shading show a ball as round; a triangle that states none is lit as the flat
face it is, and `ShadingModel::kGouraudFaceted` lights every triangle that way whatever
it states. `Model::specular` is the exponent of the model's highlight (`kMatte`, the
default, for none). The rasterizer casts no shadows. `docs/SPEC.md`, section 3.13, has
the formulas, and section 14, decision 18, the choices.

### Textures: one texture, at the size it suits and at every other

The `textures` scene is a wooden crate standing on a floor that runs to the horizon.
Both are painted with one texture, `assets/textures/wooden_crate.ppm`, 512 x 512
texels, and by one method: a pixel finds the point `(u, v)` of the texture that its
triangle shows there and, unless it is told otherwise, takes the color of the one texel
that point falls in. The book calls that nearest neighbor filtering. The scene is made
to show what that gets right, what it leaves, and what the filters of the chapter's
second half do about it:

| where | how large the texture is drawn (at 1920 x 1080) | nearest neighbor filtering | bilinear filtering |
|---|---|---|---|
| the crate | one to one and a half texels to a pixel: a face is some 340 pixels wide and 400 to 580 high | right: this is the size the texture was made for | the same, a little softer |
| the floor at the bottom of the picture, under 2 from the camera | too large: a texel is 4 pixels wide and 3 high | blocks of one color with sharp borders | right: the wood runs smoothly from one texel's color to the next |
| the floor 3 to 5 from the camera, in front of the crate | within a factor of about 2 of its own size, too large across and then too small in depth | nearly right | nearly right, smoother across |
| the floor toward the horizon | too small, mostly in depth: a pixel covers 2 texels in the middle of the picture, 25 at 18 away, 120 at 41 away, over 1600 under the horizon | the boards break up into dashes and dots, the wood into noise | the same dashes and noise, a little milder |

The crate looks good because a pixel there shows about one texel. The floor is the
object that needs the filters. Where a texture is drawn larger than it is, every pixel
inside a texel gets the texel's one color, and the texels show as blocks. Where it is
drawn smaller, a pixel covers many texels and shows whichever one its point happens to
fall in; the rest are never looked at, so fine detail turns into noise and regular
detail into patterns that are not in the texture. (A floor is drawn too small in depth
long before it is across, because it leans away: at a distance of 40 a pixel covers 25
times as much floor in depth as across.)

`--texture-filter nearest-neighbor|bilinear` chooses how the color at a point of the
texture is made from its texels, nearest neighbor when it is not given:

- `nearest-neighbor` takes the texel the point falls in: `tx = u * width` and
  `ty = v * height`, rounded down.
- `bilinear` keeps the fractions `fx` and `fy` that the rounding cuts off and blends the
  four texels around the point with them, the texel at `(tx, ty)`, the next in its row
  and the two below them: across the top pair, across the bottom pair, and then between
  the two results. The color changes evenly from one texel to the next instead of
  jumping at the border, so the blocks become gradients. This is the book's bilinear
  filtering, and it mends the first of the two faults: compare the bottom of
  `--texture-filter bilinear` with the default picture. In its lowest hundred rows two
  pixels side by side have the same color 68% of the time under nearest neighbor
  filtering and 1% of the time under bilinear filtering.

Bilinear filtering does nothing for the second fault. Four neighbouring texels of the
hundreds under a pixel are as much of an accident as one, and the floor toward the
horizon is still noise. Mipmapping, which keeps the texture at several sizes and picks
the one that fits, mends that, and trilinear filtering the seams between those sizes.
**Neither is implemented yet**, on purpose: that part of the picture is what they will
change. This is also why bilinear filtering is something to ask for and not the
default: it is right where the texture is drawn too large and no better anywhere else,
at four texels a pixel, about a sixth more time on this picture.

The bilinear picture follows the listing in the book, in which a texel is the point at
the top-left corner of the square that nearest neighbor filtering gives it. It
therefore lies half a texel nearer the texture's top-left corner than the default
picture, which shows only when the two are laid over each other.

`--texture-mapping linear|perspective-correct` chooses how the texture coordinates get
from the corners of a triangle to its pixels, perspective-correct when it is not given.
These are the two pictures of the chapter's first half, and either goes with either
filter:

- `linear` interpolates `u` and `v` over the picture of the triangle, like a color.
  That is what one tries first, and it is wrong for every face that leans away from
  the camera, because equal steps on the canvas are not equal steps on the face. The
  two triangles of a face are each stretched their own way, and a straight board
  bends where they meet: slightly on the crate (look at the plank across each face),
  and into zigzags on the floor tiles nearest the camera.
- `perspective-correct` interpolates `u/z` and `v/z`, which do change by equal steps
  on the canvas, along with `1/z`, which the depth buffer needed anyway, and divides
  at every pixel: `u = (u/z) / (1/z)`. Every board is straight.

In code, a model is painted by giving it a texture and its triangles a point of the
texture at each corner:

```cpp
const cgfs::Texture crate(cgfs::ReadPpm("assets/textures/wooden_crate.ppm"));

cgfs::Model face;
face.vertices = {{-1, 2, 0}, {1, 2, 0}, {1, 0, 0}, {-1, 0, 0}};   // a square, seen from -Z
face.triangles = {
    {.v = {0, 1, 2}, .uvs = {cgfs::Uv{0, 0}, cgfs::Uv{1, 0}, cgfs::Uv{1, 1}}},
    {.v = {0, 2, 3}, .uvs = {cgfs::Uv{0, 0}, cgfs::Uv{1, 1}, cgfs::Uv{0, 1}}},
};
face.texture = &crate;   // the model points to its texture; keep it alive

cgfs::RenderScene(canvas, viewport, models, instances, camera, lights,
                  cgfs::ShadingModel::kFlat, &depth_buffer, cgfs::BackFaceCulling::kOn,
                  cgfs::TextureMapping::kPerspectiveCorrect, cgfs::TextureFilter::kBilinear);
```

`u` runs from the left edge of the texture, 0, to its right edge, 1, and `v` from its
top, 0, to its bottom, 1, whatever its size; the coordinates belong to the corners of
the triangles, not to the vertices, since a corner of a crate is a different corner of
the picture on each face that meets there. The texels take the place of the triangle's
color and the light multiplies them, under any shading model. The last two arguments
may be left out: the mapping is then perspective-correct and the filter nearest
neighbor. A coordinate beyond an edge of the texture gives the texel at that edge under
either filter; nothing repeats, and the floor is paved by having a face for every tile.
`docs/SPEC.md`, section 3.14, has the formulas and section 14, decisions 19 and 20, the
choices.

The texture is a picture by another author (the file name in `assets/textures/` names
them); its licence is not recorded in the repository, which `assets/textures/README.md`
says and which wants settling before the repository is passed on.

Hidden-surface removal has two halves. The depth buffer is something a scene asks for:
`RenderScene` and `RenderObject` take a `DepthBuffer` of the canvas's size, and without
one triangles are painted in list order, as before. Back-face culling is something
`RenderScene` does unless it is told not to: every triangle that is turned away from the
camera is removed after the transforms and before the clipping, and
`BackFaceCulling::kOff` as the last argument draws them all. The `filled` scene uses
both. On that picture culling saves about a third of the time at 1920 x 1080, 44% of
the drawing itself (`docs/SPEC.md`, section 15, has the measurements).

Culling needs two things of a model: that it is closed, and that every triangle lists
its corners clockwise as seen from outside, which is how the triangle's normal
`(B - A) x (C - A)` comes to point out of the object. The order is part of the model and
is not computed. The wireframe scenes (`instances`, `camera`) and the `clipping` scene
turn culling off: the first two show every edge, and the third shows the inside of a
lantern that the near plane has cut open, which is made of back faces; it is still
painted without a depth buffer too, composed so that its list order is its depth order.
`docs/SPEC.md`, sections 3.12, 9 and 14, has the details.

## Layout

```
include/cgfs/   public headers: one folder per layer, one file per concept
  math/         vec3.h          Vec3 and Dot, Cross, Length, Normalized, Reflect
                mat4.h          Mat4: 4x4 matrices in homogeneous coordinates, their product and
                                application to a point and to a direction, scaling, translation
                                and rotation about Y
                plane.h         Plane, SignedDistance and IntersectSegmentPlane
                bounding_sphere.h  BoundingSphere and MakeBoundingSphere
                triangle_normal.h  TriangleNormal: the normal of a triangle from its corners
  image/        color.h         Color (double channels, nominal 0..255), Rgb8, ToRgb8
                pixel_index.h   PixelIndex: the centered coordinate system, as one function
                canvas.h        Canvas: pixel buffer with a centered coordinate system, Save()
                depth_buffer.h  DepthBuffer: 1/z per pixel of a canvas, for hidden-surface
                                removal
                image_io.h      PPM writer and reader, PNG writer
                texture.h       Texture: an image to paint triangles with, looked up by
                                GetTexel(u, v) with a TextureFilter (nearest neighbor or
                                bilinear), and Uv, a point of it
  camera/       viewport.h      Viewport: CanvasToViewport(x, y), field of view
                camera.h        Camera (position, orientation and clipping planes) and
                                MakeCameraMatrix
                clipping_planes.h  MakeClippingPlanes: the five planes of what a viewport shows
  lighting/     light.h         AmbientLight, PointLight, DirectionalLight and Light, the
                                std::variant of the three; TransformLight, a light in other
                                coordinates
                illumination.h  the illumination equation: DiffuseReflection and
                                SpecularReflection, its two terms, ComputeIllumination, the sum
                                over a list of lights, and kMatte for a surface without a highlight
  raytracer/    scene.h         Sphere (with its specular exponent and reflectivity) and Scene,
                                the spheres and the lights
                raytracer.h     IntersectRaySphere and RayTracer: Render, ClosestIntersection,
                                TraceRay (recursive, for reflections), ComputeLighting (shadows)
  rasterizer/   point.h         Point (x, y, the intensity h, the depth 1/z, a normal and
                                texture coordinates), and ToPixel (the one
                                coordinate-to-pixel conversion)
                interpolate.h   Interpolate
                line.h          DrawLine
                triangle.h      DrawWireframeTriangle, DrawFilledTriangle, DrawShadedTriangle
                                (the last two through a depth buffer when given one),
                                DrawTriangleFunction, the signature they share,
                                DrawTexturedTriangle, which paints a texture, carried across
                                by a TextureMapping (linear or perspective-correct) and
                                looked up by a TextureFilter, and
                                DrawPhongTriangle, which computes the light at every pixel
                projection.h    ViewportToCanvas and ProjectVertex: a Vec3 seen through a
                                Viewport becomes the Point the primitives draw; UnprojectVertex,
                                the way back from a pixel and its depth
                object.h        Triangle (three vertex indices, a color, and an intensity, a
                                normal and texture coordinates per corner) and RenderObject: project an object's
                                vertices, draw its triangles outlined, filled or shaded, through
                                a depth buffer or painted in list order
                model.h         Model: an object's vertex list and triangle list, stored once,
                                with a bounding sphere, a specular exponent and a texture; TransformModel
                                and RenderModel: the model under a transform matrix, and its
                                wireframe
                instance.h      Instance (a model index, position, orientation and scale),
                                MakeModelMatrix and RenderInstance
                culling.h       IsBackFacing, CullBackFaces and the switch BackFaceCulling:
                                an object without the triangles turned away from the camera
                clipping.h      ClipScene, ClipInstance, ClipInstanceAgainstPlane,
                                ClipTrianglesAgainstPlane and ClipTriangle: what a list of
                                planes leaves of a scene
                shading.h       ShadingModel (flat, faceted Gouraud, Gouraud, Phong) and
                                RenderObject under lights: an object's triangles in their
                                colors, or in the object's texture, times the light computed
                                per triangle, per corner or per pixel
                scene.h         RenderScene: every instance of a model list and an instance
                                list, in order, as a Camera sees them: transformed, culled,
                                clipped, projected and drawn, with a triangle primitive or
                                under a list of lights
  demo/         raytracer_scenes.h, rasterizer_scenes.h
                                the pictures shared by render_demo and the golden tests
                textures.h      CrateTexture: the texture of the textures picture, read from
                                assets/textures/
src/<layer>/    implementations, mirroring include/cgfs/<layer>/
apps/           render_demo; its scene table is the one list of pictures
tests/<layer>/  Catch2 unit tests, mirroring the layers; tests/demo/golden/ holds the
                reference renders
assets/         textures/: the crate texture, as the JPEG it came as and the PPM that is read
third_party/    stb_image_write.h (PNG output)
docs/SPEC.md    design specification
CLAUDE.md       the design principles and the rules that keep this tree in shape
```

CMake options (all default on except the sanitizers): `CGFS_BUILD_TESTS`,
`CGFS_BUILD_APPS`, `CGFS_WARNINGS_AS_ERRORS`, `CGFS_ENABLE_SANITIZERS`.

## Golden images

`tests/demo/golden_test.cpp` renders the demo scenes (ray-traced and rasterized) at
320x180 and compares them pixel for pixel with the reference images in
`tests/demo/golden/`. If the rendering behaviour changes on
purpose, regenerate the references with a Release build and review the pictures before
committing:

```bash
./build/release/apps/render_demo --scene flat --width 320 --height 180 --output tests/demo/golden/flat_320x180.ppm
./build/release/apps/render_demo --scene diffuse --width 320 --height 180 --output tests/demo/golden/diffuse_320x180.ppm
./build/release/apps/render_demo --scene specular --width 320 --height 180 --output tests/demo/golden/specular_320x180.ppm
./build/release/apps/render_demo --scene reflective --width 320 --height 180 --output tests/demo/golden/reflective_320x180.ppm
./build/release/apps/render_demo --scene mirrorball --width 320 --height 180 --output tests/demo/golden/mirrorball_320x180.ppm
./build/release/apps/render_demo --scene lines --width 320 --height 180 --output tests/demo/golden/lines_320x180.ppm
./build/release/apps/render_demo --scene triangles --width 320 --height 180 --output tests/demo/golden/triangles_320x180.ppm
./build/release/apps/render_demo --scene shaded --width 320 --height 180 --output tests/demo/golden/shaded_320x180.ppm
./build/release/apps/render_demo --scene cube --width 320 --height 180 --output tests/demo/golden/cube_320x180.ppm
./build/release/apps/render_demo --scene objects --width 320 --height 180 --output tests/demo/golden/objects_320x180.ppm
./build/release/apps/render_demo --scene instances --width 320 --height 180 --output tests/demo/golden/instances_320x180.ppm
./build/release/apps/render_demo --scene camera --width 320 --height 180 --output tests/demo/golden/camera_320x180.ppm
./build/release/apps/render_demo --scene clipping --width 320 --height 180 --output tests/demo/golden/clipping_320x180.ppm
./build/release/apps/render_demo --scene filled --width 320 --height 180 --output tests/demo/golden/filled_320x180.ppm
./build/release/apps/render_demo --scene shading --shading flat --width 320 --height 180 --output tests/demo/golden/shading_flat_320x180.ppm
./build/release/apps/render_demo --scene shading --shading gouraud-faceted --width 320 --height 180 --output tests/demo/golden/shading_gouraud_faceted_320x180.ppm
./build/release/apps/render_demo --scene shading --shading gouraud --width 320 --height 180 --output tests/demo/golden/shading_gouraud_320x180.ppm
./build/release/apps/render_demo --scene shading --shading phong --width 320 --height 180 --output tests/demo/golden/shading_phong_320x180.ppm
./build/release/apps/render_demo --scene textures --texture-mapping perspective-correct --width 320 --height 180 --output tests/demo/golden/textures_perspective_correct_320x180.ppm
./build/release/apps/render_demo --scene textures --texture-mapping linear --width 320 --height 180 --output tests/demo/golden/textures_linear_320x180.ppm
./build/release/apps/render_demo --scene textures --texture-filter bilinear --width 320 --height 180 --output tests/demo/golden/textures_bilinear_320x180.ppm
```

The three `textures` references are 320 x 180 pictures of a scene composed for
1920 x 1080: in them the crate is drawn six times too small as well, and the texture is
nowhere drawn larger than it is, so the bilinear one shows no blocks removed. They pin
the arithmetic; the picture to look at is the large one.
