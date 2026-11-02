# Computer graphics from scratch

A project for accelerated education about computer graphics: a software 3D renderer
written from scratch in C++, chapter by chapter, based on the book
[*Computer Graphics from Scratch*](https://gabrielgambetta.com/computer-graphics-from-scratch/)
by Gabriel Gambetta. Every algorithm is implemented by hand and pinned by tests, so the
code can be read as a worked version of the book. No graphics libraries are involved:
vector math, the pixel canvas and the image writers are all part of the project.

It currently ray-traces spheres with ambient, point and directional lights, diffuse
shading and specular highlights, and writes the result to a PNG or PPM file.
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
./build/release/apps/render_demo                       # 1920x1080 specular scene -> render.png
./build/release/apps/render_demo --scene flat --width 480 --height 270 --output flat.png
./build/release/apps/render_demo --distance 2 --output zoomed.ppm
./build/release/apps/render_demo --help
```

Scenes: `specular` (three shiny spheres on a shiny yellow floor with ambient, point and
directional lights; the lights show up as highlights), `diffuse` (the same scene with
matte surfaces), `flat` (the spheres under full ambient light), `axes` (the coordinate
axes and corner dots, no ray tracing). A larger `--distance` narrows the field of view.

## Layout

```
include/cgfs/   public headers, one per module
    vec3.h        Vec3 and Dot, Cross, Length, Normalized, Reflect
    color.h       Color (double channels, nominal 0..255), Rgb8, ToRgb8
    canvas.h      Canvas: pixel buffer with a centered coordinate system, Save()
    image_io.h    PPM writer and reader, PNG writer
    viewport.h    Viewport: CanvasToViewport(x, y), field of view
    scene.h       Sphere (with its specular exponent), the lights (a std::variant) and Scene
    raytracer.h   IntersectRaySphere and RayTracer: Render, TraceRay, ComputeLighting
    demo_scenes.h the scenes shared by render_demo and the golden tests
src/            implementations
apps/           render_demo
tests/          Catch2 unit tests and the golden-image tests; tests/golden/ holds the
                reference renders
third_party/    stb_image_write.h (PNG output)
docs/SPEC.md    design specification
```

CMake options (all default on except the sanitizers): `CGFS_BUILD_TESTS`,
`CGFS_BUILD_APPS`, `CGFS_WARNINGS_AS_ERRORS`, `CGFS_ENABLE_SANITIZERS`.

## Golden images

`tests/golden_test.cpp` renders the demo scenes at 320x180 and compares them pixel for
pixel with the reference images in `tests/golden/`. If the rendering behaviour changes on
purpose, regenerate the references with a Release build and review the pictures before
committing:

```bash
./build/release/apps/render_demo --scene flat --width 320 --height 180 --output tests/golden/flat_320x180.ppm
./build/release/apps/render_demo --scene diffuse --width 320 --height 180 --output tests/golden/diffuse_320x180.ppm
./build/release/apps/render_demo --scene specular --width 320 --height 180 --output tests/golden/specular_320x180.ppm
```
