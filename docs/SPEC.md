# Specification

Software 3D renderer written from scratch in C++.

## 1. Purpose

The project exists for accelerated education about computer graphics. It implements the
renderers of the book *Computer Graphics from Scratch* by Gabriel Gambetta
(https://gabrielgambetta.com/computer-graphics-from-scratch/) step by step in C++,
with every algorithm written by hand and pinned by tests, so that the code can be read
alongside the book as a worked example.

This document is the design reference for the renderer: what it renders and with which
formulas, how the code is structured, how it is built and tested, and how it is expected
to grow. The current feature set is a ray tracer for spheres with ambient, point and
directional lights, diffuse shading, specular highlights, shadows and reflections, and
the first steps of the rasterizer, lines, filled and shaded triangles, the perspective
projection of vertices, wireframe objects made of triangles and scenes of instances of
models, each with a model transform, seen from a movable camera; section 13 lists what
comes next.

## 2. Goals and non-goals

Goals:

- G1. Behaviour pinned by tests: unit tests for every module and golden-image tests that
  fix the renderer's output pixel for pixel.
- G2. A structure that accommodates the planned next steps (a movable camera for the
  ray tracer, the rest of the rasterizer) without rewrites.
- G3. CMake as the only build system. A fresh clone builds and tests with three
  commands on Linux with GCC or Clang.
- G4. No required third-party runtime dependencies. "From scratch" is the point of the
  project: vector math, the canvas and image writing are our own code.
- G5. Full-HD renders of the demo scene take well under a second in a Release build.

Non-goals:

- GPU rendering, SIMD intrinsics, or any hand-tuned performance work.
- A scene file format or a GUI. Scenes are built in C++ code.
- Windows and macOS are not tested. Nothing platform-specific may be introduced, so they
  should keep working, but they are not part of the acceptance criteria.

## 3. Rendering model

The formulas below are the ones the code implements. The golden images (section 8)
depend on every detail here, so a change in this section is a change of the reference
images.

### 3.1 Canvas

- A canvas is a `width` x `height` buffer of RGB pixels, 8 bits per channel. Defaults:
  1920 x 1080, white background.
- Coordinates are centered: `(0, 0)` is the middle of the canvas, x grows to the right,
  y grows upward. The valid range is `-width/2 <= x < width/2` and
  `-height/2 <= y < height/2` (integer division).
- `PutPixel(x, y, color)` maps centered coordinates to the buffer as
  `column = width / 2 + x` and `row = height / 2 - 1 - y`, with row 0 at the top of the
  image. Pixels outside the buffer are silently ignored.
- Colors are written to the buffer by clamping each channel to `[0, 255]` and then
  truncating toward zero to an 8-bit integer.
- `Clear()` fills the canvas with its background color. `Clear(color)` sets a new
  background color first.
- The canvas can be saved to an image file. The format follows the file extension.

### 3.2 Viewport

- The camera sits at the origin and looks along +Z.
- The viewport is a `width` x `height` rectangle perpendicular to Z, centered on the
  Z axis at `distance` from the camera. Defaults: 1 x 1 at distance 1.
- `CanvasToViewport(x, y)` returns the 3D point on the viewport that canvas pixel
  `(x, y)` looks through:

  `Vx = x * width / canvas_width`, `Vy = y * height / canvas_height`, `Vz = distance`.

- The field of view in degrees is `2 * atan(width / 2 / distance)` horizontally and the
  same with `height` vertically. The defaults give about 53.13 degrees on both axes.
- The demo renders use `width = canvas_width / canvas_height`, `height = 1`, so that a
  16:9 canvas does not stretch the image.

### 3.3 Ray-sphere intersection

For a ray `P(t) = O + t * D` and a sphere with center `C` and radius `r`, let
`CO = O - C`. The hits are the roots of

`<D, D> t^2 + 2 <CO, D> t + <CO, CO> - r^2 = 0`.

`IntersectRaySphere` returns both roots `t1 = (-b + sqrt(disc)) / 2a` and
`t2 = (-b - sqrt(disc)) / 2a`. When the discriminant is negative it returns
`(+inf, +inf)`. A tangent ray returns two equal roots.

### 3.4 Ray tracing

- `Render` casts one ray per canvas pixel from `O = (0, 0, 0)` through
  `D = CanvasToViewport(x, y)` with `t_min = 1`, `t_max = +inf` and
  `recursion_depth = kReflectionDepth = 3`, and writes the returned color with
  `PutPixel`.
- `ClosestIntersection(O, D, t_min, t_max)` tests every sphere and returns the sphere
  with the smallest root `t` such that `t_min <= t <= t_max`, over all spheres and both
  roots, together with that `t`. Both bounds are inclusive. Infinite roots never count,
  so a miss returns no sphere and `t = +inf`. It serves the camera rays and the shadow
  rays alike.
- `TraceRay(O, D, t_min, t_max, recursion_depth)` calls `ClosestIntersection` and
  returns the scene's background color when nothing is hit.
- On a hit, `P = O + t * D`, `N = (P - C) / |P - C|`, `V = -D` (from the point back
  toward the ray's origin), and the local color is
  `sphere.color * ComputeLighting(P, N, V, sphere.specular)`. The product may exceed 255
  per channel; it is clamped only when written to the canvas.
- With `r = sphere.reflective`, the result is the local color when `recursion_depth <= 0`
  or `r <= 0`. Otherwise the ray bounces off in the mirror direction
  `R = Reflect(V, N) = 2 N <N, V> - V`, the reflected color is
  `TraceRay(P, R, kSurfaceEpsilon, +inf, recursion_depth - 1)`, and the result is
  `local_color * (1 - r) + reflected_color * r`. The epsilon is the one the shadow rays
  use (3.5), for the same reason: `P` lies on the sphere, which would otherwise be hit
  again at `t = 0`. A camera ray therefore bounces at most three times, and each bounce
  shades its own hit point with the full lighting model, shadows included.
- Pixel order during rendering is an implementation detail. The output must not depend
  on it.

### 3.5 Lighting

Three light types, each with an `intensity`:

- Ambient: adds `intensity` to every point.
- Point: has a `position`; the light direction at `P` is `L = position - P`.
- Directional: has a `direction` that points toward the light; `L = direction`.

Every sphere has a `specular` exponent `s`. The value `-1` (`kMatte`, the default)
means the surface has no highlight; the demo scenes use 10 (somewhat shiny), 500 (shiny)
and 1000 (very shiny).

`ComputeLighting(P, N, V, s)`, with `V` pointing from `P` toward the viewer, returns

```
i = sum(ambient I)
  + sum over point and directional lights that are not blocked of
        I * <N, L> / (|N| |L|)                      diffuse, skipped when <N, L> <= 0
      + I * (<R, V> / (|R| |V|))^s                  specular, with R = 2 N <N, L> - L,
                                                    skipped when s = -1 or <R, V> <= 0
```

A light is blocked, and `P` is in its shadow, when
`ClosestIntersection(P, L, kSurfaceEpsilon, t_max)` finds a sphere, with `t_max = 1` for
a point light (`L = position - P` reaches exactly to the light, so only spheres between
the two count) and `t_max = +inf` for a directional light. `kSurfaceEpsilon = 0.001`
starts the shadow ray a little past `P` because `P` lies on a sphere, which solves the
intersection equation at `t = 0`; without it every surface would shadow itself. A
blocked light contributes neither diffuse nor specular; ambient light is never blocked.

`R` is the direction of `L` mirrored about the normal (`Reflect(L, N)`): the highlight is
strongest when the viewer sits exactly where the light bounces to, and falls off as
`cos^s` of the angle between `R` and `V`, so a larger `s` gives a smaller, sharper spot.
As in the book, the specular term is not gated on `<N, L> > 0`; only the diffuse term is.
`N` is unit length by construction; the division by `|N|` is kept so the code reads like
the formula.

### 3.6 Rasterizer: lines and triangles

The rasterizer draws 2D primitives straight into the canvas with `PutPixel`; nothing of
the ray tracer is involved. A `Point` is a canvas position in the centered integer
coordinates of 3.1 together with an intensity `h`, a multiplier for the color that only
the shaded triangle reads; it defaults to 1, the full color.

- `Interpolate(i0, d0, i1, d1)` returns the values of a dependent variable `d` at the
  integers `i0, i0 + 1, ..., i1`, given `d = d0` at `i0` and `d = d1` at `i1`: that is
  `i1 - i0 + 1` values, with `values[i - i0]` the value at `i`. The slope is
  `a = (d1 - d0) / (i1 - i0)` and the values are the running sum
  `d0, d0 + a, d0 + 2a, ...`, computed by repeated addition as in the book's listing.
  When `i0 == i1` the result is `{d0}`. Precondition: `i0 <= i1`; with `i0 > i1` the
  result is empty.
- `DrawLine(P0, P1, color)` draws the segment from `P0` to `P1`, both ends included.
  When `|x1 - x0| > |y1 - y0|` the line is horizontal-ish: the endpoints are swapped if
  needed so that `x0 < x1`, `ys = Interpolate(x0, y0, x1, y1)`, and for every `x` in
  `x0..x1` the pixel `(x, ys[x - x0])` is set. Otherwise the line is vertical-ish (exact
  diagonals included) and the roles of x and y are exchanged: `y0 < y1` after the swap,
  `xs = Interpolate(y0, x0, y1, x1)`, and `(xs[y - y0], y)` is set for every `y`. Every
  column (row) of the line's span gets exactly one pixel, consecutive pixels are at most
  one step apart in the other coordinate, and both endpoint orders give the same pixels.
- The interpolated coordinate is a `double`. The pixel it selects is found by `ToPixel`,
  truncation toward zero (`static_cast<int>`), the conversion the book's code performs
  and the same rule as for colors in 3.1. Consequence: `-0.5` and `0.5` both select pixel 0, so the
  staircase of a line mirrors at the axes (section 14, decision 8).
- `DrawWireframeTriangle(P0, P1, P2, color)` draws the three edges with `DrawLine`.
- `DrawFilledTriangle(P0, P1, P2, color)` fills the triangle, edges included. The
  vertices are sorted so that `y0 <= y1 <= y2` (three conditional swaps, as in the
  book). The x coordinate of each side at every row it spans is
  `x01 = Interpolate(y0, x0, y1, x1)`, `x12 = Interpolate(y1, x1, y2, x2)` and
  `x02 = Interpolate(y0, x0, y2, x2)`. The short sides are concatenated into
  `x012 = x01 without its last value + x12`, which has one value per row like `x02`.
  With `m = floor(len(x012) / 2)`, `x_left` is `x02` when `x02[m] < x012[m]` and
  `x012` otherwise, and `x_right` is the other one. Then for every row `y` from `y0`
  to `y2` the pixels from `trunc(x_left[y - y0])` to `trunc(x_right[y - y0])`, both
  included, are set, with the truncation toward zero of the previous bullet. A shared
  edge is interpolated from the same two endpoints by both triangles that use it, so
  neighbouring triangles tile without gaps and overlap on the edge's pixels. Collinear
  vertices give (part of) a line. Section 15 records what the middle-row rule does to
  triangles two rows tall.
- `DrawShadedTriangle(P0, P1, P2, color)` fills the triangle like `DrawFilledTriangle`
  and scales the color at every pixel by an intensity interpolated from the vertices'
  `h`. The vertices are sorted as above, each `h` moving with its vertex. Alongside each
  x list the sides get an h list from the same calls over the same rows:
  `h01 = Interpolate(y0, h0, y1, h1)`, `h12 = Interpolate(y1, h1, y2, h2)`,
  `h02 = Interpolate(y0, h0, y2, h2)` and `h012 = h01 without its last value + h12`.
  The left and right sides are chosen on the x lists as above and the h lists follow
  them. Then for every row `y`, with `x_l = trunc(x_left[y - y0])` and
  `x_r = trunc(x_right[y - y0])`,
  `h_segment = Interpolate(x_l, h_left[y - y0], x_r, h_right[y - y0])` and the pixel
  `(x, y)` is set to `color * h_segment[x - x_l]` for every `x` from `x_l` to `x_r`.
  A row with `x_r < x_l` is skipped before that call: it is a row the filled triangle
  leaves empty (section 15), and it would violate `Interpolate`'s precondition. So a
  shaded triangle covers exactly the pixels of the filled triangle with the same
  vertices, and with `h = 1` at every vertex it is that filled triangle. Interpolating
  along the sides and then along every row is interpolating over the triangle's plane,
  what the book's Shading chapter does once the vertices carry lighting. The product is
  clamped only at the canvas (3.1); `h` is nominally in `[0, 1]` and is not checked. Two
  triangles that share an edge compute the same `h` on the edge's pixels, since both
  interpolate it from the same two vertices in the same order.
- There is no clipping. Pixels outside the canvas are dropped by `PutPixel`, so a line
  or a triangle may run off the canvas; it costs time in proportion to its full size.

### 3.7 Rasterizer: perspective projection

The rasterizer's 3D pipeline starts with the viewport of 3.2, the same one the ray
tracer looks through: the camera at the origin looking along +Z and the viewport
`width` x `height` at `distance` `d`.

- `ViewportToCanvas(viewport, x, y)` returns the canvas point of the viewport point
  `(x, y)`: `Cx = x * canvas_width / width`, `Cy = y * canvas_height / height`, each
  truncated to a pixel with `ToPixel`, as in the book's code. It is the inverse of
  `CanvasToViewport` up to that truncation: a viewport point lands on the pixel that
  looks through it.
- `ProjectVertex(viewport, v)` returns the canvas point where the vertex `v` appears.
  The ray from the origin through `v` crosses the viewport plane `z = d` at
  `Px = v.x * d / v.z`, `Py = v.y * d / v.z` (similar triangles), and that point goes
  through `ViewportToCanvas`. Vertices on one ray from the camera project to one pixel;
  a vertex twice as far from the camera lands at half the distance from the center;
  moving the viewport away (a larger `d`) zooms in. Precondition: `v.z > 0`, asserted.
- There is no clipping. A vertex outside the field of view projects beyond the canvas
  and the primitives draw what fits; a vertex very near the camera plane projects very
  far away, and a canvas coordinate beyond the range of `int` is undefined behaviour in
  `ToPixel`. The clipping step of the roadmap removes both.

### 3.8 Rasterizer: objects made of triangles

An object is a list of vertices, `Vec3`s in the camera's coordinates, and a list of
triangles, each naming three of the vertices by index and carrying a color. The
indirection stores a vertex once however many triangles meet at it (a cube is eight
vertices and twelve triangles, not thirty-six vertices) and projects it once.

- `Triangle` is `{v, color}`, with `v` an array of three indices into the object's
  vertex list.
- `RenderObject(viewport, vertices, triangles)` first projects every vertex with
  `ProjectVertex` (3.7) into a list `projected` parallel to `vertices`, then draws every
  triangle `T`, in list order, as
  `DrawWireframeTriangle(projected[T.v[0]], projected[T.v[1]], projected[T.v[2]], T.color)`:
  the book's `RenderObject` and `RenderTriangle`. Triangles that share a vertex agree on
  its pixel, and where two edges cover the same pixels the triangle drawn later shows.
  Preconditions: every index is less than the number of vertices, asserted; every vertex
  has `z > 0`, asserted by `ProjectVertex`.
- It is a wireframe: every edge is drawn, those at the back too. Hidden-surface removal
  (the depth buffer of the roadmap) and clipping come later; until then an object must
  lie entirely in front of the camera, and an edge of a closed surface, which belongs to
  two triangles, always shows the color of the one drawn later (section 15).

### 3.9 Rasterizer: models, instances and scenes

The object of 3.8 becomes a model, described once, and a scene places it any number of
times: the book's `model`, `instance` and `scene`, with a position only (no scale or
rotation until the transforms step).

- `Model` is `{vertices, triangles}`: the two lists `RenderObject` takes, in the model's
  own coordinates (model space), with the object's center at the origin by convention.
- `Instance` is `{model, position}`: `model` an index into the scene's model list, the
  way a `Triangle`'s corners are indices into the vertex list, so a model is stored once
  however many instances show it; `position` the point of the camera's coordinates where
  the model's origin lands.
- `RenderInstance(viewport, instance, models)` draws `models[instance.model]` with every
  vertex `V` moved to `V' = V + instance.position`, through `RenderObject` (3.8) with
  the model's triangles: the book's `RenderInstance`, whose projection loop and triangle
  loop are `RenderObject`'s. Since 3.10 the move is the instance's model matrix, which
  with the default orientation and scale adds exactly `position`. The model is
  unchanged. Preconditions: `instance.model < models.size()`, asserted; every moved
  vertex has `z > 0`, asserted by `ProjectVertex`.
- A scene is a model list and an instance list, the same shape as an object (a vertex
  list and a triangle list that indexes it) one level up; there is no scene type until a
  scene holds more than those two lists (section 14, decision 11).
  `RenderScene(viewport, models, instances, camera)` draws every instance in list order
  as the camera sees it (3.10); with the default camera the result is `RenderInstance`'s
  for each: the book's `RenderScene`. Where two instances overlap, the later one's edges
  show, as for the triangles within an object.
- Wireframes, no hidden-surface removal and no clipping, as in 3.8: every instance must
  stand entirely in front of the camera.

### 3.10 Rasterizer: model and camera transforms

An instance is placed by a scaling, a rotation and a translation, applied in that order,
and the camera stands anywhere, turned any way. Both are 4 x 4 matrices on homogeneous
coordinates and are composed into one matrix per instance; the perspective projection
stays outside the matrices (section 14, decision 12).

- `Mat4` is a 4 x 4 matrix of doubles, `m[row][col]`, all zeros by default;
  `Mat4::Identity()` changes nothing. `A * B` is the matrix product, the transform that
  applies `B` first and then `A`. `M * p`, for a `Vec3` p, applies `M` to the homogeneous
  point `(p.x, p.y, p.z, 1)` and returns `(x / w, y / w, z / w)`, the way back to
  Cartesian coordinates; for the affine matrices built here, whose bottom row is
  `(0, 0, 0, 1)`, `w` is exactly 1 and the division changes nothing. `Transposed(M)`
  exchanges rows and columns; for a rotation matrix it is the inverse.
- `MakeScalingMatrix(s)` is `diag(s, s, s, 1)`. `MakeTranslationMatrix(t)` is the
  identity with `(tx, ty, tz)` as its last column, so that
  `M (x, y, z, 1) = (x + tx, y + ty, z + tz, 1)`. `MakeOYRotationMatrix(degrees)` is the
  book's demo matrix `[[cos, 0, -sin, 0], [0, 1, 0, 0], [sin, 0, cos, 0], [0, 0, 0, 1]]`
  of the angle in radians: `x' = x cos - z sin`, `z' = x sin + z cos`, y unchanged, so a
  positive angle turns +X toward +Z and +Z toward -X. It is the one rotation the chapter
  needs; it calls `cos` and `sin`, the rasterizer's first `libm` calls (section 15).
- `Instance` gains `orientation`, a rotation matrix, the identity by default, and
  `scale`, 1 by default. `MakeModelMatrix(instance)` is
  `MakeTranslationMatrix(position) * orientation * MakeScalingMatrix(scale)`, the book's
  `M_model = I_T I_R I_S`: applied to a vertex it scales first, then rotates, then
  translates, the book's `ApplyTransform`, so the instance grows and turns about its own
  origin before it moves into the world. With the defaults it is the translation matrix
  and adds exactly `position` to a vertex, so 3.9 holds bit for bit.
- `Camera` is `{position, orientation}`, the identity orientation by default: the camera
  at the origin looking along +Z that every earlier section assumed, so a picture drawn
  without a camera is unchanged. `MakeCameraMatrix(position, orientation)` is
  `Transposed(orientation) * MakeTranslationMatrix(-position)`, the book's: a vertex is
  first moved by `-position`, which puts the camera at the origin, and then turned by
  the inverse of the camera's rotation, its transpose, which puts the camera's line of
  sight on +Z. Precondition: `orientation` is a rotation (orthonormal), not checked.
- `RenderModel(viewport, model, transform)` replaces every vertex `V` by `transform * V`
  and draws the result with the model's triangles through `RenderObject` (3.8), which
  projects each vertex with `ProjectVertex` and outlines each triangle: the book's
  `RenderModel`.
- `RenderInstance(viewport, instance, models)` is
  `RenderModel(models[instance.model], MakeModelMatrix(instance))`: the camera at the
  origin. `RenderScene(viewport, models, instances, camera = Camera{})` is the book's
  Listing 10-5: `M_camera = MakeCameraMatrix(camera.position, camera.orientation)` once,
  then for every instance `I` in list order
  `RenderModel(models[I.model], M_camera * MakeModelMatrix(I))`. One matrix per instance
  takes a vertex from the model's coordinates through the world into the camera's, and
  the projection follows vertex by vertex.
- The projection is not a matrix, by the book's own choice: "for practical reasons we
  won't be using the full projection matrix". The model and camera transforms are
  applied as matrices, their result is converted back to Cartesian coordinates, and
  `ProjectVertex` projects it as before, which leaves room for operations in 3D that are
  not matrix transforms, clipping first of all, between the transforms and the
  projection. The chapter's homogeneous projection matrix (a bottom row `(0, 0, 1, 0)`,
  which divides by z) is exercised by one unit test of `Mat4` and built nowhere else.
- No clipping yet: a camera that moves into the scene can leave vertices with `z <= 0`
  in its coordinates, which `ProjectVertex` asserts against; the demo keeps everything in
  front of the camera.

### 3.11 Numeric conventions

- All geometry and color math uses `double`. Switching to `float` would be a deliberate
  decision rather than a detail, because it changes the golden images.
- Colors are `double` in the nominal range 0..255 until they reach the canvas.
- There is no display. The canvas is saved to a file and viewed with any image viewer.

## 4. Architecture

### 4.1 Repository layout

```
CMakeLists.txt            top-level project: library, demo pictures, apps, tests
CMakePresets.json         dev / release configure, build and test presets
.clang-format
CLAUDE.md                 design principles and the folder-structure rules
README.md                 build instructions and project overview
docs/SPEC.md              this document
include/cgfs/             public headers: one folder per layer, one file per concept
    math/vec3.h  mat4.h
    image/color.h  canvas.h  image_io.h
    camera/viewport.h  camera.h
    raytracer/scene.h  raytracer.h
    rasterizer/point.h  interpolate.h  line.h  triangle.h  projection.h  object.h
    rasterizer/model.h  instance.h  scene.h
    demo/raytracer_scenes.h  rasterizer_scenes.h
                          the demo pictures shared by render_demo and the golden tests
src/<layer>/              implementation files, mirroring include/cgfs/<layer>/
apps/
    render_demo.cpp       command-line renderer for the demo pictures
tests/
    CMakeLists.txt
    support/test_helpers.h      Catch2 printers for Vec3/Mat4/Color/Rgb8/Point, a temp-file helper
    math/vec3_test.cpp  mat4_test.cpp
    image/color_test.cpp  canvas_test.cpp  image_io_test.cpp
    camera/viewport_test.cpp  camera_test.cpp
    raytracer/raytracer_test.cpp
    rasterizer/interpolate_test.cpp  line_test.cpp  triangle_test.cpp  projection_test.cpp
    rasterizer/object_test.cpp  model_test.cpp  instance_test.cpp  scene_test.cpp
    rasterizer/pixel_helpers.h
    demo/golden_test.cpp
    demo/golden/          reference renders of the demo pictures (PPM)
third_party/
    CMakeLists.txt
    stb/stb_image_write.h single-header PNG writer (public domain), vendored
    stb/stb_image_write.cpp  the one translation unit with the implementation
```

`build/` is the out-of-source build directory and is git-ignored. In-source builds are
rejected by the top-level `CMakeLists.txt`.

### 4.2 Layers and dependency order

Everything lives in namespace `cgfs` (the demo pictures in `cgfs::demo`); the folders,
not the namespaces, carry the structure. The layers, bottom to top, each depending only
on the ones below it:

```
math         vec3, mat4                                             vector algebra
image        color, canvas, image_io                                colors, the pixel buffer, file formats
camera       viewport, camera                                       how canvas pixels map into the world
raytracer    scene, raytracer                                       the ray tracing renderer
rasterizer   point, interpolate, line, triangle, projection,        the rasterizing renderer
             object, model, instance, scene
demo         raytracer_scenes, rasterizer_scenes                    the demo pictures
```

- `math/vec3`: `Vec3` and its free functions, header-only, `constexpr` where possible.
  `math/mat4`: `Mat4`, the 4 x 4 matrix of the transforms, its product, its application
  to a point and the scaling, translation and rotation constructors; header-only.
- `image/color`: `Color`, `Rgb8`, `ToRgb8`, header-only. `image/canvas`: the pixel buffer
  and the centered coordinate system. `image/image_io`: PPM writer and reader, PNG
  writer; used by `Canvas::Save` and the tests.
- `camera/viewport`: pixel to viewport-point mapping and field of view. Used by the ray
  tracer and by the rasterizer's projection. `camera/camera`: `Camera`, a position and
  an orientation, and `MakeCameraMatrix`; plain data for whichever renderer moves its
  camera, the rasterizer today.
- `raytracer/scene`: plain data: spheres, lights, background color.
  `raytracer/raytracer`: intersection, lighting, the render loop.
- `rasterizer/point`: `Point`, with its intensity `h`, and `ToPixel`, the one conversion
  from interpolated coordinates to pixels. `rasterizer/interpolate`: `Interpolate`, the
  building block. `rasterizer/line`: `DrawLine`. `rasterizer/triangle`:
  `DrawWireframeTriangle`, `DrawFilledTriangle` and `DrawShadedTriangle`.
  `rasterizer/projection`: `ViewportToCanvas` and `ProjectVertex`, from a `Vec3` seen
  through a `Viewport` to the `Point` the primitives draw; the start of the 3D pipeline.
  `rasterizer/object`: `Triangle`, three vertex indices and a color, and `RenderObject`,
  which projects an object's vertex list and outlines its triangles; the first use of
  the projection. `rasterizer/model`: `Model`, the vertex list and the triangle list of
  an object, stored once, and `RenderModel`, which draws a model under a transform
  through `RenderObject`. `rasterizer/instance`: `Instance`, a model index with a
  position, an orientation and a scale, `MakeModelMatrix`, its model matrix, and
  `RenderInstance`. `rasterizer/scene`: `RenderScene`, every instance of a model list
  and an instance list, in order, as a camera sees them.
- `demo/raytracer_scenes`: the ray-traced demo scenes, each a function returning a
  `Scene`. `demo/rasterizer_scenes`: the rasterized demo pictures, each a function that
  draws into a canvas. Either way the app and the golden tests render exactly the same
  thing.

The canvas knows nothing about 3D. The viewport knows nothing about spheres. The tracer
and the rasterizer write to the canvas only through `PutPixel`, and neither knows the
other exists. `CLAUDE.md` states the design principles and the rules that keep the tree
in this shape: a new layer is a new folder under `include/cgfs/`, `src/` and `tests/`
and a line in the table above.

### 4.3 Targets

| Target            | Kind               | Contents                                                                          |
|-------------------|--------------------|-----------------------------------------------------------------------------------|
| `cgfs`            | static library     | the renderer: every layer under `src/` and `include/cgfs/` except `demo`          |
| `cgfs::cgfs`      | alias              | the name the demo library links against                                           |
| `cgfs_demo`       | static library     | `src/demo/`, the demo pictures; links `cgfs::cgfs` publicly                       |
| `cgfs::demo`      | alias              | the name the app and the tests link against                                       |
| `stb_image_write` | static library     | the stb implementation, built without the project's warning flags; system include |
| `render_demo`     | executable         | `apps/render_demo.cpp`                                                            |
| `cgfs_tests`      | executable (CTest) | all `tests/<layer>/*_test.cpp`                                                    |

`BUILD_SHARED_LIBS` is respected but not used by default.

## 5. Public API

Signatures below are the intended shape. Names, parameter order and semantics are
normative; exact qualifiers and overload sets may be adjusted during implementation.

### 5.1 `math/vec3.h`, `math/mat4.h`

```cpp
struct Vec3 {
    double x = 0, y = 0, z = 0;
    // + - unary- , * and / by scalar (scalar on either side for *), compound forms
    bool operator==(const Vec3&) const = default;
};
constexpr double Dot(Vec3 a, Vec3 b);
constexpr Vec3   Cross(Vec3 a, Vec3 b);
double           Length(Vec3 v);          // sqrt(Dot(v, v))
Vec3             Normalized(Vec3 v);      // v / Length(v); precondition: Length(v) > 0
constexpr Vec3   Reflect(Vec3 v, Vec3 normal);   // 2 N <N, v> - v; precondition: |normal| = 1
```

```cpp
// mat4.h
struct Mat4 {                         // 4 x 4, homogeneous coordinates (section 3.10)
    double m[4][4]{};                 // m[row][col]; all zeros by default
    static constexpr Mat4 Identity();
    bool operator==(const Mat4&) const = default;
};
constexpr Mat4 operator*(const Mat4& a, const Mat4& b);   // applies b first, then a
constexpr Vec3 operator*(const Mat4& m, Vec3 p);          // m (p, 1), divided by w
constexpr Mat4 Transposed(const Mat4& m);                 // the inverse of a rotation
constexpr Mat4 MakeScalingMatrix(double scale);
constexpr Mat4 MakeTranslationMatrix(Vec3 translation);
Mat4           MakeOYRotationMatrix(double degrees);      // about Y; cos and sin
```

`Mat4` is plain data with free functions, like `Vec3`; the constructors carry the book's
demo names. There is no projection matrix (section 14, decision 12).

### 5.2 `image/color.h`

```cpp
struct Color {
    double r = 0, g = 0, b = 0;          // nominal range 0..255; may exceed it before clamping
    constexpr Color operator*(double s) const;   // scale by light intensity
    constexpr Color operator+(Color o) const;    // needed for reflections later
    bool operator==(const Color&) const = default;
};
struct Rgb8 { std::uint8_t r, g, b; };
Rgb8 ToRgb8(Color c);                     // clamp to [0, 255], then truncate (section 3.1)

namespace colors { inline constexpr Color kWhite{255, 255, 255}, kBlack{0, 0, 0},
                                         kRed{255, 0, 0}, kGreen{0, 255, 0}, kBlue{0, 0, 255}; }
```

### 5.3 `image/canvas.h`

```cpp
class Canvas {
public:
    explicit Canvas(int width = 1920, int height = 1080, Color background = colors::kWhite);
    // throws std::invalid_argument when width <= 0 or height <= 0

    int   width() const;
    int   height() const;
    Color background() const;

    void Clear();                                   // fill with the current background
    void Clear(Color color);                        // set the background, then fill
    void PutPixel(int x, int y, Color color);       // centered coords; out of range ignored
    std::optional<Rgb8> GetPixel(int x, int y) const;   // nullopt when out of range

    std::span<const std::uint8_t> Data() const;     // row-major RGB8, top row first, no padding
    void Save(const std::filesystem::path& path) const;
    // ".ppm" writes binary P6, ".png" writes PNG. Any other extension or an I/O
    // failure throws std::runtime_error.
};
```

### 5.4 `image/image_io.h`

```cpp
struct Image { int width = 0, height = 0; std::vector<std::uint8_t> rgb; };
void  WritePpm(const std::filesystem::path&, int width, int height, std::span<const std::uint8_t> rgb);
void  WritePng(const std::filesystem::path&, int width, int height, std::span<const std::uint8_t> rgb);
Image ReadPpm(const std::filesystem::path&);   // binary P6 with maxval 255; used by golden tests
```

### 5.5 `camera/viewport.h`, `camera/camera.h`

```cpp
class Viewport {
public:
    Viewport(int canvas_width, int canvas_height,
             double width = 1.0, double height = 1.0, double distance = 1.0);
    Viewport(const Canvas& canvas,
             double width = 1.0, double height = 1.0, double distance = 1.0);
    // width = height * canvas_width / canvas_height, so the image is not stretched
    static Viewport MatchingCanvasAspect(const Canvas& canvas,
                                         double height = 1.0, double distance = 1.0);

    Vec3 CanvasToViewport(int x, int y) const;

    struct FieldOfView { double horizontal_degrees, vertical_degrees; };
    FieldOfView Fov() const;

    int canvas_width() const;  int canvas_height() const;
    double width() const;  double height() const;  double distance() const;
};
```

The viewport stores the canvas dimensions by value. It never holds a reference to a
canvas, so there is no lifetime coupling between the two.

```cpp
// camera.h
struct Camera {                               // where the eye is and which way it looks (section 3.10)
    Vec3 position;
    Mat4 orientation = Mat4::Identity();      // a rotation: the camera's axes in the world's
};
Mat4 MakeCameraMatrix(Vec3 position, const Mat4& orientation);  // Transposed(orientation) * T(-position)
```

The default camera is the origin looking along +Z, the camera of every earlier chapter.
The viewport is not part of it: the camera says where the eye is, the viewport what
window it looks through.

### 5.6 `raytracer/scene.h`

```cpp
inline constexpr double kMatte = -1.0;    // the specular value of a surface with no highlight
struct Sphere {
    Vec3 center; double radius = 1.0; Color color;
    double specular = kMatte;     // shininess exponent
    double reflective = 0.0;      // 0: no reflection .. 1: a perfect mirror
};

struct AmbientLight     { double intensity = 0; };
struct PointLight       { double intensity = 0; Vec3 position; };
struct DirectionalLight { double intensity = 0; Vec3 direction; };   // points toward the light
using Light = std::variant<AmbientLight, PointLight, DirectionalLight>;

struct Scene {
    std::vector<Sphere> spheres;
    std::vector<Light>  lights;
    Color background_color = colors::kWhite;
};
```

`std::variant` instead of a class hierarchy: lights are a small closed set, value
semantics keep scenes copyable and trivially testable, and `std::visit` dispatches on
the light type. `Sphere` grew its `specular` and `reflective` fields one chapter at a
time; aggregate initialization with designated initializers and default member values
kept the existing scene code compiling each time.

### 5.7 `raytracer/raytracer.h`

```cpp
inline constexpr double kInfinity = std::numeric_limits<double>::infinity();

struct SphereIntersection { double t1 = kInfinity, t2 = kInfinity; };
SphereIntersection IntersectRaySphere(Vec3 origin, Vec3 direction, const Sphere& sphere);

struct Hit { const Sphere* sphere = nullptr; double t = kInfinity; };   // null sphere: a miss
inline constexpr double kSurfaceEpsilon = 0.001;  // where a ray cast from a surface point starts
inline constexpr int kReflectionDepth = 3;        // bounces per camera ray

class RayTracer {
public:
    RayTracer(const Scene& scene, const Viewport& viewport);   // stores references
    void   Render(Canvas& canvas) const;
    // precondition: canvas dimensions equal the viewport's; violation throws std::invalid_argument
    Hit    ClosestIntersection(Vec3 origin, Vec3 direction, double t_min, double t_max) const;
    Color  TraceRay(Vec3 origin, Vec3 direction, double t_min, double t_max,
                    int recursion_depth = kReflectionDepth) const;
    double ComputeLighting(Vec3 point, Vec3 normal, Vec3 view, double specular) const;
    // normal must be unit length; view points from the point toward the viewer;
    // specular is the exponent s, or kMatte for no highlight
};
```

The scene and viewport must outlive the tracer. `Hit::sphere` points into the scene's
sphere vector and is valid as long as that vector is not modified. `ClosestIntersection`,
`TraceRay` and `ComputeLighting` are public so the tests can exercise them directly.

### 5.8 `rasterizer/point.h`, `interpolate.h`, `line.h`, `triangle.h`, `projection.h`, `object.h`, `model.h`, `instance.h`, `scene.h`

```cpp
// point.h
struct Point {                 // canvas coordinates and the vertex's intensity (section 3.6)
    int x = 0;
    int y = 0;
    double h = 1.0;            // multiplier for the color; read by DrawShadedTriangle only
    bool operator==(const Point&) const = default;
};
constexpr int ToPixel(double coordinate);    // truncation toward zero (section 3.6)

// interpolate.h
std::vector<double> Interpolate(int i0, double d0, int i1, double d1);
// i1 - i0 + 1 values of d at i0..i1; {d0} when i0 == i1; precondition i0 <= i1 (section 3.6)

// line.h
void DrawLine(Canvas& canvas, Point p0, Point p1, Color color);

// triangle.h
void DrawWireframeTriangle(Canvas& canvas, Point p0, Point p1, Point p2, Color color);
void DrawFilledTriangle(Canvas& canvas, Point p0, Point p1, Point p2, Color color);
void DrawShadedTriangle(Canvas& canvas, Point p0, Point p1, Point p2, Color color);  // color * h

// projection.h
Point ViewportToCanvas(const Viewport& viewport, double x, double y);  // truncated (section 3.7)
Point ProjectVertex(const Viewport& viewport, Vec3 v);                 // precondition v.z > 0

// object.h
struct Triangle {                     // one face of an object (section 3.8)
    std::array<std::size_t, 3> v{};   // indices into the object's vertex list
    Color color;
};
void RenderObject(Canvas& canvas, const Viewport& viewport, std::span<const Vec3> vertices,
                  std::span<const Triangle> triangles);  // wireframe; every index < vertices.size()

// model.h
struct Model {                        // an object's shape, described once (section 3.9)
    std::vector<Vec3> vertices;       // in the model's own coordinates
    std::vector<Triangle> triangles;
};
void RenderModel(Canvas& canvas, const Viewport& viewport, const Model& model,
                 const Mat4& transform);  // RenderObject on transform * every vertex (section 3.10)

// instance.h
struct Instance {                     // one appearance of a model in the scene (section 3.9)
    std::size_t model = 0;            // index into the scene's model list
    Vec3 position;                    // where the model's origin lands
    Mat4 orientation = Mat4::Identity();  // a rotation about that origin (section 3.10)
    double scale = 1.0;               // a factor about that origin
};
Mat4 MakeModelMatrix(const Instance& instance);  // T(position) * orientation * S(scale)
void RenderInstance(Canvas& canvas, const Viewport& viewport, const Instance& instance,
                    std::span<const Model> models);  // RenderModel with the model matrix

// scene.h
void RenderScene(Canvas& canvas, const Viewport& viewport, std::span<const Model> models,
                 std::span<const Instance> instances,
                 const Camera& camera = Camera{});  // every instance, in list order, as camera sees it
```

One header per primitive. Free functions that take the canvas as their first parameter,
the shape of the book's listings minus their global canvas; the projection takes the
viewport the same way, in place of the book's global viewport size and plane distance.
`RenderObject` takes the vertex and triangle lists as spans, the book's two parameters,
so a `std::vector`, a `std::array` or a plain array serves; `RenderScene` takes the model
and instance lists the same way, and `RenderInstance` takes the model list to resolve the
instance's index, as `RenderTriangle` takes the projected list inside `RenderObject`. The
camera and the new `Instance` fields arrived as a defaulted parameter and defaulted
fields, so every call and every aggregate initializer written before them still compiles
and draws the same picture.
They are the primitives; a class with state (a depth buffer, a camera) comes when the
pipeline needs one, and the primitives stay as they are.

## 6. Build system

CMake is the only supported build system.

- `cmake_minimum_required(VERSION 3.25)`: `FetchContent`'s `FIND_PACKAGE_ARGS` needs
  3.24 and its `SYSTEM` keyword 3.25. Ninja is the recommended generator; Unix Makefiles
  must also work.
- `project(cgfs VERSION 0.1.0 LANGUAGES CXX)`.
- C++20, requested per target with `target_compile_features(... cxx_std_20)`;
  `CMAKE_CXX_EXTENSIONS` is `OFF`. The code uses `std::span`, designated initializers,
  defaulted comparisons and `std::numbers`.
- Warnings are set per target, never through `CMAKE_CXX_FLAGS`:
  GCC and Clang get `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow
  -Wno-missing-field-initializers`; MSVC gets `/W4`. The one disabled warning fires on
  designated initializers that rely on default member values, which is how scenes are
  written. The vendored stb header is included as a system header so it does not trip
  these warnings.
- `CMAKE_EXPORT_COMPILE_COMMANDS` is `ON` so editors and clang-tidy see the flags.
- Single-config generators default to `Release` when no build type is given.
- Options, all prefixed `CGFS_`:

  | Option                      | Default | Effect                                        |
  |-----------------------------|---------|-----------------------------------------------|
  | `CGFS_BUILD_TESTS`          | ON      | build `cgfs_tests` and register it with CTest |
  | `CGFS_BUILD_APPS`           | ON      | build `render_demo`                           |
  | `CGFS_WARNINGS_AS_ERRORS`   | ON      | add `-Werror` / `/WX`                         |
  | `CGFS_ENABLE_SANITIZERS`    | OFF     | add `-fsanitize=address,undefined` (GCC/Clang) |

- `CMakePresets.json` (version 6) defines:
  - `dev`: Ninja, Debug, sanitizers on, tests on, build dir `build/dev`.
  - `release`: Ninja, Release, sanitizers off, tests on, build dir `build/release`.
  - matching build presets and test presets (`ctest --preset dev` runs with
    `--output-on-failure`).
- The test framework is Catch2 v3, obtained with `FetchContent` and
  `FIND_PACKAGE_ARGS` so a system-installed Catch2 is used when present. Tests are
  registered with `catch_discover_tests`. The fetch happens only when
  `CGFS_BUILD_TESTS` is on, so a library-only configure needs no network.
- No install or packaging rules yet.

A fresh clone must build and pass with:

```
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Toolchain baseline: GCC 13 on Linux, which is what the project is developed on. Clang 16
or newer is expected to work and should be tried before each milestone is closed.

## 7. Dependencies

| Dependency             | Role        | How it is obtained                             | Required                         |
|------------------------|-------------|------------------------------------------------|----------------------------------|
| C++20 standard library | everything  | toolchain                                      | yes                              |
| `stb_image_write.h`    | PNG output  | vendored in `third_party/stb`, public domain   | yes (build only, single header)  |
| Catch2 v3              | tests       | `FetchContent`, or `find_package` if installed | only with `CGFS_BUILD_TESTS`     |

Anything else needs a line in this table and a reason. PPM output has no dependency at
all, so the renderer stays usable even if `stb` is dropped.

## 8. Testing

All tests live in `tests/`, in a tree that mirrors `include/cgfs/` layer by layer, and
run through CTest. They must pass in both presets.

Unit tests:

- `vec3`: arithmetic, `Dot`, `Cross`, `Length`, `Normalized`, `Reflect` (a 45-degree
  vector mirrors to the other side, the normal maps to itself, a grazing vector reverses).
- `mat4`: all zeros by default, and `Identity` changes neither matrices nor points; the
  product against a hand-computed 4 x 4 example, and `A * B != B * A`; `A * B` applies
  `B` first (a translation after a scaling against a scaling after a translation); a
  point comes back from homogeneous coordinates divided by `w` (a bottom row
  `(0, 0, 0, 2)` halves it, `(0, 0, 1, 0)` divides by z); `Transposed` exchanges rows
  and columns and undoes itself; `MakeScalingMatrix` scales about the origin and
  `MakeScalingMatrix(1)` is the identity; `MakeTranslationMatrix` moves every point by
  the vector in its last column and `MakeTranslationMatrix(0)` is the identity;
  `MakeOYRotationMatrix` turns +X to +Z and +Z to -X at 90 degrees within `1e-12`,
  leaves y exactly, is exactly the identity at 0 degrees, matches cos and sin by hand at
  30 degrees, composes (30 + 60 = 90), keeps lengths and inverts by transposition.
- `color`: scaling, addition, `ToRgb8` clamping and truncation (e.g. `254.9 -> 254`,
  `300 -> 255`, `-5 -> 0`).
- `canvas`: default dimensions; `Clear` with and without a color; `PutPixel` at the
  origin and the four corners lands on the expected buffer rows and columns; writes
  outside the range are ignored; odd dimensions follow the integer-division rule;
  `GetPixel` round-trips `PutPixel`.
- `image_io`: PPM write then read round-trips the bytes; PNG write produces a file that
  starts with the PNG signature.
- `viewport`: a table of points for a 1920 x 1080 canvas and a 1 x 1 x 1 viewport
  (center, top-right `(0.5, 0.5, 1)`, bottom-left, quarter); the aspect-matched
  viewport maps the top-right corner to `(16/18, 0.5, 1)`; `Fov()` of the default
  viewport is 53.13 degrees on both axes within 0.01.
- `camera`: the default camera is at the origin with the identity orientation and its
  matrix is exactly the identity; a moved camera maps its own position to the origin and
  a point 5 ahead to `(0, 0, 5)`; a camera turned 90 degrees looks along -X and maps
  `(-5, 0, 0)` to `(0, 0, 5)`, what was ahead to its right and up to up; for the book's
  demo camera, `(-3, 1, 2)` turned -30 degrees, the points `position + orientation * v`
  for `v` on the camera's axes map to `v`, and the matrix equals the transpose times the
  translation.
- `raytracer`:
  - `IntersectRaySphere`: miss gives two infinite roots; hit gives two finite roots in
    the right order; tangent gives equal roots; origin inside the sphere gives one
    negative root.
  - `ClosestIntersection`: the closest sphere in `[t_min, t_max]` wins regardless of
    scene order; a `t_min` inside a sphere hits its far side; a miss (ray passes by,
    sphere behind the ray, range stops short or starts past it, empty scene) has a null
    sphere and `t = +inf`; both range bounds are inclusive.
  - `ComputeLighting`: ambient only; a point light straight above a point lit at full
    intensity; a light behind the surface contributes nothing; the result does not
    depend on the length of `L`. Specular: a matte surface ignores the view direction;
    the full light intensity is added when `V` is the mirror direction; the highlight
    falls off as `cos^s` (checked for s = 2 and s = 10 at 45 degrees); nothing is added
    when `<R, V> <= 0`; the result does not depend on the lengths of `L` and `V`; point
    lights produce highlights too. Shadows: a sphere between the point and a point light
    removes the diffuse and the specular term (also when it touches the light or
    contains the point); a sphere beyond the light, behind the point or off to the side
    does not; a directional light is blocked at any distance but only ahead of the
    point; each light is shadowed independently and ambient light never is; the sphere
    the point lies on does not shadow it (straight up and at a grazing 60 degrees), while
    the same sphere does shadow a point that is not on it; a blocker whose far side ends
    just short of `kShadowEpsilon` along `L` is skipped and one just past it counts.
  - `TraceRay`: a single sphere with full ambient light returns its color on the axis
    and the background color for a ray that misses; a shiny sphere lit from the camera
    gets the diffuse term plus the full highlight, with the sphere's own exponent; a
    sphere between the hit point and a point light leaves only the ambient light, and
    the same sphere beyond the light changes nothing. Reflections (ambient light only,
    so every color is exact): a sphere with `reflective = 0` ignores the depth; a
    reflective sphere hit head-on mixes the background in behind it in proportion to
    `r` (0.5, 0.25 and 1, a perfect mirror), and not at all at depth 0; a 45-degree hit
    bounces by the mirror law and shows a sphere placed on the bounced ray; two facing
    half-mirrors give the exact colors for depths 0 to 3 (and a negative depth never
    bounces), which also proves a bounce does not hit the sphere it leaves.
  - `Render`: the center pixel of the two-mirror scene carries the depth-3 color, so
    camera rays bounce `kReflectionDepth = 3` times.
- `rasterizer`:
  - `Interpolate`: a single point gives `{d0}`; one value per integer step with both
    ends included; fractional slopes; a decreasing `d`; equal end values give a
    constant; a negative independent variable; with a slope of 1/3 every value is within
    `1e-12` of `d0 + k * a`.
  - `DrawLine`: horizontal, vertical, diagonal and zero-length lines light exactly the
    expected pixels; a horizontal-ish line of slope 1/2 and a vertical-ish one of
    slope 2 light the pixels the truncation rule predicts, and their mirror images
    through the origin pin truncation toward zero rather than flooring; both endpoint
    orders give identical canvases for seven lines covering both branches and both swap
    cases; a horizontal-ish line has exactly one pixel in every column and a vertical-ish
    one in every row, with no gaps; lines that leave the canvas or lie entirely outside
    it draw what fits and nothing else; pixels take the line's color after the canvas's
    clamping.
  - `DrawFilledTriangle`: a right triangle fills exactly its rows; all six vertex orders
    give the same canvas; the long side may be on the left or on the right; the short
    sides are joined at the middle vertex and fractional x truncates (a pixel-exact
    expectation for a 7-row triangle); flat bottom and flat top; collinear vertices
    give a line; the two triangles of a rectangle cut along its diagonal cover the
    rectangle exactly and overlap on the diagonal's pixels only, which the later one
    owns; a triangle larger than the canvas fills it and one outside it draws nothing;
    the color is clamped. `DrawWireframeTriangle` draws the same pixels as the three
    `DrawLine` calls.
  - `DrawShadedTriangle`: with `h = 1` at every vertex it gives the filled triangle's
    canvas for eleven shapes, among them the two whose rows the filled triangle skips
    (the two-row case and the straddled apex row of section 15); a constant `h` of 0.5
    gives 127 everywhere; on the right triangle with the vertical side x = 0, `h = 0, 1,
    0` at its vertices gives `255 * x / 4` at every pixel and `h = 0, 0, 1` gives
    `255 * y / 4` (every value a multiple of 1/4, so exact to the byte); all six vertex
    orders give the same canvas and each intensity stays with its vertex; the two
    triangles of a rectangle cut along its diagonal give identical bytes on the
    diagonal's three pixels; `h = 2` is clamped to 255 by the canvas and a negative `h`
    to 0.
  - `ViewportToCanvas`: the viewport test's table the other way round (the default
    viewport on 1920 x 1080); a 2 x 1 viewport scales each axis by its own ratio;
    truncation toward zero (`33.7 -> 33`, `-33.7 -> -33`, `0.5` and `-0.5` to 0).
  - `ProjectVertex`: a vertex on the viewport plane keeps its x and y; a vertex twice
    and four times as far lands at a half and a quarter of the distance from the
    center, and one nearer than the viewport beyond the canvas; four vertices on one
    ray project to one pixel; the viewport at distance 2 sees `(0.5, 0.5, 4)` where
    the one at distance 1 sees `(0.5, 0.5, 2)`; on a 64 x 32 canvas with a 2 x 1
    viewport at distance 2 (powers of two, so every step is exact) `ProjectVertex`
    inverts `CanvasToViewport` on every pixel.
  - `RenderObject` (on a 32 x 32 canvas through a 1 x 1 viewport at distance 1, where a
    vertex `(x, y, 1)` lands on the pixel `(32 x, 32 y)`): one triangle gives the canvas
    of `DrawWireframeTriangle` on its projected corners; a triangle names its corners by
    index in any order, and a vertex no triangle uses leaves no pixel; the square cut
    along its diagonal, two triangles sharing two vertices, gives the canvas of the two
    wireframes and lights the outline plus the diagonal, 79 pixels; every triangle keeps
    its color and the one drawn later owns the shared edge, in both orders; a triangle
    twice as far and twice as large gives the same picture, and twice as far without the
    scaling half the size; the lists may be a `std::array` and a plain array; an object
    without triangles, or without vertices, draws nothing.
  - `RenderModel` (same canvas and viewport; a lopsided triangle in its own plane
    z = 0): a translation to z = 1 gives the wireframe of its corners and the canvas of
    `RenderObject` on the vertices moved by hand; the transform is applied before the
    projection (twice as far is half the size; scaled by 2 and twice as far is the same
    picture); a half turn about Y mirrors the model in x, exactly, since cos 180 is -1
    and the model has z = 0; a quarter turn shows it edge-on, 14 pixels on the column
    x = 0 from y = -7 to 6; a product of two translations draws as their sum; an empty
    model draws nothing.
  - `RenderInstance` (same canvas and viewport; the model's triangle lies in its own
    plane z = 0 and is placed at z = 1): at `(0, 0, 1)` it gives the wireframe of its
    corners and the canvas of `RenderObject` on the vertices moved by hand; a position
    along x or y shifts the picture by the projected amount and one along z shrinks it
    (twice as far, half the size; all three at once); an instance names its model by
    index, so the second of two models is drawn in its own color and the first leaves no
    pixel; the model is unchanged by drawing it and the same model drawn again at
    another position adds the second outline; a model without triangles, or without
    vertices, draws nothing. `MakeModelMatrix` with the defaults is exactly the
    translation matrix; with scale 2, a quarter turn and a translation it maps
    `(1, 0, 0)` to `(1, 2, 5)` (scaled, turned, then moved) and the origin to the
    translation, and equals `T * R * S`; the scale shrinks the model about its own
    origin (half the size in place; twice the size twice as far is the same picture)
    and the orientation turns it there (a half turn mirrors a lopsided model in x).
  - `RenderScene`: three instances of one model, placed apart, give the three outlines,
    the canvas of three `RenderInstance` calls and as many pixels as the three drawn
    alone; two models of one shape in two colors at the same position show the color of
    the instance listed later, in both orders; instances may use any model in any order;
    a scene without instances, with or without models, draws nothing; the lists may be a
    `std::array` and a plain array. Without a camera the scene is drawn as with
    `Camera{}`; moving the camera moves the picture the opposite way, exactly (left by
    0.25 on the viewport plane puts the instance 8 pixels right, rising puts it lower,
    stepping back halves it); a camera turned 90 degrees to its left sees an instance at
    `(-1, 0, 0)`, turned to face it, as the default camera sees it at `(0, 0, 1)`; the
    book's demo camera, moved and turned, sees an instance turned the same way and
    placed 1 ahead along its line of sight exactly as the default camera sees the
    untransformed instance at `(0, 0, 1)`. The turned-camera tests use a triangle whose
    corners are 0.14 from its center, off the pixel boundary that 0.125 sits on, so that
    cos and sin, exact only to a few ulps, cannot move a pixel (section 15).

Golden-image tests:

- `tests/demo/golden/` holds 320 x 180 renders of the five ray-traced demo scenes (flat ambient, the
  diffuse scene with the yellow floor sphere, the specular scene with the same geometry
  and lights but shiny surfaces, the reflective scene with the same again plus a
  reflectivity per sphere, and the mirror ball scene: fourteen spheres of mixed finishes
  under two point lights, which exercises every feature at once), made with the
  aspect-matched viewport and stored as binary PPM. They are the reference output of the
  renderer and are committed. The diffuse, specular, reflective and mirror ball
  references show the shadows the spheres cast on the floor and on each other; the flat
  reference has no shadows because ambient light is never blocked; only the reflective
  and mirror ball references show reflections. `lines_320x180.ppm`,
  `triangles_320x180.ppm`, `shaded_320x180.ppm`, `cube_320x180.ppm`,
  `objects_320x180.ppm`, `instances_320x180.ppm` and `camera_320x180.ppm` are the
  rasterizer's demos (section 9): every pixel of them comes from `DrawLine`,
  `DrawFilledTriangle` or `DrawShadedTriangle`, the objects' through `RenderObject`, the
  instances' and the camera scene's through `RenderScene`. The 2D scenes compute their
  vertices with integer arithmetic and their colors and intensities without any `libm`
  call, and the projections of the cube, the objects and the instances are a few IEEE
  additions, multiplications and divisions, which every platform rounds the same way,
  so those pictures are reproducible bit for bit. The camera scene is the first whose
  vertices pass through `cos` and `sin`; section 15 records the margin by which its
  projected vertices clear the pixel boundaries, so that a `libm` off by a few ulps
  still draws the same pixels.
- `golden_test.cpp` renders the same scenes and compares every channel of every pixel
  with the reference image. The test passes when no channel differs by more than 1. The
  tolerance absorbs floating-point differences between compilers and standard
  libraries; everything else is exact.
- The scenes are defined once, in the `demo` layer, shared by `render_demo` and the
  golden test, so the app and the test cannot drift apart.
- The references are regenerated only when rendering behaviour changes on purpose, with
  `render_demo --scene <flat|diffuse|specular|reflective|mirrorball|lines|triangles|shaded|cube|objects|instances|camera> --width 320 --height 180 --output tests/demo/golden/...`
  from a Release build, and the pictures are reviewed before they are committed. The
  `flat` reference doubles as a regression test: neither the specular nor the shadow code
  path may change the output of an ambient-only scene, `diffuse` pins the shadows of
  matte surfaces independently of the highlights, and `specular` pins the output of
  non-reflective surfaces under the recursive `TraceRay`.

## 9. Demo application

`render_demo` renders one of the demo pictures to a file:

```
render_demo [--scene reflective|specular|diffuse|flat|mirrorball|lines|triangles|shaded|cube|objects|instances|camera|axes] [--width N] [--height N] [--distance D] [--output PATH]
```

Defaults: `reflective`, 1920 x 1080, distance 1, `render.png`. The `mirrorball` scene is
the project's own composition, built from the same features as the book's scene: a chrome
ball ringed by six colored spheres of different finishes on a dark polished floor,
marbles in front, spheres floating above, two point lights for crossing shadows and a
dusk-blue sky. The `lines` scene is the rasterizer's: `DrawLinesScene` clears the canvas
to a dark background and draws a dim pair of axes, string-art envelopes in the four
corners (lines whose ends slide along the two edges and together outline a parabola) and
a sunburst of lines from the center to a rectangle around it, colored once around the hue
wheel; 110 lines in all, every slope and both endpoint orders among them. The `triangles`
scene is the rasterizer's second: `DrawTrianglesScene` draws a big triangle in every
corner, each filled and then outlined in a brighter shade the way the book's demo does it
(a flat top, a vertical edge, a flat bottom and a vertex beyond the canvas among them),
and over them a color wheel of 24 filled wedges from the origin to the rim of a
rectangle, which share their edges and tile without gaps. The `shaded` scene is the
rasterizer's third: `DrawShadedTrianglesScene` draws on the left a green triangle in the
proportions of the book's demo (its vertices scaled from the book's 600 x 600 canvas to
the canvas height) with the intensities 0.3, 0.1 and 1 at its vertices, and on the right
a glowing color wheel of 24 shaded wedges from a bright center (`h = 1`) to the dim rim
(`h = 0.2`) of a rectangle, which share their edges and shade continuously across them.
The `cube` scene is the rasterizer's first 3D picture and the book's: `DrawCubeScene`
projects the eight corners of the cube at x = -2..-1, y = -0.5..0.5, z = 5..6 with
`ProjectVertex` and draws its twelve edges with `DrawLine`, the front face blue, the back
face red and the edges from front to back green. The `objects` scene is the first made
of objects: `DrawObjectsScene` draws with `RenderObject` the book's cube, eight corners
at x, y, z = ±1 moved to x -= 1.5, z += 7 and twelve triangles in six colors, two per
face, and to its right a square pyramid of five vertices and six triangles, a white base
of two triangles and four sides in four more colors; every vertex is projected once and
every triangle outlined, so the faces show their diagonals, nothing is hidden, and an
edge shared by two triangles shows the color of the one drawn later. The `instances`
scene is the first drawn as a scene of instances: `DrawInstancesScene` stores the same
cube and pyramid once each, as models, and places them sixteen times with `RenderScene`.
A building is a stack of cubes with a pyramid for a roof, standing on the ground 1.5
below the camera: a tower of three floors at x = 0, z = 20, and a house of one floor on
each side of the street, at x = -3.5 and 3.5 and z = 14, 10 and 6, listed far to near so
that the near edges show where buildings overlap. The houses shrink toward the tower,
which is perspective, and the roofs' white base edges outline every eave, since the roof
is drawn after the cube under it. The `camera` scene is the chapter's closing picture:
`DrawCameraScene` stores the cube once and places two instances with a model transform
each, the first scaled to 0.75 at (-1.5, 0, 7) and the second turned 195 degrees about Y
at (1.25, 2.5, 7.5), the instances and the camera of the book's demo, and draws them with
`RenderScene` as seen from a camera at (-3, 1, 2) turned -30 degrees about Y, that is 30
degrees to its right, toward the cubes; every vertex goes through one matrix, the camera
matrix times the instance's model matrix, and then through `ProjectVertex` as before. The
`axes` scene draws the coordinate axes and corner dots; it exercises the canvas without
ray tracing. For the ray-traced scenes, the cube, the objects, the instances and the
camera scene the viewport width always matches the canvas aspect ratio, and `--distance`
zooms them; it has no effect on the 2D pictures. The output format follows
the extension. The program prints the render time in milliseconds and exits non-zero with
a message on bad arguments or I/O failure. Argument parsing is hand-written; no library
is added for it. The scene table in `render_demo.cpp` is the single list of pictures: the
usage text, the default and the dispatch are derived from it, so a new picture is a new
row.

## 10. Code style and conventions

- Naming: types `PascalCase`; functions and methods `PascalCase` (this matches the
  textbook's listings: `PutPixel`, `TraceRay`); variables and parameters `snake_case`;
  private data members `snake_case_`; constants `kPascalCase`; trivial accessors
  `width()`-style lowercase.
- Formatting is enforced by `.clang-format`: `BasedOnStyle: Google`, `ColumnLimit: 100`,
  `IndentWidth: 4`. No formatting debates in review; run the tool.
- Headers use `#pragma once` and live under `include/cgfs/<layer>/`; one concept per
  header. Headers include what they use and nothing more. The design principles and the
  folder rules are in `CLAUDE.md`.
- `[[nodiscard]]` on every function whose only effect is its return value.
- `constexpr` for `Vec3` and `Color` operations.
- Pass `Vec3` and `Color` by value; pass `Scene`, `Canvas` and `Viewport` by reference.
- No raw `new`/`delete`, no owning raw pointers. The renderer does not need heap
  allocation outside `std::vector`.
- Doc comments in headers explain the geometry, not just the signature. The project is
  a learning project; the headers should read like a tutorial.

## 11. Error handling

- Constructor preconditions (canvas size, viewport dimensions and distance must be
  positive) throw `std::invalid_argument`.
- File I/O failures throw `std::runtime_error` with the path in the message.
- The ray tracer's render loop, intersection and lighting code never throw and never
  allocate. The rasterizer's `Interpolate` returns a `std::vector`, as in the book: one
  allocation per line, a few per triangle and one per row of a shaded triangle; the
  primitives can only throw if an allocation fails.
- Programming errors inside the library (a non-unit normal, a dimension mismatch that
  slipped past the constructor check) are `assert`ed in Debug builds.

## 12. Performance

Targets apply to Release builds on a single core. Debug builds have no targets.

| Scene                               | Size        | Target                                           |
|-------------------------------------|-------------|--------------------------------------------------|
| reflective demo (4 spheres, 3 lights, 3 bounces) | 1920 x 1080 | under 1 second; around 250 ms is the expectation |
| reflective demo                                  | 320 x 180   | under 50 ms, so the golden test stays cheap      |
| lines demo (110 lines)                           | 1920 x 1080 | under 10 ms; about 2 ms measured                 |
| triangles demo (28 filled, 4 outlined)           | 1920 x 1080 | under 20 ms; about 7 ms measured                 |
| shaded demo (1 triangle, 24 shaded wedges)       | 1920 x 1080 | under 20 ms; about 7 ms measured                 |
| cube demo (12 projected lines)                   | 1920 x 1080 | under 10 ms; under 1 ms measured                 |
| instances demo (16 instances, 150 triangles)     | 1920 x 1080 | under 10 ms; about 2 ms measured                 |
| camera demo (2 transformed instances, 24 triangles)| 1920 x 1080 | under 10 ms; 1 to 2 ms measured                  |

The render loop iterates rows in the outer loop so the canvas is written sequentially.
Rows are independent, which keeps the door open for a row-parallel `Render` later; no
threading yet. Every hit point casts one shadow ray per point or directional light, each
of which tests every sphere, so lighting costs about as much as the camera ray itself.
Reflections multiply that by up to four (the camera ray and three bounces) on every
pixel that shows a reflective surface; in the demo scene almost every pixel does.

## 13. Roadmap

The project follows the chapters of *Computer Graphics from Scratch*
(https://gabrielgambetta.com/computer-graphics-from-scratch/). The architecture in
section 4 is meant to absorb these steps as additions:

- Arbitrary camera for the ray tracer: `Mat4` and `Camera` exist now (`math/mat4.h`,
  `camera/camera.h`, built for the rasterizer); `Render` would start its rays at
  `camera.position` and turn `D` by `camera.orientation`.
- Row-parallel rendering with `std::thread` or a parallel algorithm, with golden tests
  proving the output is unchanged.
- Rasterizer, so far `rasterizer/` with `Point`, `Interpolate`, `DrawLine`,
  `DrawWireframeTriangle`, `DrawFilledTriangle`, `DrawShadedTriangle`, the perspective
  projection `ProjectVertex`, `RenderObject`, the wireframe of an object given as a
  vertex list and an indexed triangle list, `Model`, `Instance`, `RenderInstance` and
  `RenderScene`, a scene of models placed by position, and the transforms of the
  chapter's second half: `Mat4`, the model matrix of an instance (scale, rotation about
  Y, translation), `Camera` and the camera matrix, composed into one matrix per
  instance by `RenderScene` and applied by `RenderModel`. Next, in the book's order:
  clipping (in 3D, between the transforms and the projection, which is why the
  projection is not a matrix), the depth buffer, shading (which feeds each vertex's
  lighting into `Point::h`) and textures. Rotations about X and Z are added the way
  `MakeOYRotationMatrix` is, when a scene needs them.
- Optional interactive window behind a `CGFS_ENABLE_WINDOW` option (SDL2 or similar),
  off by default so the dependency policy in section 7 holds for everyone else.

None of these should require changing a public signature from section 5 other than by
adding parameters with defaults or fields with defaults.

## 14. Design decisions

The judgment calls behind this design, so they are not relitigated by accident.

1. Colors use the 0..255 convention of the textbook, stored as `double`, clamped and
   truncated at the canvas. A 0..1 convention would be cleaner but would break the
   line-by-line correspondence with the book's listings.
2. `double` everywhere. `float` would be the usual choice for a software renderer, but
   the cost is irrelevant at this scale and `double` keeps the golden images stable
   across compilers.
3. Lights are a `std::variant`, not a class hierarchy.
4. PNG output through the vendored single-header `stb_image_write`; PPM is the
   dependency-free fallback and the golden-image format.
5. Catch2 v3 via `FetchContent` for tests. doctest or GoogleTest would work equally; the
   choice only affects `tests/`.
6. Google-based clang-format with 4-space indentation and 100 columns; `PascalCase`
   methods to match the book.
7. No window or live preview. Save a file and open it.
8. Pixel coordinates are truncated toward zero, like colors (decision 1), because that
   is what the book's code does: an interpolated `-0.5` and `0.5` both select pixel 0,
   so the staircase of a line mirrors at the axes. Rounding to nearest would sample at
   pixel centers and flooring would make the staircase translation-invariant; either
   would change every rasterized golden image, so switching is a deliberate decision,
   not a tidy-up.
9. The vertex intensity `h` is a field of `Point`, with the default 1, as in the book's
   listings, rather than extra parameters of `DrawShadedTriangle`. The sort that orders
   the vertices by y then carries each intensity with its point, every `Point{x, y}`
   written so far keeps meaning "the full color", and the later chapters attach more
   per-vertex data the same way. `DrawLine` and `DrawFilledTriangle` ignore it.
10. The projection lives in the rasterizer layer, as free functions that take the
    `Viewport`, rather than as methods of `Viewport`: its result is a `Point`, a
    rasterizer type, truncated with the rasterizer's `ToPixel`, and the camera layer
    stays below the rasterizer. `Viewport` keeps the exact mapping in one direction,
    `CanvasToViewport`; the lossy one lives next to its user.
11. An `Instance` names its model by index into the scene's model list, not by pointer
    and not by copy, for the reason a `Triangle` names its corners by index: the model
    is stored once, the scene is plain copyable data with no lifetime to manage, and the
    lists stay the book's lists. The cost is that `RenderInstance` and `RenderScene`
    take the model list to resolve the index, as `RenderTriangle` takes the projected
    list. There is no rasterizer scene type yet: the ray tracer's `Scene` owns that name
    in the flat namespace, and a scene that is only a model list and an instance list is
    passed as the two lists, exactly as an object is passed to `RenderObject`. The
    camera (3.10) joined as a defaulted parameter of `RenderScene`, which keeps every
    earlier call compiling; a scene type is still a step away, for when clipping planes
    or lights make the bundle worth naming.
12. The projection is not a matrix. The chapter derives the homogeneous projection and
    viewport matrices and then sets them aside "for practical reasons", and the project
    does the same: `Mat4` carries only the affine model and camera transforms, and
    `ProjectVertex` stays the one place where a vertex becomes a pixel. Clipping, which
    must happen in 3D between the transforms and the projection, is the practical
    reason. `M * p` still divides by w, so the arithmetic of a projective matrix is
    there and tested; it is only not used by the pipeline. Three smaller calls go with
    it: `Mat4` is a plain `double m[4][4]` with free functions, like `Vec3`, so a matrix
    in the code reads like the book's listing; an `Instance` keeps its scale,
    orientation and position as data and `MakeModelMatrix` rebuilds the matrix on every
    render, two products per instance, rather than caching it, so the struct has one
    source of truth; and rotation exists only about Y, the one the book's demos use.

## 15. Implementation notes

Measurements and known behaviours of the implementation, recorded 2026-11-02, updated
2026-11-03 with shadows and reflections, 2026-11-04 with the rasterizer's lines and
filled triangles and the reorganisation into layers, 2026-11-05 with shaded triangles,
the perspective projection and objects made of triangles, and 2026-11-09 with models,
instances and scenes and, the same day, with model and camera transforms.

- Catch2 is pinned to v3.16.1.
- The golden images are 320 x 180. The ±1 tolerance in the test has never been needed on
  the development machine; it stays as insurance against a different standard library.
- Measured on the development machine, Release, single thread: the 1920 x 1080 diffuse
  scene renders in about 85 ms, the specular scene in about 105 ms and the reflective
  scene in about 200 ms; the 320 x 180 golden scenes in under 10 ms. Before shadows the
  diffuse and specular figures were 80 ms and 140 ms: the shadow rays cost little, and a
  shadowed light skips its `pow`, which is why the specular scene got faster. The three
  bounces of the reflective scene roughly double the specular time rather than
  quadrupling it, because most bounces leave the scene after one or two hits. The mirror
  ball scene (14 spheres, 4 lights) takes about 360 ms at 1920 x 1080 and 10 ms at
  320 x 180.
- Shadow and reflected rays start at `kSurfaceEpsilon = 0.001` along their direction,
  the book's value. A shadow ray toward a point light `|L|` units away is offset by
  `0.001 |L|` in world units, so the offset scales with the distance to the light; a
  reflected ray's direction has the length of the camera ray's. No shadow acne or
  self-reflection has been seen on the demo scenes.
- The book's reflections chapter prints its scene with the blue sphere at `(-2, 0, 4)`
  and the green one at `(2, 0, 4)`, the reverse of every earlier chapter; the book's
  own demo code keeps green at `x = -2` and blue at `x = +2`. `ReflectiveScene` follows
  the demo code (and the project's other scenes) and applies the reflectivities by color:
  red 0.2, blue 0.3, green 0.4, floor 0.5.
- Odd canvas sizes: `Render` leaves the last column and row untouched (the loops cover
  `-width/2 <= x < width/2`, `-height/2 <= y < height/2`), and the pixel mapping in 3.1
  puts y = 0 on the top row for odd heights. Both are tested so a change is deliberate.
- `Interpolate` accumulates the slope by repeated addition, as the book's listing does,
  so a value that should be an exact integer can come out a few ulps below it and
  truncate one pixel short: `Interpolate(0, 0, 10, 1)` ends at `0.9999999999999999`,
  and the line from `(0, 0)` to `(10, 1)` therefore ends at `(10, 0)`. The book's own
  demos behave the same way. Computing `d0 + k * a` instead of the running sum would hit
  integer endpoints exactly for integer inputs; it is not adopted, to keep the listing,
  and nothing in the lines demo is visibly affected. IEEE arithmetic makes the result the
  same on every compiler, so the golden image does not depend on it.
- The lines demo draws its 110 lines in about 2 ms at 1920 x 1080 and in well under a
  millisecond at 320 x 180. Each line allocates one vector.
- `DrawFilledTriangle` decides which side is left by comparing the two x lists at the
  middle row, as the book does, which is reliable only where the sides are apart. For
  a triangle two rows tall whose middle vertex shares the lower row (`y1 == y0`,
  `y2 == y0 + 1`) the middle index is the top row, where both lists hold `x2`; the
  comparison then says nothing, and the lower row is filled only when `x1 <= x0` and
  skipped otherwise. Triangles three or more rows tall are not affected, and a one-row
  triangle is drawn as the segment between its first two vertices after the sort, the
  third being ignored. An exact rule is the sign of the integer cross product
  `(x2 - x0) * (y1 - y0) - (y2 - y0) * (x1 - x0)`; it is not adopted, so the code keeps
  the book's shape. Neither case occurs in the demo.
- The running sums can also leave the apex row of a triangle empty. Both x lists end at
  `x2` in exact arithmetic, but as running sums they can land on either side of it, and
  when the left one truncates above the right one (`-4.999999999999997` against `-5.0`,
  say) the row has no pixels and the apex pixel is not drawn. A simulation over every
  triangle with vertices in `[-6, 6]^2` finds such a row in about one triangle in eight:
  always the apex row for a proper triangle, the apex or the middle vertex's row when
  the vertices are collinear. The book's code behaves the same. `DrawFilledTriangle`'s
  inner loop then runs zero times; `DrawShadedTriangle` skips such a row explicitly,
  because `Interpolate(x_l, ..., x_r, ...)` with `x_l > x_r` would violate the
  precondition, an assertion failure in Debug builds. The unit tests draw both skipping
  cases through both functions. Ending every list on the exact `d1` would remove the
  apex case; it is not adopted, to keep the book's listing, and no demo is visibly
  affected.
- An outline drawn over a filled triangle (the corner triangles of the demo) can stick
  out of the fill by one pixel along horizontal-ish edges, where `DrawLine` interpolates
  y over x while the fill interpolates x over y; along vertical-ish edges the two
  coincide exactly.
- The triangles demo (28 filled triangles, four of them outlined, covering most of the
  canvas) draws in about 7 ms at 1920 x 1080. Each triangle allocates three vectors and
  one more for the concatenation; the fill itself goes through `PutPixel` pixel by pixel.
- The shaded demo (one shaded triangle and 24 shaded wedges, covering about half of the
  canvas) draws in about 7 ms at 1920 x 1080. Each shaded triangle allocates six vectors
  for its sides, two for the concatenations and one per row for `h_segment`, the book's
  shape; at this size the per-row allocation costs nothing measurable.
- `ViewportToCanvas` truncates, so `ProjectVertex(CanvasToViewport(x, y))` is `(x, y)`
  only when the arithmetic is exact. On the demo sizes with the aspect-matched viewport
  it is not: `y = -63` on a 320 x 180 canvas comes back as `-62.99999999999999`, which
  truncates toward zero to `-62`, and about one pixel in seven moves one pixel toward
  the center this way (one in three at distance 3). The book's demo truncates the same
  way (`| 0`). The unit test of the round trip therefore uses power-of-two sizes, where
  every product and quotient is exact, and the cube demo pins its own picture.
- The cube demo (twelve projected lines) draws in well under a millisecond at
  1920 x 1080.
- The objects demo (two `RenderObject` calls, 18 triangles, 54 lines) draws in 1 to 2 ms
  at 1920 x 1080. `RenderObject` allocates one vector for the projected vertices; every
  edge of a closed surface is drawn twice, once per triangle that has it, which is the
  book's shape and costs nothing visible.
- In a wireframe an edge that two triangles share shows the color of the one drawn
  later, and on a closed surface every edge is shared, so the first triangle drawn can
  vanish entirely: a tetrahedron shows three of its four colors whatever the order. The
  objects demo therefore pairs the book's cube, whose faces keep their diagonals in
  their own color, with a square pyramid, where every triangle keeps an edge of its own.
- The instances demo (16 instances of 2 models, 9 cubes and 7 pyramids, 150 triangles
  and 450 lines) draws in about 2 ms at 1920 x 1080. Each instance allocates one vector
  for its moved vertices and `RenderObject` one more for the projected ones; the two
  models are built once per picture.
- Factoring the cube and the pyramid of the objects demo into `Model`s shared with the
  instances demo changed no output: `objects_320x180.ppm` is byte for byte the same.
- In the instances demo a roof's base and the top of the cube under it share their four
  edges and cross their diagonals: the pyramid's base is cut from front-left to
  back-right, the cube's top from front-right to back-left, so every roof shows a white
  X over a purple one. Nothing hides in a wireframe; the depth buffer will settle it.
- The camera demo (2 instances, 24 triangles, 72 lines) draws in 1 to 2 ms at
  1920 x 1080; the matrices cost nothing measurable next to the lines. The instances
  demo, now drawn through the same matrices with the default camera, is byte for byte
  the picture it was: with the identity orientation and scale 1 the model matrix is
  exactly the translation matrix, the default camera's matrix is exactly the identity,
  and `M * p` then evaluates `1 * x + 0 * y + 0 * z + tx`, which IEEE arithmetic makes
  exactly `x + tx`, before dividing by a `w` of exactly 1.
- `MakeOYRotationMatrix` is the rasterizer's first use of `cos` and `sin`, so the camera
  reference image depends on `libm`. A replica of the pipeline in Python shows that at
  320 x 180 no projected coordinate of the scene's sixteen vertices comes closer than
  0.066 pixels to a pixel boundary, and the nearest vertex is 4.06 in front of the
  camera: a `cos` or `sin` off by a few ulps moves a coordinate by about 1e-14 pixels
  and cannot change the picture. A new camera scene should be checked the same way, or
  simply rendered on the other platform, before its reference is trusted there.
- Tests of turned cameras use a triangle whose corners are 0.14 from its center rather
  than 0.125: 0.14 * 32 = 4.48 truncates to the same pixel 4 but is not a pixel
  boundary, which the rotation's rounding errors, a few ulps, would otherwise cross:
  through a camera turned 90 degrees a corner at 0.125 comes out as `0.125 - 6e-17`,
  which truncates to pixel 3.
- On 2026-11-04 the tree was reorganised into the layers of 4.2 (`CLAUDE.md` has the
  rules): the rasterizer was split into one file per primitive, the demo pictures moved
  into their own library target with the `axes` picture joining them from the app, the
  rasterizer tests were split to match, and `render_demo` became table-driven. The
  refactor changed no output: all eight pictures at 320 x 180 and four of them at
  321 x 181, a zoomed render, the usage text and two error messages were compared byte
  for byte before and after.
- The `.clang-format` file is in place and clang-format 18 is now installed on the
  development machine, but the sources were formatted by hand to its rules and
  `clang-format --dry-run` still reports a handful of differences in older files. A
  reformat is a separate, mechanical change; new code follows the surrounding style.
