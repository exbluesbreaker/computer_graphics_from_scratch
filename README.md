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
a list of colored triangles that index them, as wireframes, and renders scenes in which
such models are placed any number of times as instances, each scaled, turned and moved
by its own model transform, as seen by a camera that can stand anywhere and look any way.
The transforms are 4 x 4 matrices; the projection, as in the book, deliberately is not.
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
./build/release/apps/render_demo --help
```

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
turned, every vertex going through one matrix before `ProjectVertex`), `axes` (the
coordinate axes and corner dots, no ray tracing). A larger `--distance` narrows the field
of view of the ray-traced scenes, the cube, the objects, the instances and the camera
scene.

## Layout

```
include/cgfs/   public headers: one folder per layer, one file per concept
  math/         vec3.h          Vec3 and Dot, Cross, Length, Normalized, Reflect
                mat4.h          Mat4: 4x4 matrices in homogeneous coordinates, their product and
                                application to a point, scaling, translation and rotation about Y
  image/        color.h         Color (double channels, nominal 0..255), Rgb8, ToRgb8
                canvas.h        Canvas: pixel buffer with a centered coordinate system, Save()
                image_io.h      PPM writer and reader, PNG writer
  camera/       viewport.h      Viewport: CanvasToViewport(x, y), field of view
                camera.h        Camera (position and orientation) and MakeCameraMatrix
  raytracer/    scene.h         Sphere (with its specular exponent and reflectivity), the
                                lights (a std::variant) and Scene
                raytracer.h     IntersectRaySphere and RayTracer: Render, ClosestIntersection,
                                TraceRay (recursive, for reflections), ComputeLighting (shadows)
  rasterizer/   point.h         Point (x, y and the intensity h), and ToPixel (the one
                                coordinate-to-pixel conversion)
                interpolate.h   Interpolate
                line.h          DrawLine
                triangle.h      DrawWireframeTriangle, DrawFilledTriangle, DrawShadedTriangle
                projection.h    ViewportToCanvas and ProjectVertex: a Vec3 seen through a
                                Viewport becomes the Point the primitives draw
                object.h        Triangle (three vertex indices and a color) and RenderObject:
                                project an object's vertices, outline its triangles
                model.h         Model: an object's vertex list and triangle list, stored once,
                                and RenderModel: draw it under a transform matrix
                instance.h      Instance (a model index, position, orientation and scale),
                                MakeModelMatrix and RenderInstance
                scene.h         RenderScene: every instance of a model list and an instance
                                list, in order, as a Camera sees them
  demo/         raytracer_scenes.h, rasterizer_scenes.h
                                the pictures shared by render_demo and the golden tests
src/<layer>/    implementations, mirroring include/cgfs/<layer>/
apps/           render_demo; its scene table is the one list of pictures
tests/<layer>/  Catch2 unit tests, mirroring the layers; tests/demo/golden/ holds the
                reference renders
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
```
