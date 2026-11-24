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
projection of vertices, objects made of triangles, drawn as wireframes, filled or
shaded, and scenes of instances of models, each with a model transform, seen from a
movable camera, clipped in 3D to what that camera can see and, where asked, drawn
through a depth buffer that removes the hidden surfaces and lit by the same lights and
the same equation as the ray tracer's spheres, with flat, Gouraud or Phong shading;
section 13 lists what comes next.

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
- That mapping is one function, `PixelIndex(width, height, x, y)`: the index
  `row * width + column` of the pixel in a buffer stored row by row, or nothing when
  the pixel is outside. The canvas keeps its colors by it and the depth buffer (3.12)
  its depths, so the two agree on where every pixel is and on which pixels exist.
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

The lights and the illumination equation are a layer of their own, `lighting/`, below
both renderers (section 14, decision 17): the ray tracer sums the terms of the equation
here, with shadows, and the rasterizer uses the sum without them (3.13).

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

The two terms of one light are `DiffuseReflection(I, N, L)` and
`SpecularReflection(I, N, L, V, s)` of `lighting/illumination.h`, exactly the two lines
of the sum above, each returning 0 where its line is skipped for the sign of its dot
product. `ComputeLighting` adds them for every light that is not blocked, the specular
one only when `s` is not `kMatte`. `ComputeIllumination(P, N, V, s, lights)` of the same
header is that sum with no light blocked, the book's `ComputeIllumination`: what the
rasterizer, which casts no shadows, computes (3.13).

### 3.6 Rasterizer: lines and triangles

The rasterizer draws 2D primitives straight into the canvas with `PutPixel`; nothing of
the ray tracer is involved. A `Point` is a canvas position in the centered integer
coordinates of 3.1 together with an intensity `h`, a multiplier for the color that only
the shaded triangle reads; it defaults to 1, the full color. It also carries a depth,
`inv_z`, which the filled and the shaded triangle read when they are given a depth
buffer (3.12) and which nothing reads otherwise, and a `normal`, which only the Phong
triangle reads (3.13).

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
- The 2D primitives do not clip. Pixels outside the canvas are dropped by `PutPixel`, so
  a line or a triangle may run off the canvas; it costs time in proportion to its full
  size. Clipping is done in 3D, on the scene, before anything reaches them (3.11).

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
  moving the viewport away (a larger `d`) zooms in. The point also carries
  `inv_z = 1 / v.z`, all that is left of the vertex's depth once it is a pixel, for
  the depth buffer (3.12); `ViewportToCanvas`, which is given no depth, leaves it at
  0. Precondition: `v.z > 0`, asserted.
- `UnprojectVertex(viewport, x, y, inv_z)` is the way back, from a pixel and the `1/z`
  kept for it to the point of the scene it shows; Phong shading needs it and 3.13
  defines it.
- The projection does not clip. A vertex outside the field of view projects beyond the
  canvas and the primitives draw what fits; a vertex very near the camera plane projects
  very far away, and a canvas coordinate beyond the range of `int` is undefined behaviour
  in `ToPixel`. Clipping the scene first (3.11) removes both: what it leaves has
  `z >= d` and projects onto the canvas or the row or column next to it.

### 3.8 Rasterizer: objects made of triangles

An object is a list of vertices, `Vec3`s in the camera's coordinates, and a list of
triangles, each naming three of the vertices by index and carrying a color. The
indirection stores a vertex once however many triangles meet at it (a cube is eight
vertices and twelve triangles, not thirty-six vertices) and projects it once.

- `Triangle` is `{v, color, h, normals}`, with `v` an array of three indices into the
  object's vertex list and `h` an array of three intensities, `h[k]` the intensity of
  the triangle at its corner `v[k]`: the `h` of 3.6, a multiplier for the color. It is 1
  at every corner by default, the full color. The intensity belongs to the corner of the
  triangle, not to the vertex, so two triangles that meet at a vertex may be differently
  bright there, as two faces of a cube are (section 14, decision 14). `normals` is an
  array of three normals, one per corner in the same way, none by default; only the
  drawing under lights reads them (3.13).
- `RenderObject(viewport, vertices, triangles, draw_triangle = DrawWireframeTriangle,
  depth_buffer = none)`
  first projects every vertex with `ProjectVertex` (3.7) into a list `projected`
  parallel to `vertices`, then draws every triangle `T`, in list order, as
  `draw_triangle(P0, P1, P2, T.color, depth_buffer)` with `Pk = projected[T.v[k]]` given
  the intensity `T.h[k]`: the book's `RenderObject` and `RenderTriangle`. `draw_triangle` is one of the
  three triangle primitives of 3.6, which share the signature `DrawTriangleFunction`:
  `DrawWireframeTriangle`, the default and the outline the book draws at this point,
  `DrawFilledTriangle` or `DrawShadedTriangle`; the first two ignore the intensities.
  Triangles that share a vertex agree on its pixel, and where two triangles cover the
  same pixels the one drawn later shows, unless there is a depth buffer. Preconditions: every index is less than the
  number of vertices, asserted; every vertex has `z > 0`, asserted by `ProjectVertex`.
- Without a depth buffer there is no hidden-surface removal. A wireframe
  shows every edge, those at the back too, and an edge of a closed surface, which
  belongs to two triangles, always shows the color of the one drawn later (section 15).
  Filled or shaded triangles come out right only when the list runs from the back of the
  object to its front as the camera sees it; for a convex solid seen from outside it is
  enough that every triangle turned away from the camera comes before every triangle
  turned toward it. With a depth buffer (3.12) the filled and the shaded primitive set
  a pixel only where their triangle is the nearest thing so far, and the order of the
  list stops mattering. `RenderObject` clips nothing; `ClipScene` (3.11) does that before
  an object arrives here.

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
  as the camera sees it (3.10), without its back faces (3.12) and clipped to the camera's
  clipping planes (3.11); with the default camera and the culling off the result is
  `RenderInstance`'s for each: the book's `RenderScene`.
  Where two instances overlap, the later one shows, as for the triangles within an
  object, unless the scene is drawn through a depth buffer (3.12).
- `RenderInstance` and `RenderModel` clip nothing, cull nothing, take no depth buffer
  and outline their triangles: what they draw must stand entirely in front of the camera.

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
- `TransformModel(model, transform)` is the model under a transform, as a model of its
  own: every vertex `V` replaced by `transform * V`, the same triangles with the normals
  at their corners turned along (3.13), and the bounding sphere moved along (3.11). `RenderModel(viewport, model, transform)` draws
  the vertices of `TransformModel(model, transform)` with the model's triangles through
  `RenderObject` (3.8), which projects each vertex with `ProjectVertex` and outlines
  each triangle: the book's `RenderModel`.
- `RenderInstance(viewport, instance, models)` is
  `RenderModel(models[instance.model], MakeModelMatrix(instance))`: the camera at the
  origin. `RenderScene(viewport, models, instances, camera = Camera{},
  draw_triangle = DrawWireframeTriangle, depth_buffer = none,
  back_face_culling = BackFaceCulling::kOn)` is the book's Listing 10-5 with the
  clipping and culling steps of the next two chapters in it:
  `M_camera = MakeCameraMatrix(camera.position, camera.orientation)` once; then every
  instance `I` becomes `TransformModel(models[I.model], M_camera * MakeModelMatrix(I))`,
  geometry in the camera's coordinates; then, unless the culling is off, every instance
  loses its back faces, `CullBackFaces(instance, origin)` (3.12); then that list is
  clipped, `ClipScene(scene, camera.clipping_planes)` (3.11); then every instance that
  is left is drawn, in list order, with
  `RenderObject(vertices, triangles, draw_triangle, depth_buffer)`. One
  matrix per instance takes a vertex from the model's coordinates through the world into
  the camera's, the culling and the clipping work on the result in 3D, and the
  projection follows vertex by vertex. A camera has no clipping planes unless it is
  given some, and without them the clipping changes nothing. A second `RenderScene`
  takes lights and a shading model in place of the triangle primitive and lights what
  it draws (3.13).
- The projection is not a matrix, by the book's own choice: "for practical reasons we
  won't be using the full projection matrix". The model and camera transforms are
  applied as matrices, their result is converted back to Cartesian coordinates, and
  `ProjectVertex` projects it as before, which leaves room for operations in 3D that are
  not matrix transforms, clipping first of all, between the transforms and the
  projection. The chapter's homogeneous projection matrix (a bottom row `(0, 0, 1, 0)`,
  which divides by z) is exercised by one unit test of `Mat4` and built nowhere else.
- A camera that moves into the scene leaves vertices with `z <= 0` in its coordinates,
  which `ProjectVertex` asserts against. A camera with the clipping planes of its
  viewport (3.11) may stand anywhere; one without them, as in the `camera` demo, must
  keep everything in front of it.

### 3.11 Rasterizer: clipping

The clipping volume is the part of space the camera can draw. Clipping computes the
intersection of the scene with it: a 3D operation that takes the geometry of the scene
and returns new geometry. It therefore happens after the scene has been placed, on
vertices that have been through the model and camera transforms, and before the
perspective projection, which cannot take a vertex behind the camera at all. The volume
is bounded by planes, so clipping against it is clipping against one plane after
another.

Planes and bounding spheres:

- `Plane` is `{normal, d}`: the points `P` with `<N, P> + D = 0`, `normal` a unit vector
  and `d` the book's `D`, minus the distance from the origin to the plane along `N`. The
  side the normal points to is in front of the plane; clipping keeps what is in front,
  so a clipping plane's normal points into the volume.
- `SignedDistance(plane, vertex)` is `<N, vertex> + D`: zero on the plane, positive in
  front of it, negative behind, and for a unit normal the distance itself.
- `IntersectSegmentPlane(A, B, plane)` is `t = (-D - <N, A>) / <N, B - A>`, the book's:
  the fraction of the way from `A` to `B` at which the segment crosses the plane, at
  `Q = A + t (B - A)`. The same `t` gives any attribute that is linear along the
  segment, `a + t (b - a)`. Precondition: `<N, B - A> != 0`.
- `BoundingSphere` is `{center, radius}`, a sphere that contains an object. Its default
  radius is infinite: the sphere that contains everything, always a correct bound and
  one that never saves work. `MakeBoundingSphere(points)` is the book's approximation:
  the center is the average of the points and the radius the distance from it to the
  farthest one. It contains every point, is exact for a shape symmetric about its center
  (a box) and not always the smallest otherwise; for no points it is the origin with
  radius 0.
- `Model` has a `bounding_sphere` around its vertices, in their coordinates, the default
  one unless a model is given another. `TransformModel(model, transform)` (3.10) moves
  it with the vertices: the center is transformed like a vertex and the radius is
  multiplied by the factor by which `transform` scales lengths, the book's
  `radius * scale`. The factor is the length of the matrix's first column
  `(m00, m10, m20)`, where the unit vector along X ends up; for the transforms of this
  pipeline, a rotation, a uniform scaling and a translation, every direction is scaled
  by it. Precondition: `transform` scales uniformly.

The clipping volume:

- `Camera` has `clipping_planes`, a list of planes in the camera's own coordinates (the
  eye at the origin looking along +Z), normals pointing into the volume, as in the
  book's demo. It is empty by default, which clips nothing.
- `MakeClippingPlanes(viewport)` returns the five planes of what a viewport `w` x `h` at
  distance `d` shows on a canvas `cw` x `ch`, in this order:

  ```
  near    N = (0, 0, 1)                    D = -d
  left    N = ( d, 0, W) / |( d, 0, W)|    D = 0
  right   N = (-d, 0, W) / |(-d, 0, W)|    D = 0
  bottom  N = (0,  d, H) / |(0,  d, H)|    D = 0
  top     N = (0, -d, H) / |(0, -d, H)|    D = 0
  ```

  with `W = (w / 2) (1 + 1 / cw)` and `H = (h / 2) (1 + 1 / ch)`. The near plane is the
  viewport's plane `z = d`. Each of the other four passes through the eye and through
  one side of a rectangle on that plane: the left one holds the points with
  `x / z = -W / d`, that is `d x + W z = 0`. With `W = H = d`, a field of view of 90
  degrees, the normals are the book's `(±1/sqrt(2), 0, 1/sqrt(2))` and
  `(0, ±1/sqrt(2), 1/sqrt(2))`. There is no far plane.
- The rectangle is the viewport's and half a pixel more on every side (section 14,
  decision 13), because of the truncation in `ViewportToCanvas` (3.7). The canvas shows
  more than the viewport: its leftmost column collects what projects up to one pixel to
  the left of the viewport's left side and its bottom row what projects up to one pixel
  below it, which a volume cut exactly at the viewport's sides would leave unpainted.
  And a triangle cut by a side plane gets corners that project exactly onto the plane's
  line on the canvas: at the viewport's own sides that line is the border between two
  columns or rows, where rounding would decide the pixel, and half a pixel further out
  it is the middle of one. A clipping volume may be larger than what is shown; what it
  lets through beyond the canvas, `PutPixel` drops.

Clipping, from the triangle up. An "instance" in these functions is the book's at this
stage of the pipeline: not an `Instance`, a model index with a transform, but the
geometry it stands for, a `Model` whose vertices and bounding sphere are in the camera's
coordinates, as `TransformModel` returns it; a scene is a list of those.

- `ClipTriangle(triangle, plane, vertices, clipped_triangles)` appends to
  `clipped_triangles` what the plane leaves of one triangle, whose indices refer to
  `vertices`. With `d0`, `d1`, `d2` the signed distances of its corners:
  - none negative: the triangle itself;
  - none positive: nothing;
  - exactly one positive, the corner `A`: the triangle `A B' C'`, with `B'` where the
    side `AB` crosses the plane and `C'` where `AC` does;
  - exactly two positive, `A` and `B`, and `C` behind: the two triangles `A B A'` and
    `A' B B'`, with `A'` on `AC` and `B'` on `BC`.

  These are the book's four cases with the corners on the plane assigned: a distance of
  0 is neither positive nor negative, so a triangle that only touches the plane is kept
  or dropped whole, and one cut through a corner gives one triangle rather than a
  triangle and a sliver without area. `A`, `B` and `C` are the triangle's corners
  rotated, never swapped, so every piece keeps the triangle's winding. A crossing point
  is `A + t (B - A)` with `t = IntersectSegmentPlane(A, B, plane)`, always computed from
  the corner in front toward the corner behind, so two triangles that share the cut side
  compute the same point bit for bit; it is appended to `vertices` and named by index.
  The new corner's intensity is `hA + t (hB - hA)`, its normal `NA + t (NB - NA)`, no
  normal where neither corner had one and not normalized (3.13), and the color is the
  triangle's.
  The book's `ClipTriangle` returns the list; this one appends to it, as the book's own
  demo code does.
- `ClipTrianglesAgainstPlane(triangles, plane, vertices)` returns the concatenation of
  `ClipTriangle` over the list, in order; the new vertices follow the old ones in
  `vertices`.
- `ClipInstanceAgainstPlane(instance, plane)`, with `d` the signed distance of the
  bounding sphere's center and `r` its radius: when `d > r` the instance is in front of
  the plane and is returned as it is; when `d < -r` it is behind and nothing is returned
  (`std::nullopt`); otherwise it is returned with its triangles replaced by
  `ClipTrianglesAgainstPlane` of them. The sphere is trusted: the first two cases do not
  look at the triangles. In the third the vertices that no triangle names any more, the
  corners that were cut off, are removed and the triangles renumbered, so that every
  vertex of the result is in front of the plane or on it, to within the rounding of the
  crossing points; the bounding sphere is kept, since what is left is inside it. An
  instance whose sphere reached across the plane while its triangles did not is returned
  with no triangles. Precondition: `|plane.normal| = 1`.
- `ClipInstance(instance, planes)` clips against every plane in turn, each working on
  what the one before left, and returns nothing as soon as one plane leaves nothing.
- `ClipScene(scene, planes)` returns the instances of `scene` clipped by `ClipInstance`,
  in order, without those of which nothing is left. With no planes it is the scene.
- `RenderScene` (3.10) calls `ClipScene` between the transforms and the projection.
  With the planes of `MakeClippingPlanes(viewport)` every vertex that reaches
  `ProjectVertex` has `z >= d`, to within the rounding of a crossing point, and so
  `z > 0`, whatever the scene and wherever the camera stands, and it projects onto the
  canvas or the row or column next to it.
- Drawn as a wireframe a clipped triangle is the outline of its pieces: the cut shows as
  an edge along the plane, and the diagonal between the two pieces of a quadrilateral
  shows too. Drawn shaded, the pieces have the intensities of the whole triangle at
  their corners, taken along the sides in 3D; inside a piece `DrawShadedTriangle`
  interpolates along the canvas as before (section 15).

### 3.12 Rasterizer: hidden-surface removal

Triangles are drawn one after another, and a pixel that several of them cover shows the
one drawn last (3.8). A depth buffer makes it show the nearest one instead, whatever the
order: for every pixel of the canvas it remembers how near the nearest thing drawn there
so far is, and a triangle sets a pixel only where it is nearer than that. This is the
book's depth buffering, the first half of its chapter. The second half, back-face
culling, removes the triangles that are turned away from the camera before they are
drawn at all, and follows the depth buffer below.

- The buffer stores `1/z`, not `z`, because `1/z` can be interpolated over the picture
  of a triangle and `z` cannot. A triangle lies in a plane `A X + B Y + C Z + D = 0` of
  the camera's coordinates. The projection (3.7) shows the point `(X, Y, Z)` at
  `x = X d / Z`, `y = Y d / Z` on the viewport, so `X = x Z / d` and `Y = y Z / d`;
  substituting these in the plane's equation and dividing by `Z D` gives

  ```
  1/Z = -(A x + B y + C d) / (D d)
  ```

  which is linear in `x` and `y`, and so in the canvas coordinates: the same amount is
  added to `1/Z` with every step along a row or along a side, which is exactly what
  `Interpolate` (3.6) computes. `Z` itself is the reciprocal of that linear function and
  does not change by equal steps. Halfway along the picture of an edge that runs from
  `z = 1` to `z = 4` the depth is `1.6`, the reciprocal of the mean of `1` and `1/4`,
  and not `2.5`; interpolating `z` would put the edge behind a surface at `z = 2` that
  it is in front of. (`D = 0` is a plane through the camera, seen edge-on: its picture
  is a line.)
- `1/z` orders the depths in reverse: in front of the camera `z > 0`, so `1/z > 0`, and
  the nearer of two points has the larger `1/z`. As `z` grows without bound `1/z` falls
  to `0`, so `0` stands for "infinitely far" and is what an empty pixel holds; no
  special value is needed for "nothing drawn yet".
- A `Point` (3.6) carries `inv_z`, the `1/z` of the vertex it is the projection of.
  `ProjectVertex` (3.7) sets it. It defaults to `0`, infinitely far: a point made in 2D
  has no depth, and nothing reads the field unless there is a depth buffer.
- `DepthBuffer(width, height)` holds one `double` per pixel of a canvas of that size,
  addressed by the canvas's centered coordinates (3.1), all `0` at first.
  `UpdateIfCloser(x, y, inv_z)` is the book's `UpdateDepthBufferIfCloser`: when
  `(x, y)` is on the canvas and `inv_z` is greater than the stored value it stores
  `inv_z` and returns true; otherwise it changes nothing and returns false. The
  comparison is strict, so of two surfaces that are equally deep at a pixel the one
  drawn first stays, and a depth of `0` or less is never stored. `InvZ(x, y)` reads the
  stored value. The buffer and the canvas agree on which coordinates are on the canvas
  because both get the answer from one function, `PixelIndex` (3.1).
- `DrawFilledTriangle` and `DrawShadedTriangle` (3.6) take a depth buffer as an optional
  last parameter. The pixels they consider are exactly those of 3.6. Alongside each x
  list the sides get a list of `1/z` from the same calls over the same rows, as for `h`:
  `iz01 = Interpolate(y0, inv_z0, y1, inv_z1)`, `iz12`, `iz02` likewise and
  `iz012 = iz01 without its last value + iz12`, and the lists follow their x lists to
  the left and to the right. In every row
  `iz_segment = Interpolate(x_l, iz_left[y - y0], x_r, iz_right[y - y0])`, and the pixel
  `(x, y)` is set only if `UpdateIfCloser(x, y, iz_segment[x - x_l])` says so. Without a
  depth buffer every pixel is set, as before, and the rows' depths are not computed:
  the 2D pictures and every scene drawn without one are byte for byte what they were,
  and take as long as they did.
  `DrawWireframeTriangle` takes the parameter too, since the three share a signature,
  and ignores it: an outline neither hides anything nor is hidden.
- `RenderObject` (3.8) and `RenderScene` (3.9) take the depth buffer as an optional last
  parameter and hand it to the primitive with every triangle. Things hide each other
  only through a buffer they share, so the caller makes one buffer of the canvas's size
  for the picture, as it makes the canvas, and passes it to every call that draws into
  that picture. With it the order of the instances, and of the triangles in a model, no
  longer decides what is seen.
- What depth buffering leaves undecided is a tie. Two faces that share an edge both draw
  the edge's pixels (3.6), from the same two vertices and so at the same depth, give or
  take the rounding of the running sums; such a pixel shows one face or the other, and
  which one is decided by the last bit. Between two faces that are both visible this
  cannot be told from a correct picture. Where one of the two is turned away from the
  camera its picture lies inside the other's, and the shared edge can show a line one
  pixel wide in the color of the hidden face. Removing the faces turned away, back-face
  culling (below), removes those lines; coplanar surfaces that overlap would tie over
  their whole area and have to be avoided in the scene.
- The depths are interpolated between vertices that were truncated to whole pixels
  (3.7), like `h`, so the depth at a pixel is that of a slightly moved triangle. The
  difference is far below the distance between two surfaces that do not touch.

Back-face culling. A closed object, one whose surface has no opening, shows a camera
outside it only its front faces, the triangles turned toward the camera, and those cover
its back faces completely. The depth buffer finds that out pixel by pixel, after the
back faces have been drawn; culling takes them out of the scene first, about half of its
triangles.

- A triangle with the corners `A`, `B`, `C`, in that order, is classified by the book's
  test, with all the points in one coordinate system:

  ```
  N = (B - A) x (C - A)        the triangle's normal
  V = camera - A               from a corner of the triangle to the camera
  <N, V> >  0                  front-facing
  <N, V> <= 0                  back-facing
  ```

  `<N, V> = |N| |V| cos(angle)` and the lengths are positive, so the sign of the dot
  product is the sign of the cosine: front-facing means an angle of less than 90
  degrees between the normal and the direction to the camera, and nothing is
  normalized. `V` may start at any point of the triangle: `<N, camera - P>` is the same
  for every `P` in its plane, the camera's distance from the plane times `|N|`. It is
  the direction to the camera and not the direction the camera looks in: under
  perspective a face turned partly away from the line of sight is still seen when it
  stands to the side. A triangle seen exactly edge-on (`<N, V> = 0`) is back-facing,
  the book's choice, and so is a triangle without area, whose `N` is zero. This is
  `IsBackFacing(a, b, c, camera_position)`.
- `N` points out of the object only if the model lists the corners of every triangle in
  the right order: clockwise as seen from outside the object, the book's rule, in the
  book's coordinates (X to the right, Y up, Z forward). The order is information the
  model carries, one bit per triangle. It cannot be computed from the triangle: `ABC`
  and `ACB` are the same three points, with opposite normals, and a flat triangle has no
  outside of its own. `ABC`, `BCA` and `CAB` are one order and one normal. (For a whole
  closed surface the order could be recovered: two triangles that share an edge must run
  along it in opposite directions, which fixes all of them from one, and the sign of the
  enclosed volume says which of the two consistent choices faces out. Nothing here does
  that; the models are written in the right order and trusted.)
- The order survives the pipeline. A rotation, a translation and a scaling by a positive
  factor (3.10) turn and move the normal with the triangle, and `ClipTriangle` (3.11)
  rotates the corners of its pieces and never swaps them. A negative scale would mirror
  the model and turn every triangle inside out; it is not supported together with
  culling.
- `CullBackFaces(object, camera_position)` is the book's Listing 12-1: the object, a
  `Model` whose vertices are in the same coordinates as the camera's position, without
  the triangles that `IsBackFacing` says are turned away, the others as they were and in
  their order. Only the triangle list changes. The vertex list stays, with the vertices
  that only back faces named still in it, and the bounding sphere stays.
- `RenderScene` (3.10) culls every instance after the transforms, in the camera's
  coordinates, where the camera is the origin, and before the clipping, so that no
  triangle is cut that would not be drawn. Every vertex that reaches `ProjectVertex`
  still has `z > 0`: an instance that no plane cuts is inside the clipping volume with
  all its vertices, named by a triangle or not, and an instance that a plane cuts loses
  its unnamed vertices there (3.11). Without clipping planes the precondition was
  already that every instance stands entirely in front of the camera.
- Culling is `RenderScene`'s default and is turned off by its last parameter,
  `BackFaceCulling::kOff`, with which every triangle is drawn. That is for a model that
  is not closed (a single triangle, a floor that may be seen from below); for a look
  into an object that the near plane has cut open, since from inside an object every
  face is a back face and with culling the cut shows what stands behind the object
  instead; and for a wireframe that is to show its far edges too. `RenderObject`,
  `RenderModel` and `RenderInstance` do not cull.
- With culling a closed convex object needs no depth buffer for itself: its front faces
  do not overlap. Between objects, and within an object that is not convex, front faces
  still hide each other and the depth buffer decides. The two are independent: either
  can be used without the other.
- Culling also settles the ties above along the edges of faces turned away: a face that
  is not drawn wins nothing. The ties between two front faces remain, and cannot be
  told from a correct picture.
- In exact arithmetic culling does not change the picture of a closed object, it only
  saves the work. Here it can change single pixels: the fill of 3.6 leaves the apex row
  of a triangle short now and then (section 15), so a back face can reach one pixel
  beyond the front faces that should cover it, and that pixel is no longer set. Section
  15 has the counts for the demo pictures, and the time culling saves.

### 3.13 Rasterizer: shading

Until here a triangle was drawn in its color, or in its color times intensities that
the model stated at its corners (3.8). With lights in the scene the intensities are
computed, by the illumination equation of 3.5: the lights and the equation are the ray
tracer's, and the code is the same (`lighting/`). The equation gives the light at one
point. Shading, the book's word, is how light computed at points is spread over a
triangle, and the book's Shading chapter has three ways to do it, each computing more
than the one before.

The light at a point:

- `ComputeIllumination(P, N, V, s, lights)` is the sum of 3.5 with nothing in the way
  of any light:

  ```
  i = sum(ambient I)
    + sum over point lights (L = position - P) and directional lights (L = direction) of
          DiffuseReflection(I, N, L)
        + SpecularReflection(I, N, L, V, s)         unless s = kMatte
  ```

  `N` is the unit normal of the surface at `P`, asserted; `V` points from `P` toward the
  viewer and may have any length. The result is not clamped and may exceed 1. The
  rasterizer casts no shadows: every light reaches every point.
- The equation asks only where the point, its normal, the lights and the viewer are
  relative to one another, so it may be evaluated in any coordinates all of them are
  in. The rasterizer uses the camera's, as the book's demo does: the vertices are there
  after the transforms of 3.10, and the viewer is the origin, so `V = -P`. The lights
  are given in the world's coordinates, where the instances are placed, and follow the
  scene: `TransformLight(light, M)` is the point light with the position
  `M * position`, the directional light with the direction
  `TransformDirection(M, direction)`, and the ambient light as it is.
- `TransformDirection(M, d)` is `M` applied to the homogeneous `(d.x, d.y, d.z, 0)`: a
  direction is the difference of two points, the 1s of their fourth coordinates cancel,
  and the matrix's translation column, multiplied by that 0, drops out. What is left is
  the upper 3 x 3 part of `M`, its rotation and scaling. Moving a scene does not change
  where an infinitely distant light is, or which way a surface faces.

Normals:

- The normal of a triangle `A B C` is `TriangleNormal(A, B, C) = (B - A) x (C - A)`,
  the vector back-face culling tests (3.12): it points out of an object whose triangles
  list their corners clockwise as seen from outside, is as long as twice the triangle's
  area, and is zero for a triangle without area.
- A triangle is flat, but the triangles of a sphere stand for a curved surface, which
  faces a different way at each of a triangle's three corners. `Triangle.normals[k]` is
  the normal of that surface at the corner `v[k]`, the book's model format
  (`normals = [N0, N1, N2]` beside `vertices`). It belongs to the corner of the
  triangle and not to the vertex, as in the book: at a corner of a cube three faces
  meet, each with a normal of its own. The zero vector, the default, says the corner
  has no normal of its own and the triangle's is used, which is right for every flat
  face, so a cube states nothing. A model may state its normals at any length.
- `Model.specular` is the exponent `s` for the model's surface, `kMatte` by default.
- `TransformModel(model, M)` (3.10) turns the normals with the model and returns them
  at unit length: each becomes `Normalized(TransformDirection(M, N))`, and a corner
  without a normal stays without one. The translation in `M` does not apply to a
  direction, and its scale, which stretches every direction, is taken out again by the
  normalization. From there on the normals are unit vectors, which is what lets them be
  interpolated with one another. Precondition, as before: `M` scales uniformly;
  otherwise a transformed normal is no longer perpendicular to its surface.
- `ClipTriangle` (3.11) gives a new corner the normal `NA + t (NB - NA)` and does not
  normalize it. It is shorter than 1, and it is exactly what the interpolation of the
  normals over the whole triangle has at that point, so interpolating between the
  corners of a piece continues the interpolation over the whole triangle.

The shading models. `ShadingModel` is `kFlat`, `kGouraud` or `kPhong`.
`RenderObject(viewport, object, lights, shading_model = kPhong, depth_buffer = none)`
draws an object under lights, the book's `RenderModel` of this chapter: `object` is a
`Model` in the camera's coordinates and `lights` are the lights in the same
coordinates. Every vertex is projected once, as in 3.8. For every triangle, with `Vk`
its corners, `Pk` their projections, `N = Normalized(TriangleNormal(V0, V1, V2))` and
`Nk` the normal the triangle states at corner `k`, or `N` where it states none:

- Flat shading computes the light once, at the triangle's center
  `C = (V0 + V1 + V2) / 3` with the triangle's own normal,
  `i = ComputeIllumination(C, N, -C, s, lights)`, and draws
  `DrawFilledTriangle(P0, P1, P2, color * i)`. It uses `N` whatever normals the model
  states: one point, one normal, one value per triangle. A flat face under a
  directional light has one value at every point, and flat shading is exact for it.
  Under a point light the two triangles of a face get values a little apart and a seam
  shows between them, and a curved surface shows every triangle in a shade of its own.
- Gouraud shading computes the light at the three corners,
  `hk = ComputeIllumination(Vk, Normalized(Nk), -Vk, s, lights)`, and draws
  `DrawShadedTriangle` (3.6) through the `Pk` with those intensities. With the
  triangle's own normal at every corner that removes the seam. With the normals of the
  surface at the corners, all the triangles around a vertex compute the same light
  there, the intensity runs on continuously across their edges, and a sphere looks
  round. What it cannot show is what the light does between the corners: a highlight
  smaller than a triangle is missed or caught at one corner and smeared, and a lamp
  close to a large face leaves the face dim, the dimmer the closer it comes, since its
  light reaches the far corners at a grazing angle (the book's Figure 13-7).
- Phong shading interpolates the normals instead of the light and computes the light at
  every pixel: `DrawPhongTriangle(viewport, P0, P1, P2, color, s, lights, depth_buffer)`
  with `Pk.normal = Nk`. It fills the triangle as `DrawFilledTriangle` does, the same
  pixels, and five quantities travel along the sides and along every row with x: `1/z`
  and the three components of the normal, each interpolated by the calls of 3.6 (for a
  quantity `v`: `v02 = Interpolate(y0, v0, y2, v2)` and
  `v012 = Interpolate(y0, v0, y1, v1)` without its last value `+
  Interpolate(y1, v1, y2, v2)`, the book's `EdgeInterpolate`, and
  `Interpolate(x_l, v_left, x_r, v_right)` in every row). At a pixel `(x, y)` that is
  to be set, which with a depth buffer is one where the triangle is the nearest so far:

  ```
  P = UnprojectVertex(viewport, x, y, 1/z)
  N = Normalized((nx, ny, nz))
  V = -P
  pixel = color * ComputeIllumination(P, N, V, s, lights)
  ```

  The normal is normalized after the interpolation, a vector between two unit vectors
  being shorter than either. Preconditions: every `Pk` has `inv_z > 0` and a normal
  that is not zero, and no two of the normals are opposite.
- `UnprojectVertex(viewport, x, y, inv_z)` is the book's `UnProjectVertex`, the way
  back from a pixel and its depth to the point it shows. With
  `(Vx, Vy, d) = CanvasToViewport(x, y)` the viewport point the pixel looks through, and
  `z = 1 / inv_z`, it returns `(Vx z / d, Vy z / d, z)`: the projection of 3.7 solved
  for the vertex. It inverts `ProjectVertex` up to that function's truncation.
  Precondition: `inv_z > 0`, asserted.
- None of the three reads the intensities stored in the triangles (`h`); under lights
  they are computed. A triangle without area has no normal and is not drawn. The normals
  are taken as they are: unit vectors as `TransformModel` leaves them, or interpolated
  between unit vectors as the clipping leaves them. Gouraud shading normalizes one to
  compute the light at its corner; Phong shading normalizes after interpolating.

Scenes under lights:

- `RenderScene(viewport, models, instances, camera, lights, shading_model = kPhong,
  depth_buffer = none, back_face_culling = kOn)` is the book's `RenderScene` of this
  chapter, the steps of 3.10 with the lights taken along:
  `M_camera = MakeCameraMatrix(...)`; every instance placed by `TransformModel`, which
  turns its normals; every light `L` replaced by `TransformLight(L, M_camera)`; back
  faces culled (3.12) and the scene clipped (3.11) as before; then every instance that
  is left drawn by `RenderObject(instance, lights, shading_model, depth_buffer)`.
  `lights` are in the world's coordinates.
- The overload that takes a triangle primitive (3.10) is unchanged and draws what it
  drew: outlines, plain colors, or the intensities a model states.
- Lit faces are solid, so a scene wants a depth buffer here unless it is listed from
  the back to the front. Back-face culling matters more than in 3.12: a face turned
  away from the camera is lit by its outward normal like any other.
- A triangle cut by a clipping plane is lit as its pieces. Under Phong shading that is
  the light the whole triangle would have shown, the pieces' corners having the normals
  the whole triangle has at those points. Under Gouraud shading it is not: a new corner
  gets the light computed there, where the whole triangle had the light interpolated
  between its own corners, which is the same only where the light is linear along the
  side. Under flat shading every piece is lit at its own center, so the pieces of one
  triangle can differ in shade when a point light is near.
- The interpolation of the normals, like that of `h`, is linear on the canvas and so not
  quite linear over the triangle in space unless the triangle is parallel to the
  viewport; only `1/z` is. Section 13 says what removes the difference.
- Shading does not change the outline of an object: a sphere made of triangles keeps its
  corners along its silhouette, however smoothly its inside is lit.

### 3.14 Numeric conventions

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
    math/vec3.h  mat4.h  plane.h  bounding_sphere.h  triangle_normal.h
    image/color.h  pixel_index.h  canvas.h  depth_buffer.h  image_io.h
    camera/viewport.h  camera.h  clipping_planes.h
    lighting/light.h  illumination.h
    raytracer/scene.h  raytracer.h
    rasterizer/point.h  interpolate.h  line.h  triangle.h  projection.h  object.h
    rasterizer/model.h  instance.h  culling.h  clipping.h  shading.h  scene.h
    demo/raytracer_scenes.h  rasterizer_scenes.h
                          the demo pictures shared by render_demo and the golden tests
src/<layer>/              implementation files, mirroring include/cgfs/<layer>/
apps/
    render_demo.cpp       command-line renderer for the demo pictures
tests/
    CMakeLists.txt
    support/test_helpers.h      Catch2 printers for Vec3/Mat4/Plane/Color/Rgb8/Point/Triangle/
                                Light, a temp-file helper
    math/vec3_test.cpp  mat4_test.cpp  plane_test.cpp  bounding_sphere_test.cpp
    math/triangle_normal_test.cpp
    image/color_test.cpp  pixel_index_test.cpp  canvas_test.cpp  depth_buffer_test.cpp
    image/image_io_test.cpp
    camera/viewport_test.cpp  camera_test.cpp  clipping_planes_test.cpp
    lighting/light_test.cpp  illumination_test.cpp
    raytracer/raytracer_test.cpp
    rasterizer/interpolate_test.cpp  line_test.cpp  triangle_test.cpp  projection_test.cpp
    rasterizer/object_test.cpp  model_test.cpp  instance_test.cpp  culling_test.cpp
    rasterizer/clipping_test.cpp  shading_test.cpp  scene_test.cpp
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
math         vec3, mat4, plane, bounding_sphere, triangle_normal    vector algebra and plain geometry
image        color, pixel_index, canvas, depth_buffer, image_io     colors, the pixel buffer, the depth buffer, file formats
camera       viewport, camera, clipping_planes                      how canvas pixels map into the world
lighting     light, illumination                                    the lights and the illumination equation
raytracer    scene, raytracer                                       the ray tracing renderer
rasterizer   point, interpolate, line, triangle, projection,        the rasterizing renderer
             object, model, instance, culling, clipping, shading,
             scene
demo         raytracer_scenes, rasterizer_scenes                    the demo pictures
```

- `math/vec3`: `Vec3` and its free functions, header-only, `constexpr` where possible.
  `math/mat4`: `Mat4`, the 4 x 4 matrix of the transforms, its product, its application
  to a point and to a direction and the scaling, translation and rotation constructors;
  header-only.
  `math/plane`: `Plane`, `SignedDistance` and `IntersectSegmentPlane`.
  `math/bounding_sphere`: `BoundingSphere` and `MakeBoundingSphere`.
  `math/triangle_normal`: `TriangleNormal`, the normal of a triangle from its three
  corners, which back-face culling and shading both ask for. All three header-only, and
  in this layer because they depend on nothing but `Vec3`.
- `image/color`: `Color`, `Rgb8`, `ToRgb8`, header-only. `image/pixel_index`:
  `PixelIndex`, the centered coordinate system as one function, header-only.
  `image/canvas`: the pixel buffer. `image/depth_buffer`: `DepthBuffer`, a `1/z` per
  pixel of a canvas, for hidden-surface removal; in this layer because it depends on
  nothing but the canvas's size and coordinates. `image/image_io`: PPM writer and
  reader, PNG writer; used by `Canvas::Save` and the tests.
- `camera/viewport`: pixel to viewport-point mapping and field of view. Used by the ray
  tracer and by the rasterizer's projection. `camera/camera`: `Camera`, a position, an
  orientation and a list of clipping planes, and `MakeCameraMatrix`; plain data for
  whichever renderer moves its camera, the rasterizer today.
  `camera/clipping_planes`: `MakeClippingPlanes`, the five planes of what a viewport
  shows.
- `lighting/light`: the three kinds of light and `Light`, the variant of them, plain
  data, and `TransformLight`, a light in other coordinates. `lighting/illumination`:
  the illumination equation, term by term, `DiffuseReflection` and `SpecularReflection`,
  and summed over a list of lights, `ComputeIllumination`; and `kMatte`, the specular
  exponent of a surface without a highlight. A layer of its own because both renderers
  light their scenes with it and neither may know the other.
- `raytracer/scene`: plain data: spheres, the lights of the lighting layer, background
  color. `raytracer/raytracer`: intersection, the lighting of a point with its shadows,
  the render loop.
- `rasterizer/point`: `Point`, with its intensity `h`, its depth `inv_z` and its
  `normal`, and `ToPixel`, the one conversion
  from interpolated coordinates to pixels. `rasterizer/interpolate`: `Interpolate`, the
  building block. `rasterizer/line`: `DrawLine`. `rasterizer/triangle`:
  `DrawWireframeTriangle`, `DrawFilledTriangle` and `DrawShadedTriangle`, the last two
  through a depth buffer when they are given one, the signature the three share,
  `DrawTriangleFunction`, and `DrawPhongTriangle`, which lights every pixel.
  `rasterizer/projection`: `ViewportToCanvas` and `ProjectVertex`, from a `Vec3` seen
  through a `Viewport` to the `Point` the primitives draw, the start of the 3D pipeline,
  and `UnprojectVertex`, the way back.
  `rasterizer/object`: `Triangle`, three vertex indices, a color and an intensity and a
  normal per corner, and `RenderObject`, which projects an object's vertex list and
  draws its triangles with the primitive it is given; the first use of the projection.
  `rasterizer/model`: `Model`, the vertex list and the triangle list of an object,
  stored once, with a bounding sphere and a specular exponent, `TransformModel`, the
  model under a transform, and `RenderModel`, which draws that through `RenderObject`. `rasterizer/instance`:
  `Instance`, a model index with a position, an orientation and a scale,
  `MakeModelMatrix`, its model matrix, and `RenderInstance`. `rasterizer/culling`:
  `IsBackFacing`, `CullBackFaces` and the switch `BackFaceCulling`, an object without
  the triangles that are turned away from the camera. `rasterizer/clipping`:
  `ClipScene`, `ClipInstance`, `ClipInstanceAgainstPlane`, `ClipTrianglesAgainstPlane`
  and `ClipTriangle`, what a list of planes leaves of a scene. `rasterizer/shading`:
  `ShadingModel`, flat, Gouraud or Phong, and `RenderObject` under lights, which draws
  an object's triangles in their colors times the light computed for them.
  `rasterizer/scene`: `RenderScene`, every instance of a model list and an instance
  list, in order, as a camera sees them: transformed, culled, clipped, projected and
  drawn, with a triangle primitive or under lights.
- `demo/raytracer_scenes`: the ray-traced demo scenes, each a function returning a
  `Scene`. `demo/rasterizer_scenes`: the rasterized demo pictures, each a function that
  draws into a canvas. Either way the app and the golden tests render exactly the same
  thing.

The canvas knows nothing about 3D, and the depth buffer nothing about triangles. The
viewport knows nothing about spheres, and a light nothing about what it lights. The tracer
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

### 5.1 `math/vec3.h`, `math/mat4.h`, `math/plane.h`, `math/bounding_sphere.h`, `math/triangle_normal.h`

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
constexpr Vec3 TransformDirection(const Mat4& m, Vec3 d); // m (d, 0): no translation (3.13)
constexpr Mat4 Transposed(const Mat4& m);                 // the inverse of a rotation
constexpr Mat4 MakeScalingMatrix(double scale);
constexpr Mat4 MakeTranslationMatrix(Vec3 translation);
Mat4           MakeOYRotationMatrix(double degrees);      // about Y; cos and sin
```

`Mat4` is plain data with free functions, like `Vec3`; the constructors carry the book's
demo names. There is no projection matrix (section 14, decision 12).

```cpp
// plane.h
struct Plane {                        // the points P with <N, P> + D = 0 (section 3.11)
    Vec3 normal;                      // N, unit length; points to the plane's front
    double d = 0.0;                   // D
    bool operator==(const Plane&) const = default;
};
constexpr double SignedDistance(const Plane& plane, Vec3 vertex);        // <N, vertex> + D
constexpr double IntersectSegmentPlane(Vec3 a, Vec3 b, const Plane& plane);
// t with a + t (b - a) on the plane: (-D - <N, a>) / <N, b - a>

// bounding_sphere.h
struct BoundingSphere {               // a sphere that contains an object (section 3.11)
    Vec3 center;
    double radius = infinity;         // the default contains everything
    bool operator==(const BoundingSphere&) const = default;
};
BoundingSphere MakeBoundingSphere(std::span<const Vec3> points);  // average; farthest point

// triangle_normal.h
constexpr Vec3 TriangleNormal(Vec3 a, Vec3 b, Vec3 c);   // (b - a) x (c - a), not normalized
// toward whoever sees a, b, c run clockwise (sections 3.12 and 3.13)
```

Plain geometry on `Vec3`, header-only like the rest of the layer. The ray tracer's
`Sphere` (5.7) is a thing to be drawn, with a color and a finish; a `BoundingSphere` is
only a bound, and shares nothing with it but the two fields.

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

### 5.3 `image/pixel_index.h`, `image/canvas.h`, `image/depth_buffer.h`

```cpp
// pixel_index.h
constexpr std::optional<std::size_t> PixelIndex(int width, int height, int x, int y);
// row * width + column of the pixel at centered (x, y); nullopt outside (section 3.1)

// canvas.h
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

// depth_buffer.h
class DepthBuffer {                          // 1/z per pixel of a canvas (section 3.12)
public:
    DepthBuffer(int width, int height);      // every pixel 0, infinitely far
    explicit DepthBuffer(const Canvas& canvas);
    // throws std::invalid_argument when width <= 0 or height <= 0

    int width() const;
    int height() const;

    bool UpdateIfCloser(int x, int y, double inv_z);
    // inv_z > stored: store it, true. Otherwise, or out of range: false, nothing changes
    std::optional<double> InvZ(int x, int y) const;     // nullopt when out of range
};
```

### 5.4 `image/image_io.h`

```cpp
struct Image { int width = 0, height = 0; std::vector<std::uint8_t> rgb; };
void  WritePpm(const std::filesystem::path&, int width, int height, std::span<const std::uint8_t> rgb);
void  WritePng(const std::filesystem::path&, int width, int height, std::span<const std::uint8_t> rgb);
Image ReadPpm(const std::filesystem::path&);   // binary P6 with maxval 255; used by golden tests
```

### 5.5 `camera/viewport.h`, `camera/camera.h`, `camera/clipping_planes.h`

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
    std::vector<Plane> clipping_planes;       // in the camera's coordinates; none: no clipping (3.11)
};
Mat4 MakeCameraMatrix(Vec3 position, const Mat4& orientation);  // Transposed(orientation) * T(-position)

// clipping_planes.h
std::vector<Plane> MakeClippingPlanes(const Viewport& viewport);
// near, left, right, bottom, top: what the viewport shows, half a pixel more at the sides
```

The default camera is the origin looking along +Z, without clipping planes: the camera
of every earlier chapter. The viewport is not part of it: the camera says where the eye
is, the viewport what window it looks through. The clipping planes are the camera's, as
in the book's demo, because they are given in its coordinates and move with it;
`MakeClippingPlanes` derives them from the viewport that the same camera looks through,
and the caller keeps the two together.

### 5.6 `lighting/light.h`, `lighting/illumination.h`

```cpp
// light.h
struct AmbientLight     { double intensity = 0; };
struct PointLight       { double intensity = 0; Vec3 position; };
struct DirectionalLight { double intensity = 0; Vec3 direction; };   // points toward the light
using Light = std::variant<AmbientLight, PointLight, DirectionalLight>;
// each of the three compares with ==
template <class... Ts> struct Overloaded;   // one lambda per kind of light, for std::visit
Light TransformLight(const Light& light, const Mat4& transform);
// the position as a point, the direction as a direction, ambient as it is (section 3.13)

// illumination.h (sections 3.5 and 3.13)
inline constexpr double kMatte = -1.0;    // the specular value of a surface with no highlight
double DiffuseReflection(double light_intensity, Vec3 normal, Vec3 to_light);
// I <N, L> / (|N| |L|), or 0 when <N, L> <= 0
double SpecularReflection(double light_intensity, Vec3 normal, Vec3 to_light, Vec3 view,
                          double exponent);
// I (<R, V> / (|R| |V|))^s with R = Reflect(L, N), or 0 when <R, V> <= 0; |normal| = 1
double ComputeIllumination(Vec3 point, Vec3 normal, Vec3 view, double specular,
                           std::span<const Light> lights);
// the sum over the lights, none blocked; |normal| = 1, view from the point to the viewer
```

`std::variant` instead of a class hierarchy: lights are a small closed set, value
semantics keep scenes copyable and trivially testable, and `std::visit` dispatches on
the light type; `Overloaded` is the usual helper for it, here because every visit of a
light uses it. The two terms are inline functions in the header, so the ray tracer's
inner loop is the code it was when they were private to it. `ComputeIllumination` takes
the lights as a span, so it serves a `std::vector<Light>` as it is.

### 5.7 `raytracer/scene.h`

```cpp
struct Sphere {
    Vec3 center; double radius = 1.0; Color color;
    double specular = kMatte;     // shininess exponent
    double reflective = 0.0;      // 0: no reflection .. 1: a perfect mirror
};

struct Scene {
    std::vector<Sphere> spheres;
    std::vector<Light>  lights;                 // lighting/light.h
    Color background_color = colors::kWhite;
};
```

`Sphere` grew its `specular` and `reflective` fields one chapter at a time; aggregate
initialization with designated initializers and default member values kept the existing
scene code compiling each time.

### 5.8 `raytracer/raytracer.h`

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

### 5.9 `rasterizer/point.h`, `interpolate.h`, `line.h`, `triangle.h`, `projection.h`, `object.h`, `model.h`, `instance.h`, `culling.h`, `clipping.h`, `shading.h`, `scene.h`

```cpp
// point.h
struct Point {                 // canvas coordinates, intensity and depth (section 3.6)
    int x = 0;
    int y = 0;
    double h = 1.0;            // multiplier for the color; read by DrawShadedTriangle only
    double inv_z = 0.0;        // 1/z of the projected vertex; read through a depth buffer
    Vec3 normal;               // of the surface at the vertex; read by DrawPhongTriangle only
    bool operator==(const Point&) const = default;
};
constexpr int ToPixel(double coordinate);    // truncation toward zero (section 3.6)

// interpolate.h
std::vector<double> Interpolate(int i0, double d0, int i1, double d1);
// i1 - i0 + 1 values of d at i0..i1; {d0} when i0 == i1; precondition i0 <= i1 (section 3.6)

// line.h
void DrawLine(Canvas& canvas, Point p0, Point p1, Color color);

// triangle.h
void DrawWireframeTriangle(Canvas& canvas, Point p0, Point p1, Point p2, Color color,
                           DepthBuffer* depth_buffer = nullptr);  // ignores the buffer
void DrawFilledTriangle(Canvas& canvas, Point p0, Point p1, Point p2, Color color,
                        DepthBuffer* depth_buffer = nullptr);
void DrawShadedTriangle(Canvas& canvas, Point p0, Point p1, Point p2, Color color,
                        DepthBuffer* depth_buffer = nullptr);     // color * h
// with a depth buffer, of the canvas's size, only the pixels where the triangle is the
// nearest so far (section 3.12)
using DrawTriangleFunction = void (*)(Canvas& canvas, Point p0, Point p1, Point p2, Color color,
                                      DepthBuffer* depth_buffer);
// the signature of the three, for whoever draws many triangles
void DrawPhongTriangle(Canvas& canvas, const Viewport& viewport, Point p0, Point p1, Point p2,
                       Color color, double specular, std::span<const Light> lights,
                       DepthBuffer* depth_buffer = nullptr);
// color * ComputeIllumination at every pixel, from the interpolated 1/z and normals; the
// lights in the camera's coordinates; every point with inv_z > 0 and a normal (section 3.13)

// projection.h
Point ViewportToCanvas(const Viewport& viewport, double x, double y);  // truncated (section 3.7)
Point ProjectVertex(const Viewport& viewport, Vec3 v);  // precondition v.z > 0; inv_z = 1 / v.z
Vec3 UnprojectVertex(const Viewport& viewport, int x, int y, double inv_z);
// the point seen at the pixel at depth 1 / inv_z; precondition inv_z > 0 (section 3.13)

// object.h
struct Triangle {                     // one face of an object (section 3.8)
    std::array<std::size_t, 3> v{};   // indices into the object's vertex list
    Color color;
    std::array<double, 3> h{1.0, 1.0, 1.0};  // the intensity at each corner
    std::array<Vec3, 3> normals{};    // the surface's normal at each corner; zero: the triangle's
    bool operator==(const Triangle&) const = default;
};
void RenderObject(Canvas& canvas, const Viewport& viewport, std::span<const Vec3> vertices,
                  std::span<const Triangle> triangles,
                  DrawTriangleFunction draw_triangle = DrawWireframeTriangle,
                  DepthBuffer* depth_buffer = nullptr);
// every index < vertices.size(); every vertex has z > 0

// model.h
struct Model {                        // an object's shape, described once (section 3.9)
    std::vector<Vec3> vertices;       // in the model's own coordinates
    std::vector<Triangle> triangles;
    BoundingSphere bounding_sphere;   // around the vertices; the default contains everything
    double specular = kMatte;         // the exponent of the surface's highlight (section 3.13)
    bool operator==(const Model&) const = default;
};
Model TransformModel(const Model& model, const Mat4& transform);
// transform * every vertex, the same triangles with their normals turned and at unit length,
// the sphere moved and scaled along (sections 3.11 and 3.13)
void RenderModel(Canvas& canvas, const Viewport& viewport, const Model& model,
                 const Mat4& transform);  // RenderObject on TransformModel's vertices (section 3.10)

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

// culling.h (section 3.12)
enum class BackFaceCulling { kOff, kOn };   // whether RenderScene removes back faces
bool IsBackFacing(Vec3 a, Vec3 b, Vec3 c, Vec3 camera_position);
// true when <(b - a) x (c - a), camera_position - a> <= 0; the normal points out of an
// object whose triangles list their corners clockwise as seen from outside
Model CullBackFaces(Model object, Vec3 camera_position);
// the object without its back-facing triangles; vertices and bounding sphere unchanged

// clipping.h (section 3.11); an "instance" is a Model in the camera's coordinates
void ClipTriangle(const Triangle& triangle, const Plane& plane, std::vector<Vec3>& vertices,
                  std::vector<Triangle>& clipped_triangles);  // appends 0, 1 or 2 triangles
std::vector<Triangle> ClipTrianglesAgainstPlane(std::span<const Triangle> triangles,
                                                const Plane& plane, std::vector<Vec3>& vertices);
std::optional<Model> ClipInstanceAgainstPlane(Model instance, const Plane& plane);
std::optional<Model> ClipInstance(Model instance, std::span<const Plane> planes);
std::vector<Model> ClipScene(std::vector<Model> scene, std::span<const Plane> planes);

// shading.h (section 3.13)
enum class ShadingModel { kFlat, kGouraud, kPhong };
void RenderObject(Canvas& canvas, const Viewport& viewport, const Model& object,
                  std::span<const Light> lights,
                  ShadingModel shading_model = ShadingModel::kPhong,
                  DepthBuffer* depth_buffer = nullptr);
// the object, a Model in the camera's coordinates, under lights in the same coordinates:
// every triangle in its color times the light computed per triangle, per corner or per pixel

// scene.h
void RenderScene(Canvas& canvas, const Viewport& viewport, std::span<const Model> models,
                 std::span<const Instance> instances, const Camera& camera = Camera{},
                 DrawTriangleFunction draw_triangle = DrawWireframeTriangle,
                 DepthBuffer* depth_buffer = nullptr,
                 BackFaceCulling back_face_culling = BackFaceCulling::kOn);
// every instance, in list order, as camera sees it: transformed, culled, clipped, projected,
// drawn; through the depth buffer, if there is one, the nearest surface shows whatever the
// order; with BackFaceCulling::kOff the back faces are drawn too
void RenderScene(Canvas& canvas, const Viewport& viewport, std::span<const Model> models,
                 std::span<const Instance> instances, const Camera& camera,
                 std::span<const Light> lights,
                 ShadingModel shading_model = ShadingModel::kPhong,
                 DepthBuffer* depth_buffer = nullptr,
                 BackFaceCulling back_face_culling = BackFaceCulling::kOn);
// the same steps under lights, which are given in the world's coordinates and taken into
// the camera's with the scene; every instance drawn by RenderObject with the shading model
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
and draws the same picture. Clipping and shading arrived the same way: the camera's
clipping planes, a triangle's intensities and a model's bounding sphere are fields with
defaults, the triangle primitive is a defaulted parameter, and without them a scene is
drawn as it was. Hidden-surface removal arrived the same way once more: a point's
`inv_z` is a field with a default, and the depth buffer is a defaulted last parameter
of the primitives, of `RenderObject` and of `RenderScene`, a pointer because "none" is
the default and the buffer is the caller's, like the canvas. The one signature that
changed outright is the function pointer type `DrawTriangleFunction`, which cannot
have a default; whoever passes one of the three primitives by name is not affected.
Back-face culling arrived as one more defaulted parameter of `RenderScene`, and is the
first whose default changes what a call written before it draws: the default is on
(section 14, decision 16), so a scene that needs its back faces now says so. The switch
is a scoped enumeration rather than a `bool`, so that a call reads
`..., &depth_buffer, BackFaceCulling::kOff)`. `CullBackFaces` takes its object by value
and returns it, like the clipping functions, and takes the camera as a position, the one
thing about a camera the test asks for.
The clipping functions carry the book's names and take their instance, and `ClipScene`
its scene, by value: an instance that needs no clipping is moved through untouched,
which is what the pseudocode's "return instance" means, and the caller chooses between
handing a scene over (`std::move`) and keeping it. "Nothing is left" is an empty
`std::optional`, the pseudocode's NULL.
They are the primitives; a class comes when there is state to protect, and the depth
buffer is the first in the rasterizer: a buffer whose size and contents must stay in
step. The primitives stay free functions.
Shading arrived as additions beside what was there (section 14, decision 18). The
normals of a triangle, the normal of a point and the specular exponent of a model are
fields with defaults, none, none and matte, so every model and every point written
before them is what it was. The drawing under lights is a second `RenderScene` and a
second `RenderObject`, overloads that take a list of lights and a `ShadingModel` where
the first take a triangle primitive: the book keeps the names and adds the lights to
the parameters, and so does this. The two `RenderObject`s are in two headers, the first
with `Triangle` and the second with the shading it does. The shading model is a scoped
enumeration with the book's three names and defaults to Phong, the book's demo's
default and the one of the three that is right everywhere; a caller who wants a cheaper
one says so. `DrawPhongTriangle` does not have the signature the other three primitives
share and is not a `DrawTriangleFunction`: it needs the viewport, to find the point a
pixel shows, and the lights.

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
  `TransformDirection` leaves a direction as it is under a translation that moves the
  same three numbers as a point, turns it under a rotation and stretches it under a
  scaling; for a translation times a rotation times a scaling by 1.5 the direction
  `q - p` comes out as `M q - M p` within `1e-12`, 1.5 times as long; the zero vector
  stays zero; it is usable in a constant expression.
- `plane`: `SignedDistance` is positive in front of the plane, negative behind it, zero
  on it and `D` at the origin; flipping the normal and `D` exchanges the sides; a slanted
  unit normal (0.6, 0, 0.8) gives distances within `1e-12`; a normal of length 2 gives
  twice the distance and the same zero set. `IntersectSegmentPlane` is 1/4 from `z = 0`
  to `z = 4` across `z = 1` and 3/4 from the other end; `A + t (B - A)` lies on the
  plane, exactly for round numbers and within `1e-12` for a slanted plane; it is 0 with
  `A` on the plane, 1 with `B` on it and outside `[0, 1]` beyond the segment. Both are
  usable in constant expressions.
- `bounding_sphere`: the default sphere has an infinite radius and contains anything;
  `MakeBoundingSphere` of the book's cube is the origin and `sqrt(3)`, and of a box off
  the origin its center and half its diagonal, exactly; of a lopsided set it is the
  average and the distance to the farthest point (center (1, 0, 0), radius 3, where the
  smallest sphere has radius 2) and still contains every point; one point gives radius
  0 and no points the origin with radius 0.
- `triangle_normal`: for a viewer at the origin who looks along +Z, three corners that
  run clockwise give `(0, 0, -4)`, toward the viewer, and the same corners the other way
  round `(0, 0, 4)`; the three rotations of the corners give one normal; for a right
  triangle with legs 3 and 4 in a tilted plane the normal is perpendicular to all three
  sides and 12 long, twice the area; a floor listed clockwise as seen from above faces
  up; three points on a line, or two corners at one place, give the zero vector; usable
  in a constant expression.
- `color`: scaling, addition, `ToRgb8` clamping and truncation (e.g. `254.9 -> 254`,
  `300 -> 255`, `-5 -> 0`).
- `pixel_index`: on a 4 x 4 grid the origin has index 6 and the four corners 0, 3, 12
  and 15; a step in x is the next index and a step up one width earlier; coordinates
  outside the grid have none; a 5 x 3 grid follows the integer-division rule; the
  function is usable in a constant expression; and for every pixel of a 5 x 3 canvas
  and a border around it `PutPixel` changes exactly the three bytes at
  `3 * PixelIndex`, or nothing where there is no index.
- `canvas`: default dimensions; `Clear` with and without a color; `PutPixel` at the
  origin and the four corners lands on the expected buffer rows and columns; writes
  outside the range are ignored; odd dimensions follow the integer-division rule;
  `GetPixel` round-trips `PutPixel`.
- `depth_buffer`: non-positive dimensions throw; the size can come from a canvas; an
  empty buffer holds 0 at every pixel; any positive `1/z` is nearer than nothing; a
  nearer surface replaces a farther one and not the other way around; of two equal
  depths the first stays; 0 and negative values are never stored; every pixel has a
  depth of its own; coordinates outside the buffer are never the nearest and have no
  value; a 5 x 3 buffer has exactly the pixels of a 5 x 3 canvas.
- `image_io`: PPM write then read round-trips the bytes; PNG write produces a file that
  starts with the PNG signature.
- `viewport`: a table of points for a 1920 x 1080 canvas and a 1 x 1 x 1 viewport
  (center, top-right `(0.5, 0.5, 1)`, bottom-left, quarter); the aspect-matched
  viewport maps the top-right corner to `(16/18, 0.5, 1)`; `Fov()` of the default
  viewport is 53.13 degrees on both axes within 0.01.
- `camera`: the default camera is at the origin with the identity orientation, has no
  clipping planes and its matrix is exactly the identity; a moved camera maps its own position to the origin and
  a point 5 ahead to `(0, 0, 5)`; a camera turned 90 degrees looks along -X and maps
  `(-5, 0, 0)` to `(0, 0, 5)`, what was ahead to its right and up to up; for the book's
  demo camera, `(-3, 1, 2)` turned -30 degrees, the points `position + orientation * v`
  for `v` on the camera's axes map to `v`, and the matrix equals the transpose times the
  translation.
- `clipping_planes`: five planes, the near one first, `(0, 0, 1)` with `D = -d` for two
  distances; for a field of view of 90 degrees on a canvas of 10^9 pixels a side, where
  the half pixel does not show, the normals are the book's within `1e-6` and every side
  plane has `D = 0`; on 64 x 32 pixels with a 2 x 1 viewport at distance 2 the normals
  are those of `(±2, 0, 1 + 1/64)` and `(0, ±2, 1/2 + 1/64)`; every normal has length 1
  for four viewports; for the demo viewport on 320 x 180 the rectangle half a pixel
  larger than the viewport lies on the four side planes at three depths, the viewport's
  center is on the near plane and in front of the others, the viewport's own sides are
  inside the volume by less than half a pixel and the points one pixel beyond them
  outside it; a point of each side plane projects to ±160.5 or ±90.5, the middle of a
  pixel; a table of points is in or out of the volume as it projects inside the viewport
  and is no nearer than it, each side plane rejecting only its own side, and a point
  behind the camera on the rays of a point in view is out; a viewport at distance 2
  narrows the volume and moves the near plane.
- `lighting`: `kMatte` is -1. `DiffuseReflection`: a light straight above the surface
  arrives at full intensity; the intensity follows the cosine of the angle (0.5 at 60
  degrees, `1 / sqrt(2)` at 45, within `1e-12`); a light behind the surface, or grazing
  it, adds exactly nothing; neither vector has to be normalized. `SpecularReflection`:
  the full intensity is seen in the mirror direction, for any exponent; it fades as
  `cos^s` away from it (s = 2 and s = 10 at 45 degrees) and a larger exponent leaves
  less; nothing is seen from more than 90 degrees off the mirror direction, or at 90
  exactly; the lengths of `L` and `V` do not matter; the term is not gated on the light
  being in front of the surface. `ComputeIllumination`: ambient light adds its
  intensity everywhere and for any exponent, and no lights give 0; the direction to a
  point light is taken from the point (full intensity under it, `1 / sqrt(2)` from 5 to
  the side of a light 5 up, nothing from above it), and a directional light arrives
  alike at points far apart; three lights add up to 1.3, unclamped; a shiny surface adds
  the highlight (the full intensity in the mirror direction, `1 / 32` at 45 degrees with
  s = 10, for a point light too) and a matte one none; a light a million units away
  arrives, nothing being in the way; and the light at a point is the same within
  `1e-12` when the point, the normal, the viewer and three lights are taken into the
  coordinates of a camera that stands elsewhere and is turned 70 degrees, points as
  points and directions as directions. `Light`: two lights are equal when they are of
  one kind with the same values. `TransformLight`: ambient light is unchanged by any
  transform; a point light's position is moved by a translation and turned by a
  rotation, its intensity kept; a directional light is not moved by a translation and
  is turned by a rotation; through the matrix of a camera at (4, 5, 6) that looks along
  -X a lamp at the camera's place lands at the origin, one 2 ahead of it at `(0, 0, 2)`,
  and a sun in the world's -X straight ahead.
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
    `DrawLine` calls. With a depth buffer: of two triangles that overlap, one at z = 2
    and one at z = 4, the nearer shows in either order, which is the painter's picture
    with the far one first; without a buffer the later one shows and the depths are
    not looked at; on an empty buffer a triangle covers what it covers without one,
    and the buffer holds its `1/z` exactly there; on the triangle (0, 0), (8, 0),
    (0, 8) with `1/z` = 1, 1/4 and 1/2 the buffer holds those at the vertices, 0.75
    and 0.375 halfway along two sides, 0.6875 at (2, 2), and 0.625 halfway along the
    bottom side, which is z = 1.6 and not 2.5; that triangle and one facing the camera
    at z = 2 on the same pixels are sorted out pixel by pixel, the tilted one in front
    up to x = 5 on the bottom row (interpolating z would stop at x = 2), and on the
    line where the two are exactly equally deep the one drawn first stays; a second
    triangle at the same depth changes nothing; a triangle of points without depth is
    not drawn; a triangle that reaches beyond the canvas is drawn as without a buffer.
    `DrawWireframeTriangle` draws its outline whole through a depth buffer and leaves
    no depth behind.
  - `DrawShadedTriangle`: with `h = 1` at every vertex it gives the filled triangle's
    canvas for eleven shapes, among them the two whose rows the filled triangle skips
    (the two-row case and the straddled apex row of section 15); a constant `h` of 0.5
    gives 127 everywhere; on the right triangle with the vertical side x = 0, `h = 0, 1,
    0` at its vertices gives `255 * x / 4` at every pixel and `h = 0, 0, 1` gives
    `255 * y / 4` (every value a multiple of 1/4, so exact to the byte); all six vertex
    orders give the same canvas and each intensity stays with its vertex; the two
    triangles of a rectangle cut along its diagonal give identical bytes on the
    diagonal's three pixels; `h = 2` is clamped to 255 by the canvas and a negative `h`
    to 0. With a depth buffer the nearer of two shaded triangles shows in either
    order with its shading, the painter's picture to the byte, and with `h = 1` the
    canvas and the depths are those of the filled triangle.
  - `DrawPhongTriangle` (a 32 x 32 canvas through a 1 x 1 viewport at distance 1, where
    the pixel `(x, y)` at depth 1 shows the point `(x / 32, y / 32, 1)`; the triangles
    face the camera unless said otherwise): under ambient light of 0.5 it is the filled
    triangle in half the color, for the eleven shapes of the shaded test; under a light
    from behind the camera it is evenly `0.75` of the color; with a lamp a quarter of a
    unit in front of the face, ambient light and an exponent of 20, every one of more
    than 200 pixels is the color times `ComputeIllumination` at the point it shows, and
    nothing else is lit; a lamp in front of the point of the pixel (0, -4) lights that
    pixel at 255 and the corners at 123, where `DrawShadedTriangle` with the corners'
    light leaves the pixel between 100 and 125; on the right triangle (0, 0), (8, 0),
    (0, 8) with normals toward the camera, along +X and along +Y the bottom side reads
    255, 241, 180, 80 and 0 at x = 0, 2, 4, 6 and 8, the cosines of the interpolated and
    normalized normals (180 where interpolated light would give 127), the left side the
    same and the pixel (2, 2) 208; with a lamp at the camera a shiny face reads 204 on
    the axis, where a matte one reads 102, and no more than the matte one eight pixels
    to the side; all six vertex orders give one canvas for a triangle with three depths
    and three normals, and exchanging two normals gives another; the same pixels at
    depth 4 show a triangle four times as large, a lamp in front of its point lighting
    the pixel in full; through a depth buffer the nearer of two shows in either order,
    the picture of the filled triangles, and the buffer holds what a filled triangle
    leaves in it, 0.625 halfway along a side from `1/z` = 1 to 1/4.
  - `ViewportToCanvas`: the viewport test's table the other way round (the default
    viewport on 1920 x 1080); a 2 x 1 viewport scales each axis by its own ratio;
    truncation toward zero (`33.7 -> 33`, `-33.7 -> -33`, `0.5` and `-0.5` to 0); the
    point has no depth, `inv_z = 0`.
  - `ProjectVertex`: a vertex on the viewport plane keeps its x and y; a vertex twice
    and four times as far lands at a half and a quarter of the distance from the
    center, and one nearer than the viewport beyond the canvas; four vertices on one
    ray project to one pixel; the viewport at distance 2 sees `(0.5, 0.5, 4)` where
    the one at distance 1 sees `(0.5, 0.5, 2)`; on a 64 x 32 canvas with a 2 x 1
    viewport at distance 2 (powers of two, so every step is exact) `ProjectVertex`
    inverts `CanvasToViewport` on every pixel. The point's `inv_z` is `1 / z`: 1, 1/2
    and 1/8 for three vertices on one ray, larger for the nearer of two vertices
    anywhere, and the vertex's depth whatever the viewport's distance.
  - `UnprojectVertex`: on 1920 x 1080 the pixel (480, -540) at depth 1 is
    `(0.25, -0.5, 1)` and at depth 4 `(1, -2, 4)`, four times as far along one ray; the
    middle pixel looks along the axis; through a viewport at distance 2 the pixel
    (960, 540) is `(0.5, 0.5, 2)` at depth 2 and `(1, 1, 4)` at depth 4; on the 64 x 32
    canvas of the round trip above, for every pixel and three depths, the vertex on the
    pixel's ray projects to the pixel with its `1/z` and unprojects to itself, exactly.
  - `RenderObject` (on a 32 x 32 canvas through a 1 x 1 viewport at distance 1, where a
    vertex `(x, y, 1)` lands on the pixel `(32 x, 32 y)`): one triangle gives the canvas
    of `DrawWireframeTriangle` on its projected corners; a triangle names its corners by
    index in any order, and a vertex no triangle uses leaves no pixel; the square cut
    along its diagonal, two triangles sharing two vertices, gives the canvas of the two
    wireframes and lights the outline plus the diagonal, 79 pixels; every triangle keeps
    its color and the one drawn later owns the shared edge, in both orders; a triangle
    twice as far and twice as large gives the same picture, and twice as far without the
    scaling half the size; the lists may be a `std::array` and a plain array; an object
    without triangles, or without vertices, draws nothing. A `Triangle`'s intensities
    are 1 unless given; with `DrawFilledTriangle` the canvas is that primitive's on the
    projected corners, and with `DrawShadedTriangle` and the default intensities the
    same; a shaded triangle with the intensities 1, 0.25 and 0.5 gives the canvas of
    `DrawShadedTriangle` on points that carry them, 255, 63 and 127 at its corners,
    whichever order the corners are named in; the two triangles of the square can be
    differently bright at the vertices they share; outlines and fills ignore the
    intensities. Through a depth buffer, of two filled triangles that overlap, at
    z = 1 and z = 2, the nearer shows in both list orders, the painter's picture with
    the far one first; the buffer holds 1/2 where only the far one is; two objects
    drawn through one shared buffer hide each other and through a buffer each the
    later one shows; a wireframe is drawn whole. A `Triangle` states no normals unless
    given, and the three primitives draw the same picture whatever normals it states.
  - `RenderModel` (same canvas and viewport; a lopsided triangle in its own plane
    z = 0): a translation to z = 1 gives the wireframe of its corners and the canvas of
    `RenderObject` on the vertices moved by hand; the transform is applied before the
    projection (twice as far is half the size; scaled by 2 and twice as far is the same
    picture); a half turn about Y mirrors the model in x, exactly, since cos 180 is -1
    and the model has z = 0; a quarter turn shows it edge-on, 14 pixels on the column
    x = 0 from y = -7 to 6; a product of two translations draws as their sum; an empty
    model draws nothing. `TransformModel` transforms every vertex and keeps the
    triangles, and with the identity returns the model; a model's default bounding
    sphere contains everything and stays so; a translation moves a sphere's center and
    keeps its radius, a scaling by 2 or by 0.75 multiplies the radius, all exactly, a
    rotation keeps it within `1e-12`, and under an instance's whole model matrix the
    radius is multiplied by the scale; the transformed sphere of `MakeBoundingSphere`
    still contains the transformed vertices under three transforms, one of them a
    mirroring. A model is matte unless it says otherwise, and a transformed model has
    its model's exponent; a translation leaves the normals of a triangle as they are
    while it moves the vertices; a quarter turn about Y takes normals along +X, +Y and
    +Z to +Z, +Y and -X; the normals come out at unit length under a scaling by 0.5 or
    by 3, under the identity when the model states them 4, 0.25 and 5 long, and turned,
    scaled and moved at once; a corner without a normal stays without one.
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
  - Clipping (against the plane `z = 1`, whose normal is an axis, with coordinates that
    are small binary fractions, so every distance, fraction and crossing point is
    exact). `ClipTriangle`: a triangle in front is appended as it is and one behind not
    at all, in both cases without a new vertex; one corner in front, its sides cut at
    1/4 and 1/2, gives the triangle `A B' C'` with the two crossing points appended in
    that order and the intensities `1 + 1/4 (0 - 1)` and `1 + 1/2 (0.25 - 1)`; the same
    result whichever of the three corners is in front, and the other winding gives
    `A C' B'`; two corners in front give `A B A'` and `A' B B'` with the intensities
    interpolated, whichever corner is behind; for both shapes and all six corner orders
    every piece has the winding and the plane of its triangle and the pieces' areas add
    up to the part in front (1/8 and 3/4 of the triangle); a corner or a side on the
    plane makes a triangle in front be kept, one behind be dropped and one in the plane
    be kept, all without new vertices; a triangle cut through a corner gives one
    triangle, the crossing on the side to that corner being the corner itself; two
    triangles that share a cut side and list it in opposite directions get the same
    crossing point bit for bit, with coordinates that are not round. Normals: with one
    corner in front the two new corners get `NA + 1/4 (NB - NA)` and
    `NA + 1/2 (NC - NA)`, exactly and not normalized; with two in front both pieces get
    the two normals interpolated half way; a triangle without normals is cut into
    pieces without normals, whichever corner the list starts from and in either
    winding; a triangle kept whole keeps its normals.
    `ClipTrianglesAgainstPlane` keeps the order and appends the new vertices after the
    old. `ClipInstanceAgainstPlane` (a 2 x 2 square of two triangles with its bounding
    sphere): in front it is returned equal to itself, behind nothing is returned, and
    across the plane its three remaining triangles and six vertices are exactly the
    expected ones, half the area, every vertex in front and in use, the crossing on the
    shared diagonal present twice at the same place, the sphere unchanged; the sphere is
    trusted, in both directions, against the triangles; a sphere that touches the plane
    (`d = r`, `d = -r`) decides nothing; with the default sphere every case goes through
    the triangles, an instance behind coming back with no triangles and no vertices; a
    vertex behind the camera that no triangle names is removed. `ClipInstance`: no
    planes return the instance; two planes leave a quarter of the square in either
    order; nothing is left as soon as one plane leaves nothing. `ClipScene`: four
    instances are kept, removed, clipped and kept, in order; no planes return the scene
    and no scene gives none. Against `MakeClippingPlanes` of the demo viewport: the
    book's cube at eight places across the planes (the near plane through it, the
    camera inside it, each side, a corner, three planes at once) comes back changed and
    not empty, with every vertex in use, in front of all five planes within `1e-9`, at
    `z > 0.999`, and projected by `ProjectVertex` to `-160 <= x <= 160`,
    `-90 <= y <= 90`; a cube well inside the volume is returned equal to itself, and
    cubes behind the camera or beyond a side leave nothing.
  - Back-face culling (coordinates that are whole numbers and small binary fractions, so
    every normal and every dot product is exact). `IsBackFacing`: a triangle in front of
    a camera at the origin with its corners clockwise as the camera sees them is a front
    face and the same corners the other way round a back face, from whichever corner
    the list starts; the same triangle is a front face from one side of its plane and a
    back face from the other, however far to the side the camera stands; seen edge-on,
    from its own plane or from one of its corners, it is a back face in both orders, and
    so is a triangle without area; a wall beside the camera whose normal `(-2, 0, 1)`
    has the sign of the line of sight is a front face and its reverse a back face, so the
    direction to the camera decides and not the direction it looks in; all three
    rotations of the corners agree for cameras off the axis; moving the triangle and the
    camera together changes nothing. `CullBackFaces` (the book's cube, an intensity per
    corner): straight ahead of the camera it keeps the two triangles of the near face;
    from beyond a corner three faces, from beyond the opposite corner the other three,
    from beside an edge two, always in list order; a camera in the plane of a face does
    not see that face; from inside nothing is left; the vertices and the bounding sphere
    are unchanged and the triangles left are the cube's own, indices, color and
    intensities; an object with nothing turned away, one without triangles and an empty
    one are returned equal to themselves; for 27 cameras around the cube and in it the
    faces left are exactly those whose plane the camera is beyond; the cube shrunk,
    turned 35 degrees and moved keeps its front, left and top, the same triangles as the
    cube itself culled from where the transform undone puts the camera.
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
    cos and sin, exact only to a few ulps, cannot move a pixel (section 15). The
    triangle primitive: an outline by default, and with `DrawFilledTriangle` or
    `DrawShadedTriangle` the canvas of that primitive on the projected corners, the
    intensities taken from the model's triangles. Clipping: instances in front of every
    plane of `MakeClippingPlanes` are drawn as without planes; instances behind the
    camera, at the camera and beyond each side are not drawn, and with one more in view
    the picture is that one alone; a triangle with one corner 3 in front of the camera
    and two 1 behind it is drawn as the outline of its tip, cut at `z = 1`; a triangle
    cut by the plane `x = 0` is the outline of its two pieces and lights no pixel behind
    the plane; drawn shaded, the same cut triangle is pixel for pixel the whole one with
    everything behind the plane removed, intensities included, 127 on the cut; the
    planes are in the camera's coordinates, so to a camera turned 90 degrees an instance
    along -X is in front and one along +X behind, and a camera that steps forward leaves
    an instance behind its near plane. The tests that compare pixels use planes whose
    normals are axes, where the crossing points land on whole pixels. Through a depth
    buffer, of two filled instances on the camera's axis the nearer shows in both
    list orders and nothing of the farther is seen; to a camera turned 180 degrees
    the instance at z = -1 of the world hides the one at z = -2, the depth being the
    camera's; and what the plane `x = 0` leaves of a triangle hides an instance
    behind it, in both orders. All of the above draw single triangles, which are not
    closed objects, and so with `BackFaceCulling::kOff`. Back-face culling: by default a
    triangle listed clockwise as the camera sees it is drawn and the same corners the
    other way round leave the canvas empty, and with the culling off both are drawn; a
    camera on the far side of a triangle, turned to look back at it, sees the one the
    default camera does not, and what is drawn is drawn as without culling; an instance
    turned half around its own origin shows its back, while a scale of 2 or 0.5 or a
    place off the axis turns nothing; of a cube straight ahead only the front face is
    drawn, so that painted in list order the left face no longer lies over the front and
    the picture is that of the front face alone, filled, through a depth buffer and as an
    outline, which has fewer pixels than the whole wireframe; a cube to the right of the
    camera and below it is, through a depth buffer, the picture of its front, left and
    top; a front face cut by a clipping plane is drawn as its pieces and a back face cut
    the same way not at all; and to a camera at the center of a cube, with the clipping
    planes of its viewport, the inside fills the canvas without culling and nothing is
    drawn with it. Under lights (a triangle listed clockwise as the camera sees it, its
    corners 0.26 from its center, off the pixel boundaries, drawn through a depth
    buffer): an instance scaled and placed off the axis is, for each shading model, the
    canvas of `RenderObject` under lights on its placed model; the three models light
    the same pixels differently, Phong shading alone reaching 223 in front of a lamp,
    and Phong is the default; the camera, the instance and the lamp moved together by
    one vector give the same canvas to the bit, and the lamp left behind another; to a
    camera turned 90 degrees a sun in the world's +X lights a triangle that faces it at
    191 on every pixel, the canvas of the default camera with the sun behind it, a sun
    in the world's -Z leaves the triangle dark, and a lamp a quarter of a unit in front
    of the triangle gives the default camera's picture within one level; an instance
    turned 45 degrees turns its stated normals from 45 degrees off the sun, 135, to
    straight at it, 191, and its own normal, for flat shading, from across the sun to 45
    degrees off it; a model twice as large with normals of three other lengths, placed
    at half the scale, is the same canvas; a triangle listed the other way round is not
    drawn, and with the culling off is lit by the ambient light alone, 63, where the
    front face has 191; of two instances on the axis the nearer shows in either order
    and without a buffer the later; a triangle with normals across the light at one
    corner and along it at two, cut by the plane `x = 0`, is under Phong shading the
    whole one with everything behind the plane removed, within one level, 135 on the
    cut, under Gouraud shading 135 on the cut where the whole triangle has 95, and under
    flat shading one shade over the same pixels.
  - Shading, `RenderObject` under lights (one triangle on the viewport plane that faces
    the camera, its corners on the pixels (0, 12), (12, -12) and (-12, -12) and its
    center on (0, -4)): Phong shading by default; under ambient light alone every
    shading model draws the filled triangle in half the color; under a directional
    light at the angle of the 3-4-5 triangle every model draws it in `0.525` of the
    color, one value being all a flat face lit from far away has; flat shading draws the
    filled triangle in the color times the light at the center, which is not the light
    under the lamp, and draws the same whatever normals the triangle states, while the
    other two change; Gouraud shading draws `DrawShadedTriangle` through the projected
    corners with `ComputeIllumination` at each vertex, the model's exponent included,
    three different values; with normals leaning by the 3-4-5 angle at the two lower
    corners those corners read 204 and the top more than 250 under a light from the
    camera, and normals half as long give the same canvases; a corner without a normal
    is lit as the flat face's while the corner with one leans away; Phong shading draws
    `DrawPhongTriangle` through the projected corners with their depths and normals; a
    lamp in front of the point of the pixel (4, 0), at a quarter of a unit, an eighth
    and a sixteenth, leaves that pixel at 255 under Phong shading every time and darker
    each time under the other two, below 135 under Gouraud throughout and below 50 at
    the end (the book's Figures 13-7 and 13-10); with a lamp at the camera a shiny face
    reads 204 on the axis under Phong shading and a matte one 102, under Gouraud
    shading the two differ by at most one level, and under flat shading the shiny one
    is brighter all over by what its center caught; two triangles folded along a shared
    side read 241 and 80 under flat shading, and with the surface's normals at their
    corners 180 on the fold from both sides under Gouraud and Phong shading, less than
    50 apart two pixels either side of it; the stored intensities are not read; the
    same corners the other way round are lit by the ambient light alone; three corners
    on a line, or two at one place, draw nothing, nor does an empty object; every
    triangle is lit in its own color; and through a depth buffer the nearer of two
    triangles shows under all three, where painted in list order the later one does.

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
  `objects_320x180.ppm`, `instances_320x180.ppm`, `camera_320x180.ppm`,
  `clipping_320x180.ppm`, `filled_320x180.ppm` and the three of the shading scene,
  `shading_flat_320x180.ppm`, `shading_gouraud_320x180.ppm` and
  `shading_phong_320x180.ppm`, are the rasterizer's demos (section
  9): every pixel of them comes from `DrawLine`, `DrawFilledTriangle`,
  `DrawShadedTriangle` or `DrawPhongTriangle`, the objects' through `RenderObject`, the
  instances', the camera
  scene's, the clipping scene's, the filled cubes' and the shading scene's through
  `RenderScene`, the last two
  through a depth buffer and with back-face culling as well. The 2D
  scenes compute their
  vertices with integer arithmetic and their colors and intensities without any `libm`
  call, and the projections of the cube, the objects, the instances and the filled
  cubes, with the depths of the last, are a few IEEE
  additions, multiplications and divisions, which every platform rounds the same way,
  so those pictures are reproducible bit for bit. The camera scene is the first whose
  vertices pass through `cos` and `sin`; section 15 records the margin by which its
  projected vertices clear the pixel boundaries, so that a `libm` off by a few ulps
  still draws the same pixels. The clipping scene is the second, and the first whose
  picture is filled: section 15 records its margin, and what had to be moved off the
  pixel boundaries to get one. The shading scene, though its camera is turned and two
  of its models are spheres, calls neither: its vertices come from additions,
  multiplications, divisions and square roots, all of which IEEE arithmetic rounds one
  way (section 9 says how). What differs between platforms in its three references is
  `pow`, in the specular term of the shiny ball, by an amount far inside the test's
  tolerance.
- `golden_test.cpp` renders the same scenes and compares every channel of every pixel
  with the reference image. The test passes when no channel differs by more than 1. The
  tolerance absorbs floating-point differences between compilers and standard
  libraries; everything else is exact.
- The scenes are defined once, in the `demo` layer, shared by `render_demo` and the
  golden test, so the app and the test cannot drift apart.
- The references are regenerated only when rendering behaviour changes on purpose, with
  `render_demo --scene <flat|diffuse|specular|reflective|mirrorball|lines|triangles|shaded|cube|objects|instances|camera|clipping|filled> --width 320 --height 180 --output tests/demo/golden/...`
  and `render_demo --scene shading --shading <flat|gouraud|phong> ...` for the three of
  the shading scene,
  from a Release build, and the pictures are reviewed before they are committed. The
  `flat` reference doubles as a regression test: neither the specular nor the shadow code
  path may change the output of an ambient-only scene, `diffuse` pins the shadows of
  matte surfaces independently of the highlights, and `specular` pins the output of
  non-reflective surfaces under the recursive `TraceRay`. In the same way `objects`,
  `instances` and `camera` pin the wireframes through the pipeline that now clips and
  can shade: they are byte for byte what they were before it could. `filled` pins
  hidden-surface removal: a scene whose list order painting gets wrong, drawn through
  the depth buffer and without its back faces. Every other reference is drawn without a
  depth buffer and did not change by a byte when it arrived. Back-face culling changed
  `filled` alone, on the 698 pixels where a face turned away had shown (section 15):
  `instances`, `camera` and `clipping` are drawn with the culling off and are byte for
  byte what they were. `filled` is also the one reference that would notice a triangle
  listed the wrong way round, and only in the cube; the other models are drawn without
  culling, and their winding was checked once, outside the tests (section 15). The three
  shading references pin one scene under the three shading models: the same 26216
  pixels are covered in each, and what differs is the light. They are drawn with
  culling, so they would notice a triangle of the sphere or of the floor listed the
  wrong way round. Shading changed no other reference: the fourteen from before it are
  byte for byte what they were.

## 9. Demo application

`render_demo` renders one of the demo pictures to a file:

```
render_demo [--scene reflective|specular|diffuse|flat|mirrorball|lines|triangles|shaded|cube|objects|instances|camera|clipping|filled|shading|axes] [--width N] [--height N] [--distance D] [--shading flat|gouraud|phong] [--output PATH]
```

Defaults: `reflective`, 1920 x 1080, distance 1, Phong shading, `render.png`. The `mirrorball` scene is
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
is drawn after the cube under it. Every triangle is outlined, those turned away from the
camera too: the picture is drawn with `BackFaceCulling::kOff`, and so is the next. The
`camera` scene is the chapter's closing picture:
`DrawCameraScene` stores the cube once and places two instances with a model transform
each, the first scaled to 0.75 at (-1.5, 0, 7) and the second turned 195 degrees about Y
at (1.25, 2.5, 7.5), the instances and the camera of the book's demo, and draws them with
`RenderScene` as seen from a camera at (-3, 1, 2) turned -30 degrees about Y, that is 30
degrees to its right, toward the cubes; every vertex goes through one matrix, the camera
matrix times the instance's model matrix, and then through `ProjectVertex` as before. The
`clipping` scene is the picture of the clipping chapter and the first with shaded
solids: `DrawClippingScene` draws a city of 47 instances of 8 models, seen by a camera
that stands in it. The models are a checkerboard floor of 24 x 24 tiles of side 2 from
(-12, -12) to (36, 36), boxes of four heights (a plinth of 1, a house of 2, a tower of 5,
a high-rise of 9), two pyramids (a roof of 1.5 and an obelisk of 6) and a mast of 40, all
2 x 2 at the base and standing on their origin, so that an instance is put on the ground
by its position and stays there when scaled. Every triangle has an intensity at each
corner: a shade per face (top 1, left 0.85, front 0.65, right 0.5, back 0.4, bottom 0.3),
the walls of a box darkening to 0.55 of it at the ground and the sides of a pyramid to
0.7 at the base, and a floor that fades as `1.1 - 0.024 (x + z)`, clamped to
`[0.2, 1]`. The city is a map of six rows of six lots on a grid of 4, the nearest lot
from (2, 2) to (4, 4), with the diagonal left open as an avenue and an obelisk at its
far end; a house is a box with a roof on it. The camera stands at the map's corner, at
(0, 2.5, 0), turned 42 degrees to its right, and its clipping planes are
`MakeClippingPlanes(viewport)`; the picture is drawn by
`RenderScene(..., camera, DrawShadedTriangle)`. The scene reaches through all five
planes: the floor runs on under and behind the camera, high-rises leave the picture at
the top and the plinth at the bottom, and a lantern an arm's length away, the mast
scaled to a post 2.23 high with the obelisk scaled to 0.1 as its cap, has the near
corner of its cap cut off by the near plane, so that the inside of the cap shows. One
high-rise stands behind the camera and is discarded whole by its bounding sphere.
Without the clipping step the scene could not be drawn: `ProjectVertex` cannot take a
vertex behind the camera. The scene is drawn without a depth buffer and without
back-face culling, painted in list order with every face drawn, so the picture is
composed for it (section 14, decisions 14, 15 and 16): every solid stands ahead of the camera and to
its right in the world's X and Z, each model lists its faces in an order that is right
from there (right, back, bottom, top, left, front), and the instances are listed from
the farthest row to the nearest, each row from its far end, the lantern last and a roof
after its house. The `filled` scene is the picture of hidden-surface removal:
`DrawFilledCubesScene` draws fifteen instances of the cube of the objects scene, moved
and nothing else, through the default camera and a `DepthBuffer` of the canvas's size,
with `RenderScene(..., Camera{}, DrawFilledTriangle, &depth_buffer)` and so with
back-face culling, the default. It is composed so that painting it in list order gets
it wrong, within every cube and between cubes, the opposite of the clipping scene,
which is still painted. The cube lists its faces back (red), right (green), front
(blue), left (yellow), top (purple), bottom (cyan), and every instance is listed in
that order whichever of its sides the camera sees. Nine of them form a wall of 3 x 3
facing the camera, at x and y = -3.5, 0 and 3.5 and z = 12: the same cube from nine
directions. Each shows what a cube shows from outside, three faces at most: the front
and, off the axis, the side and the top or bottom turned toward the camera, the cube
straight ahead a plain blue square. Painted, the faces listed after the front would lie
on top of it wherever they are turned away, the left as a yellow band on the cubes of
the left and middle columns, the top as a purple one on the top and middle rows, the
bottom as a cyan one on the middle and bottom rows. They are back faces, and culling
removes them before anything is drawn: of the wall's 54 faces 21 are left, three on a
corner cube, two on a cube beside the axis and one on the cube straight ahead, and
those do not overlap, a cube being convex. On each side of the wall a row of three
cubes leads away from the camera and outward, at (7, -2, 12), (12.5, -2, 18) and
(18, -2, 24) on the right and mirrored on the left, each cube partly behind the one
before, and is listed from the nearest to the farthest: painted, every cube would cover
the cube in front of it, front face over front face, which culling cannot help, and
through the buffer each is hidden where the nearer one stands. Each of the six shows
its front, its top and the side toward the wall. In all 78 of the 180 triangles are
drawn, and the picture needs both halves of the chapter: culling for the faces within
a cube, at half the work, and the depth buffer between the cubes. The ties of 3.12
along the edges of faces turned away, lines one pixel wide that the depth buffer alone
left in this picture, are gone with those faces; section 15 counts them. The `shading`
scene is the picture of the Shading chapter and the first under lights:
`DrawShadingScene(canvas, viewport, shading_model)` draws five instances of five models
with `RenderScene(..., camera, lights, shading_model, &depth_buffer)`, and
`--shading flat|gouraud|phong` chooses the shading model, Phong when it is not given.
It is one scene made to be drawn three times: every object in it is right from one
shading model on and wrong before it, so that from flat to Gouraud to Phong fewer and
fewer of them look wrong. The stage is a floor in the plane y = 0, 10 x 8 around the
world's origin, one face of two triangles. On it stand, from left to right, a blue cube
at (-3.6, 1, 2.2), the cube of the objects scene in one color; a matte green ball of
radius 1.2 at (-0.5, 1.2, 1.5); and a shiny red ball of the same size at
(2.7, 1.2, 0.6), with the specular exponent 30. The lights are an ambient light of 0.2,
a directional light of 0.25 toward (-1, 2, -1.5), a sun high up to the left and behind
the camera, and a point light of 0.65 at (1.5, 0.9, -1.9), a lamp in front of the shiny
ball and less than a unit above the floor. A ball of radius 0.14 marks where the lamp
is. Its color is (2550, 2040, 300), ten times off the scale, so that the ambient light
alone saturates its red and green: it is the same glowing yellow under every shading
model, a color that the lights cannot dim rather than a surface that emits light. The
camera stands at (0, 5.4, -7.4), in front of the stage and above it, turned about X to
look down on it by about 36.9 degrees, with the clipping planes of its viewport; the
scene is drawn through a depth buffer and with back-face culling, 322 of its 718
triangles.
Under flat shading the cube is nearly right: its top, which the lamp is below, is one
shade, and on each of the two sides that the lamp lights the two triangles come out 7
and 8 levels of 255 apart, a faint seam across the face, the one in the book's own
figure. The balls show their 288 triangles each, every one in a shade of its own, one
of the shiny ball's bright with what its center caught of a highlight. The floor is two
flat tones, (191, 183, 167) and (105, 101, 92) at 1920 x 1080, with a seam from corner
to corner. Under Gouraud shading the cube has no seams and the matte ball is round, the
light running on smoothly across its triangles' edges; both are as good as they get.
The shiny ball is round too and has lost its highlights, which are smaller than its
triangles: a dim smear is what the corners catch of them. The floor is evenly dull: the
lamp's light reaches its four corners at a grazing angle, and nothing is computed in
between. Under Phong shading the shiny ball has its two highlights, the lamp's and the
sun's, round and sharp, and the floor has a pool of light under the lamp, more than
twice as bright at its middle as the same pixels under Gouraud shading. What is left is
what no shading mends: the corners in the outlines of the balls.
The balls are made without trigonometry. `Sphere(n, color)` takes the octahedron
`|x| + |y| + |z| = n`, which has a vertex at every point of whole numbers on it and
`n^2` small triangles on each of its eight faces, and pushes every vertex out to the
unit sphere by normalizing it; a vertex is its own normal there, and a triangle is
listed clockwise as seen from outside by exchanging two of its corners if its normal
points inward. The balls have n = 6, 288 triangles on 146 vertices, and the bulb n = 4.
The camera's orientation is written out as a matrix, the rotation about X with the
cosine 4/5 and the sine 3/5 of the 3-4-5 triangle, instead of being computed from an
angle. So no vertex of this picture passes through `cos` or `sin`, and its geometry is
the same to the bit on every platform that follows IEEE 754 (section 15 has the
margins all the same). At the default distance on a 16:9 canvas the whole floor is in
view and no triangle is cut by a clipping plane, which matters to this picture: a cut
floor would be lit as its pieces (3.13), more of them than its two triangles. The
`axes` scene draws the coordinate axes and corner dots; it exercises the canvas without
ray tracing. For the ray-traced scenes, the cube, the objects, the instances, the camera
scene, the clipping scene, the filled cubes and the shading scene the viewport width
always matches the canvas aspect ratio,
and `--distance` zooms them (in the clipping scene and the shading scene it moves the
near plane out with the viewport as well); it has no effect on the 2D pictures.
`--shading` has an effect on the shading scene alone, the one picture under the
rasterizer's lights. The output format follows
the extension. After every render the program prints one line with the picture's name,
its size, the render time and the file it was saved to,
`rendered 'filled' at 1920x1080 in 16.95 ms -> filled.png`, and for a picture under
lights the shading model after its name,
`rendered 'shading' (gouraud shading) at 1920x1080 in 26.88 ms -> gouraud.png`, so that
three runs can be told apart by their lines. The time is that of the
drawing alone, from the canvas that already exists to the canvas that is not yet saved,
in milliseconds to two decimals: most of the rasterized pictures take a few
milliseconds or less, and in whole milliseconds they all took "0 ms" or "1 ms". It is
one run on a machine that is doing other things, good for telling 5 ms from 50 and not
5.0 from 5.3; section 15 says how the figures quoted there were taken. The program exits
non-zero with a message on bad arguments or I/O failure. Argument parsing is hand-written; no library
is added for it. The scene table in `render_demo.cpp` is the single list of pictures: the
usage text, the default and the dispatch are derived from it, so a new picture is a new
row. The shading models are a table of the same kind, each with its name on the command
line and a line of the usage text, and what the command line says about how a picture is
drawn, the distance and the shading model, reaches a picture as one small struct of
which each reads what applies to it.

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

- Constructor preconditions (canvas size, depth buffer size, viewport dimensions and
  distance must be positive) throw `std::invalid_argument`.
- File I/O failures throw `std::runtime_error` with the path in the message.
- The ray tracer's render loop, intersection and lighting code never throw and never
  allocate. The rasterizer's `Interpolate` returns a `std::vector`, as in the book: one
  allocation per line, a few per triangle, one per row of a shaded triangle and one more
  per row of a triangle drawn through a depth buffer, and four per row of a Phong
  triangle, for `1/z` and the three components of the normal; the
  primitives can only throw if an allocation fails. `RenderScene` builds the scene in
  the camera's coordinates as a list of models, one per instance, and clipping returns
  new lists; both allocate per instance, and neither throws otherwise. Back-face
  culling removes triangles from those lists in place and allocates nothing. Under
  lights `RenderScene` makes one more list, the lights in the camera's coordinates;
  `ComputeIllumination` allocates nothing and does not throw.
- Programming errors inside the library (a non-unit normal, a dimension mismatch that
  slipped past the constructor check, a vertex index out of range, a depth buffer of
  another size than the canvas) are `assert`ed in
  Debug builds. A clipping plane whose normal is not a unit vector, a bounding sphere
  that does not contain its model and a transform that does not scale uniformly are
  stated as preconditions and not checked; neither is what back-face culling asks of a
  model, that it is closed and lists the corners of every triangle clockwise as seen
  from outside. Under lights a normal that is not of unit length is asserted where the
  light is computed (`ComputeIllumination`), and a depth that is not in front of the
  camera where a pixel is taken back to its point (`UnprojectVertex`). That the normals
  an object states are unit vectors, or interpolated between unit vectors, is a
  precondition of `RenderObject` under lights that `TransformModel` and the clipping
  see to and nothing checks; so is that the normals of one triangle do not cancel.

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
| clipping demo (47 instances, 1626 triangles, shaded)| 1920 x 1080 | under 150 ms; about 50 ms measured               |
| clipping demo                                    | 320 x 180   | under 20 ms; about 2 ms measured                 |
| filled demo (15 instances, 78 of 180 triangles drawn, depth buffer)| 1920 x 1080 | under 60 ms; about 16 ms measured |
| shading demo, flat (5 instances, 322 of 718 triangles drawn, depth buffer) | 1920 x 1080 | under 70 ms; about 22 ms measured |
| shading demo, Gouraud                            | 1920 x 1080 | under 80 ms; about 27 ms measured                |
| shading demo, Phong (the light at 950565 pixels and more) | 1920 x 1080 | under 200 ms; about 61 ms measured      |
| shading demo, each of the three                  | 320 x 180   | under 10 ms; 1.1, 1.4 and 2.5 ms measured        |

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
  instance by `RenderScene` and applied by `TransformModel`; and clipping, in 3D between
  the transforms and the projection (which is why the projection is not a matrix):
  `Plane`, `BoundingSphere`, the clipping planes of a viewport and `ClipScene` down to
  `ClipTriangle`, with objects drawn filled or shaded from an intensity per triangle
  corner; and hidden-surface removal, both halves: the depth buffer, `DepthBuffer`,
  a `1/z` per pixel, through which the filled and the shaded triangle set only the
  pixels where they are nearest, so that a scene no longer has to be listed back to
  front, and back-face culling, `CullBackFaces`, which takes the triangles turned away
  from the camera out of every instance before it is clipped and drawn, on by default
  in `RenderScene`; and shading: the lights and the illumination equation shared with
  the ray tracer, normals at the corners of a model's triangles, and `RenderScene` and
  `RenderObject` under lights with the book's three shading models, flat, Gouraud and
  Phong, the last through `DrawPhongTriangle`, which lights every pixel. Next, in the
  book's order: textures (which need perspective-correct interpolation, and with it the
  intensities and the normals of a triangle will be interpolated over the triangle in
  space rather than over its picture, and those of a clipped triangle will stop
  depending on where it was cut). The rasterizer casts no shadows and mirrors nothing;
  the book leaves both to the ray tracer. Rotations about X and
  Z are added the way `MakeOYRotationMatrix` is, when a scene needs them; the one
  scene that tilts its camera about X writes the matrix out (section 9).
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
3. Lights are a `std::variant`, not a class hierarchy. Since the rasterizer needs them
   too they are a layer of their own (decision 17).
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
   per-vertex data the same way. `DrawLine` and `DrawFilledTriangle` ignore it. The
   normal that Phong shading interpolates was the next (decision 18).
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
    earlier call compiling; a scene type is still a step away. The clipping planes did
    not need one either: they went into the camera (decision 13). Nor did the lights:
    they are a third list beside the models and the instances (decision 18).
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
13. Clipping follows the book's pseudocode function for function, `ClipScene` down to
    `ClipTriangle` and `SignedDistance`, and the calls that fit it to this code are
    these.
    - The clipping planes are a field of `Camera`, as in the book's demo
      (`camera.clipping_planes`), not a parameter of `RenderScene`: they are given in
      the camera's coordinates and belong with it, and the signature stays as it was.
      The list is empty by default and `RenderScene` then clips nothing, so every
      picture from before the chapter is unchanged and its golden image keeps guarding
      it; a scene opts in by giving its camera planes.
    - What the pseudocode calls an instance, something with triangles and a bounding
      sphere that has already been placed, is a `Model` in the camera's coordinates, as
      it is in the book's own code, which returns a `Model` from `TransformAndClip`. So
      `Model` got the bounding sphere, and `TransformModel` is the step that places an
      instance. `RenderScene` builds the whole placed scene before it clips and draws,
      rather than taking the instances one at a time, so that `ClipScene` is called on
      a scene, as written.
    - `TransformModel` reads the scale of the bounding sphere's radius off the matrix
      instead of taking it as a second argument next to the matrix, as the book's
      `TransformAndClip(planes, model, scale, transform)` does. The number is the same,
      `radius * scale`; with one argument the two cannot disagree, and `RenderModel`,
      which is handed only a matrix, gets a correct sphere as well.
    - The default bounding sphere contains everything (an infinite radius) instead of
      being absent. The three-way test of `ClipInstanceAgainstPlane` then needs no
      special case: neither `d > r` nor `d < -r` can hold, and every triangle is
      examined. A model without a real sphere is clipped correctly, only more slowly.
    - Triangles stay indexed through clipping. A crossing point is appended to the
      instance's vertex list and named by index; a side shared by two triangles is cut
      twice, into two vertices at the same place (identical bits, since both are
      computed from the corner in front toward the corner behind), which costs a
      projection and keeps `ClipTriangle` the book's. Afterwards the vertices that no
      triangle names are removed: the book's JavaScript leaves them in and projects
      them to no effect, but here a vertex behind the camera in the list would fail
      `ProjectVertex`'s precondition.
    - `ClipTriangle` appends to a list instead of returning one, the shape of the
      book's demo code rather than of its pseudocode, which saves a small vector per
      triangle. Its last two cases are implemented; the book's demo leaves them empty.
    - A corner exactly on the plane is neither in front nor behind (3.11). The book's
      text does not say, and its code counts such a corner as behind (`d > 0` is "in"),
      which turns a triangle with one side on the plane and the third corner in front
      into a quadrilateral without area on one side.
    - The side planes of `MakeClippingPlanes` are half a pixel outside the viewport
      rather than through its sides. With pixel coordinates truncated toward zero
      (decision 8) the exact planes leave part of the canvas's bottom row and leftmost
      column unpainted and put every cut corner on the border between two pixels
      (section 15 has the measurements). The book's demo does not meet the problem: its
      planes are those of a 90 degree field of view around a viewport of 53.
    - `Plane` and `BoundingSphere` live in `math/` and `MakeClippingPlanes` in
      `camera/`, by the rule that a file belongs to the highest layer it depends on:
      the first two need only `Vec3`, the third a `Viewport`. The clipping itself needs
      `Model` and `Triangle` and is the rasterizer's.
14. Objects can be drawn filled or shaded before there is hidden-surface removal or
    lighting, because a demo of clipping is far easier to read with solid faces than
    with outlines. Two additions carry it, both the smallest that would. The triangle
    primitive is a parameter of `RenderObject` and `RenderScene`, a function pointer
    (`DrawTriangleFunction`), since the three primitives already share a signature;
    `RenderModel` and `RenderInstance`, steps of an earlier chapter that `RenderScene`
    no longer goes through, keep outlining. And the intensity `h` of 3.6 is stored per
    corner in `Triangle`, where the book's later chapters keep the other per-corner
    attributes (normals, texture coordinates), rather than per vertex: faces that share
    a vertex can then be shaded differently, and clipping interpolates it like the
    position. Until the Shading chapter computes it from lights, a model simply states
    it. Nothing was borrowed from the next chapter: without a depth buffer triangles
    are painted in list order, and a scene that wants solid faces to come out right
    must be listed back to front. The `clipping` demo is composed that way (section 9),
    which is a property of that picture, not of the renderer, and it is still drawn
    that way (decision 15).
15. Hidden-surface removal is a depth buffer that the caller makes and passes on, not
    something `RenderScene` does by itself.
    - It stores `1/z`, the book's final form, and never `z`; 3.12 has the reason. The
      comparison is the book's strict one, so of two equal depths the first drawn stays.
    - `DepthBuffer` is a class, the rasterizer's first: its size and its contents have
      to stay in step. It lives in `image/` by the rule of decision 13, that a file
      belongs to the highest layer it depends on: it needs the canvas's size and
      coordinate system and nothing of the rasterizer. That coordinate system, until
      then a private function of `Canvas`, became `PixelIndex` when the buffer became
      its second user, so the two cannot disagree about which pixels exist; with an odd
      height they would if each had its own rule.
    - The buffer is optional and off by default: a defaulted pointer at the end of the
      parameters of the triangle primitives, of `RenderObject` and of `RenderScene`.
      Every picture that existed before is drawn without one, is byte for byte what it
      was and takes as long as it did. `RenderScene` could have made a buffer for every
      scene instead; that would have changed `clipping_320x180.ppm`, a picture that goes
      through `cos` and `sin`, and with a depth buffer its shared edges become ties
      decided by the last bit (3.12), which the measurements of section 15 do not cover.
      The clipping demo therefore stays painted in the order it was composed for, and
      moving it to the depth buffer is a change of its reference, to be made on its own.
    - The buffer is the caller's, like the canvas, and not `RenderObject`'s: objects
      hide each other only through a buffer they share, so it has to outlive the call.
    - The depth reaches the primitives on the `Point`, as `inv_z`, set by
      `ProjectVertex`. The book's `RenderTriangle` reads `1/z` from the 3D vertices
      instead; here the triangle primitives draw canvas points and know nothing of 3D,
      so what they need of the third dimension has to arrive the way `h` does.
    - The filled and the shaded triangle each got the same few lines, in the shape of
      their listings, rather than a routine in common. The wireframe takes the parameter
      because the three share a signature, and ignores it.
    - Back-face culling, the other half of the book's chapter, came after the buffer and
      apart from it (decision 16). Until then the `filled` demo was drawn without it and
      showed the ties that it removes.
16. Back-face culling is a step of `RenderScene`, between the transforms and the
    clipping, and it is on unless the caller turns it off.
    - On by default, the opposite of the depth buffer (decision 15). The models of the
      book are closed and listed the right way round, a scene of such models loses
      nothing by it, and it is the cheaper half of hidden-surface removal: it takes
      about half the triangles away for one cross product and one dot product each. A
      caller who needs the back faces says so.
    - It is therefore the first addition that changes what an existing call draws, and
      the three earlier pictures that go through `RenderScene` turn it off, each for a
      reason of its own: `instances` and `camera` are wireframes that show every edge,
      the pictures of their chapters, and `clipping` shows the inside of the lantern's
      cap through the cut the near plane makes, which is made of back faces. All three
      are byte for byte what they were. `filled`, the picture of this chapter, is the
      one that changed.
    - The switch is a scoped enumeration, `BackFaceCulling::kOff` and `kOn`, and not a
      `bool`. It is the eighth parameter of `RenderScene` and follows a pointer:
      `..., nullptr, false)` says nothing at the call, and
      `..., nullptr, BackFaceCulling::kOff)` says what it does.
    - The test is the book's, with the book's formulas and the book's `<=`, in
      `IsBackFacing`; the normal is computed from the corners where it is needed, by a
      helper that was private to `culling.cpp` until shading asked for normals too, and
      is `TriangleNormal` in `math/` since (decision 18).
    - `CullBackFaces(object, camera_position)` is the book's
      `CullBackFaces(object, camera)` and takes a position rather than a `Camera`: the
      position is all the test asks for, and it is in whatever coordinates the object
      is in. `RenderScene` calls it in the camera's coordinates and passes the origin,
      which is simpler than taking the camera into every model's coordinates.
    - Before the clipping. The book's text does not say where the step goes, and its
      demo tests each triangle as it is about to be drawn, after the clipping. Both
      orders remove the same triangles, since the pieces of a clipped triangle keep its
      winding and its plane (3.11). Before, the back faces are not clipped at all, and
      the test reads the model's own vertices rather than crossing points, so all the
      pieces of a triangle share one verdict by construction.
    - `CullBackFaces` removes triangles and nothing else, as the book's listing does. The
      vertices that only back faces named stay in the list and are projected to no
      effect when the instance is not clipped; they are in front of the camera wherever
      the instance is (3.12), and removing them would cost about what projecting them
      does.
    - The winding rule is the book's, clockwise as seen from outside, and a model is
      trusted to follow it; a check would need the whole mesh and an assumption that
      it is closed (3.12). The rule was not needed before this step and one model did
      not follow it (section 15).
17. The lights and the illumination equation are a layer of their own, `lighting/`, below
    both renderers.
    - They were the ray tracer's: the light types in `raytracer/scene.h`, and the two
      terms of the equation private to `raytracer.cpp`. The rasterizer's Shading chapter
      lights its triangles with the same lights by the same equation, and the two
      renderers do not know of each other, so what both need moved below both. One
      definition per fact: a second set of light types, or a second copy of the
      formulas, would be free to drift from the first.
    - A layer rather than two more files in `math/`, where the rule of decision 13 would
      put them, since they need nothing but `Vec3`. `math/` is vector algebra and plain
      geometry; a light with an intensity, and the rule for how much of it a surface
      sends on, are neither, and a folder of their own says what they are. It sits
      beside `camera/`, the other layer the two renderers share.
    - What moved is the two terms, not the sum. `ComputeLighting` keeps its loop over
      the lights, because that loop is where the ray tracer's shadows are decided: a
      light that a sphere blocks is left out of it.
    - The terms are inline functions in the header. They were in an anonymous namespace
      next to their only caller and compiled into its loop, and they still are: the
      ray-traced pictures are byte for byte what they were and take as long (section
      15).
18. Shading follows the book's chapter and its demo, `RenderScene` down to
    `ComputeIllumination` and `UnProjectVertex`, and these are the calls that fit it to
    this code.
    - Drawing under lights is added beside drawing with a triangle primitive, not put
      in its place: a second `RenderScene` and a second `RenderObject` that take the
      lights and a `ShadingModel` where the first take a `DrawTriangleFunction`. The
      wireframes, the plain colors and the stated intensities of the earlier chapters
      are still what the first two draw, and every earlier picture is byte for byte
      what it was. The lights are a list in the parameters, the book's
      `RenderScene(camera, instances, lights)`, like the models and the instances; a
      scene type was not needed for them (decision 11).
    - The shading model is a parameter, a scoped enumeration, where the book's demo has
      a global `ShadingModel`. Its default is Phong, the demo's default and the one
      model that is right for every surface.
    - The three models are three calls of three primitives, not three branches inside
      one fill loop as in the demo's `RenderTriangle`: flat shading is
      `DrawFilledTriangle` in the color times the light, Gouraud shading is
      `DrawShadedTriangle` with the light at the corners as its intensities, which is
      what the chapter's text says to do, and Phong shading is a fourth primitive,
      `DrawPhongTriangle`. The first two were there and are used as they are.
    - `DrawPhongTriangle` lives in `triangle.h` with the other three and shares their
      helpers. It is the first primitive that needs more than canvas points: the
      viewport, to take a pixel back to the point it shows, and the lights. What it
      needs of each vertex still arrives on the `Point`, the normal joining `h` and
      `inv_z` there (decision 9). Its five quantities are interpolated along the sides
      by `EdgeInterpolate`, the helper the book's demo introduces in this chapter; the
      two older primitives keep their lists written out, in the shape of their listings
      (decision 15).
    - The light is computed in the camera's coordinates, as in the demo. The lights are
      taken there once per picture by `TransformLight`; the demo multiplies every light
      by the camera matrix inside `ComputeIllumination`, once per light for every point
      it lights, which under Phong shading is every pixel.
    - The view vector is `-P`, from the point to the camera at the origin. The
      chapter's text prints `V = P - C`, from the camera to the point; the equation it
      is put into takes `V` from the point toward the viewer, as the Light chapter
      defines it and as the ray tracer passes it (3.4), and with the sign of the text
      `<R, V>` would be negative wherever a highlight belongs. The demo computes
      `camera.position - vertex`, the right way round, with a camera position in the
      world's coordinates and a vertex in the camera's.
    - Normals are stated per corner of a triangle, the book's model format, and a
      corner that states none has the triangle's own. The demo has a global switch
      instead, `UseVertexNormals`, for all models at once; per corner, one scene holds a
      cube that says nothing and a ball that says everything, and the choice is the
      model's. "None" is the zero vector rather than a `std::optional`: clipping and
      `TransformModel` then need no case for it, a zero vector interpolates and
      transforms to a zero vector.
    - Flat shading uses the triangle's own normal whatever the model states. The demo,
      with vertex normals on, uses the normal of whichever corner sorts lowest on the
      canvas. One value per triangle wants the one normal the triangle has.
    - `TransformModel` returns the normals at unit length, and after that nothing
      normalizes them until the light is computed: the clipping interpolates them as
      they are and leaves the result as short as it comes out, and Phong shading
      interpolates them as they are. Normalizing the normal of a corner that the
      clipping made would be the same direction at that corner and other directions
      inside the piece, since interpolating between two unit vectors is not
      interpolating along the chord between them: the first version of this code did
      that, and in the unit test that cuts a triangle whose normals are 90 degrees
      apart the pieces came out up to six levels of 255 off the whole triangle, where
      they are within one now. The price is a precondition on `RenderObject` under
      lights, unit normals, that only a caller who bypasses `TransformModel` can break.
    - The specular exponent is a field of `Model`, as it is a field of the ray tracer's
      `Sphere`; the demo uses 50 for everything. Per model and not per triangle, where
      the color is: one finish per object is what the pictures need, and a field per
      triangle would be copied through every stage of the pipeline.
    - `TriangleNormal` moved from `culling.cpp` to `math/` when shading became its
      second user (decision 16), and `Overloaded`, the helper for visiting a light,
      from `raytracer.cpp` to `light.h` for the same reason.
    - The rasterizer's `RenderModel` and `RenderInstance` still outline and take no
      lights, steps of an earlier chapter (decision 14).
    - No shadows: the book's rasterizer has none, and `ComputeIllumination` has no scene
      to ask. The `shading` demo is composed without them in mind (section 9).

## 15. Implementation notes

Measurements and known behaviours of the implementation, recorded 2026-11-02, updated
2026-11-03 with shadows and reflections, 2026-11-04 with the rasterizer's lines and
filled triangles and the reorganisation into layers, 2026-11-05 with shaded triangles,
the perspective projection and objects made of triangles, 2026-11-09 with models,
instances and scenes and, the same day, with model and camera transforms,
2026-11-16 with clipping and shaded objects, 2026-11-20 with back-face culling, and
2026-11-24 with the lighting layer and with shading.

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
  X over a purple one. Nothing hides in a wireframe, with or without a depth buffer.
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
- `RenderScene` now places every instance with `TransformModel`, clips the list and
  draws what is left, instead of calling `RenderModel` per instance. With a camera that
  has no clipping planes the vertices that reach `RenderObject` are the same doubles as
  before, and `objects_320x180.ppm`, `instances_320x180.ppm` and `camera_320x180.ppm`
  are byte for byte unchanged.
- The clipping demo has 47 instances of 8 models and 1626 triangles, 1152 of them the
  floor's. At 320 x 180 clipping leaves 46 instances and 1149 triangles: the high-rise
  behind the camera is discarded by its bounding sphere against the near plane, without
  a look at its triangles; 33 instances come back as they went in; 13 are cut, among
  them the floor, from 1152 triangles on 625 vertices to 664 on 460, and the lantern's
  cap, from 6 triangles to 10. The picture draws in about 50 ms at 1920 x 1080 and 2 ms
  at 320 x 180. The transforms and the clipping together take about 0.06 ms of that at
  either size; the rest is `DrawShadedTriangle`, which paints every covered pixel 3.7
  times on average, since nothing is hidden before it is drawn.
- With the side planes exactly at the viewport's sides, as first written, the clipping
  demo at 320 x 180 left 46 of the 320 pixels of its bottom row unpainted, where the row
  above had 1, and 150 of the 180 of its leftmost column, where the next had 106. A
  coordinate is truncated toward zero, so the bottom row is reached only by what
  projects to y <= -90 and the leftmost column by x <= -160, which is on or beyond the
  viewport's side; a corner cut exactly there lands at -90 give or take the last bit,
  and which it was changed from corner to corner and with the camera's angle (181 and
  127 at 45 degrees). Half a pixel further out every such corner projects to -90.5 or
  -160.5 and truncates to the outermost row or column, and the gaps are gone.
- The clipping picture went through `cos` and `sin` like the camera picture, and at
  first one ulp of difference in the camera's angle changed ten thousand of its 57600
  pixels. Three things sat exactly on pixel boundaries. The corners cut by the side
  planes, as above. The city itself: with the camera at the corner of an integer grid
  and turned 45 degrees, a corner (x, z) projects to 180 (x - z) / (x + z) pixels across,
  a whole number for many small integers. And the lantern: the floor of its cap, 0.3
  below the eye and cut at z = 1, projected to row -54 exactly. The camera is therefore
  turned 42 degrees, not 45, and the lantern's post is 2.23 high, not 2.2. As it stands,
  no projected coordinate of the clipped scene's 3447 triangle corners comes closer
  than 6.0e-4 pixels to a boundary at 320 x 180, rounding noise being about 1e-13; one
  ulp in the angle changes no pixel, and an error of up to 1e-4 degrees changes at most
  four, none by more than one level in any channel, inside the golden test's tolerance.
  A new filled scene should be checked the same way: exact coincidences are the rule,
  not the exception, when round numbers meet a symmetric view.
- That the clipping demo's painting order is its depth order was checked once, outside
  the repository: the clipped triangles were dumped in drawing order and rasterized
  twice by a separate script, once keeping the last triangle on every sample and once
  the nearest, for eight canvas sizes and viewport distances (320 x 180 to 1920 x 1080,
  distances 0.3 to 2, wide and tall). The two agreed on every covered sample; with the
  list reversed they disagreed on four out of five. The argument that makes it so is in
  section 9; nothing in the renderer enforces it.
- The filled demo (15 instances of one model, 180 triangles) drew in about 20 ms at
  1920 x 1080 through its depth buffer while every triangle was drawn, and in 7 to 8 ms
  painted without one: every pixel of every triangle is compared before it is set. With
  back-face culling it draws in about 16 ms (below). It has no rotation and the
  default camera, so no `libm`: like the instances demo its vertices are exact sums and
  its projections and depths a few IEEE operations. Its coordinates are still kept off
  the pixel boundaries, which is why the wall stands at z = 12 rather than at 10 (a face
  at z = 9 projects a whole x to a whole pixel at 320 x 180): no projected coordinate of
  its 120 vertices comes closer than 0.043 pixels to a boundary there.
- What the depth buffer does to the filled demo was measured once, outside the
  repository, at 320 x 180. A script with a replica of `ProjectVertex` and
  `DrawFilledTriangle` painted the scene the way a painter would if the order were
  right: only the faces turned toward the camera, and the cubes of each row from the
  farthest to the nearest. Painted in list order, as the scene is listed, the picture
  differs from that one on 6338 of the 16840 pixels the cubes cover, 38%. Through the
  depth buffer it differs on 1311, every one of them on an edge that two faces share,
  where the two pictures settle a tie differently: the painter gives the edge to the
  face drawn later, the depth buffer to the face that is nearer by the last bit of a
  running sum. On 698 of those the face that shows is one turned away from the camera,
  the lines of 3.12: the red of a back face along the far edge of a side, the green of
  a hidden right face down the right edge of a front. They are deterministic, the
  arithmetic being what it is, and back-face culling removed them (below).
- Back-face culling leaves 78 of the filled demo's 180 triangles to draw: 21 of the
  wall's 54 faces (three on each corner cube, two on each cube beside the axis, one on
  the cube straight ahead) and 18 of the rows' 36 (the front, the top and the side
  toward the wall of each cube). The classification is exact there: the cubes are
  moved by whole and half numbers, the default camera is the origin, and the camera is
  at least 1 away from the plane of every face, so no `<N, V>` is near zero.
- Culling changed `filled_320x180.ppm` on 698 pixels, exactly the 698 of the note above:
  a pixel can change only where the face that showed was one that is no longer drawn,
  since taking away triangles that lose a pixel does not change who wins it. On 695 of
  them a face turned toward the camera shows now. The other 3 show the background:
  at the far corner of three wall cubes the back or the bottom face reached one pixel
  beyond the sides in front of it, a row that the running sums leave short (the apex
  rows above), and nothing else covers that pixel. The cubes cover 16837 pixels instead
  of 16840. At 1920 x 1080 the two pictures differ on 4092 pixels of 2073600.
- What culling saves was measured on 2026-11-20 by a benchmark outside the repository:
  the demo source compiled with its culling switch as a variable, each picture drawn
  150 to 3000 times, alternately with and without, into the same canvas, Release, one
  pinned core; the figures are medians and the minima are within 2% of them. The
  filled demo at 1920 x 1080 takes 12.7 ms without culling and 8.2 ms with it, 1.54
  times as fast; 2.6 ms of either are clearing the canvas and making the depth buffer,
  which culling does not touch, so the drawing itself falls from 10.1 ms to 5.6, by
  44%. At 320 x 180 it is 0.44 ms against 0.26, 1.7 times as fast. That is what the
  geometry promises: the back faces of a convex object cover the same pixels as its
  front faces, so about half of the pixel work goes, for a test per triangle that does
  not show in the timings. A single `render_demo` run at 1920 x 1080, which also pays
  for touching the canvas and the buffer for the first time, printed 20 to 22 ms before
  culling and prints 15 to 17 ms with it.
- The clipping demo is drawn without culling (decision 16). In the same benchmark, with
  culling turned on, it took 28.5 ms instead of 48.5 at 1920 x 1080 and 1.31 ms instead
  of 2.13 at 320 x 180, 1.7 and 1.6 times as fast: 849 triangles reach the primitive
  instead of 1144 (854 instead of 1149 at 320 x 180). None of the floor's are among the
  ones removed, every one of them facing up at a camera above it; of the solids' about
  three in five are. The two pictures differ on 475 of the 57600 pixels at 320 x 180:
  471 in one piece, the cut in the lantern's cap, through which the high-rise behind it
  and the top of its own post show instead of the cap's inside, and 4 single pixels at
  corners of solids, where a face turned away reached one pixel beyond the faces in
  front of it, as in the filled demo.
- The winding of the demo models was checked on 2026-11-20, in the same scratch
  program: for every triangle of every solid, whether its normal `(B - A) x (C - A)`
  points away from the middle of the solid, and for the floor's, whether it points up.
  The cube, the shaded box at its five heights, the shaded pyramid at its two and the
  floor follow the rule. The pyramid of the objects and instances pictures did not: its
  base was right and its four sides were listed counter-clockwise as seen from
  outside. They are listed clockwise now. An outline does not depend on the order of
  its corners (`DrawLine` draws the same pixels in either direction), so no picture
  changed. Nothing in the test suite pins this for any model but the cube, whose
  winding the `filled` reference depends on: the others are drawn without culling.
- Back-face culling changed one reference, `filled`. The other thirteen are byte for
  byte what they were, and so are the 1920 x 1080 renders of the lines, triangles,
  shaded, cube, objects, instances, camera and clipping demos, compared with those of
  the build before it.
- The depth buffer changed no picture that is drawn without one: the thirteen other
  references, and the 1920 x 1080 renders of the triangles, shaded, objects, instances,
  camera and clipping demos, are byte for byte what they were. Without a buffer the
  primitives still interpolate `1/z` along the sides, a few values per triangle, and
  skip it along the rows, so the timings did not move either. Interpolating the rows
  regardless would have cost the clipping demo about 30%, 64 ms against 49.
- A triangle cut by a plane is not, in general, pixel for pixel the part of the whole
  triangle in front of the plane. Its new corners are truncated to whole pixels like any
  vertex, so the sides that run to them are redrawn from there and can differ by a pixel
  from the same sides of the whole triangle; and its intensities are taken along the
  sides in 3D at the cut and interpolated along the canvas inside each piece, where the
  whole triangle interpolates along the canvas throughout, which is not the same thing
  under perspective. The unit test that does compare the two pixel for pixel uses a
  triangle parallel to the viewport whose pieces have every side through whole pixels.
  Perspective-correct interpolation, due with textures, removes the second difference.
- Neighbouring triangles stay joined through clipping: the two that share a cut side get
  the same crossing point bit for bit, so no gap opens along a cut. The price is the
  duplicate vertex, one per triangle and cut side, each projected on its own.
- On 2026-11-04 the tree was reorganised into the layers of 4.2 (`CLAUDE.md` has the
  rules): the rasterizer was split into one file per primitive, the demo pictures moved
  into their own library target with the `axes` picture joining them from the app, the
  rasterizer tests were split to match, and `render_demo` became table-driven. The
  refactor changed no output: all eight pictures at 320 x 180 and four of them at
  321 x 181, a zoomed render, the usage text and two error messages were compared byte
  for byte before and after.
- On 2026-11-24 the light types and the two terms of the illumination equation moved out
  of the ray tracer into the `lighting/` layer (decision 17). The refactor changed no
  output: the fifteen pictures at 640 x 360 and at 321 x 181 and the fourteen references
  were compared byte for byte before and after. Nor did it cost time: the reflective
  scene at 1920 x 1080, run alternately from the build before and the build after,
  printed 249 to 264 ms before and 241 to 248 ms after, the same within the noise of a
  single run.
- The shading demo has 5 instances of 5 models and 718 triangles: 2 in the floor, 12 in
  the cube, 288 in each ball and 128 in the bulb. Back-face culling leaves 322, and at
  the default distance on a 16:9 canvas the clipping cuts none of them: the bounding
  spheres of the floor and of the cube reach across a plane, which costs a look at their
  triangles and no more. It covers 26216 of the 57600 pixels at 320 x 180 and 950565 of
  2073600 at 1920 x 1080, the same pixels under each shading model.
- Render times of the shading demo, as `render_demo` prints them, nine runs of each on
  2026-11-24, Release, the smallest, the median and the largest: at 1920 x 1080 flat
  shading 20.6, 22.4 and 24.5 ms, Gouraud 25.1, 26.9 and 28.1 ms, Phong 59.6, 61.1 and
  69.0 ms; at 320 x 180 flat 1.04, 1.12 and 1.22 ms, Gouraud 1.28, 1.38 and 1.44 ms,
  Phong 2.35, 2.52 and 2.68 ms. Flat and Gouraud shading compute the light 322 and 966
  times a picture and differ by what `DrawShadedTriangle` costs over
  `DrawFilledTriangle`, a list of intensities per row. Phong shading computes it at
  every pixel it sets, three lights and for the shiny ball up to two `pow` calls each
  time, and allocates four lists per row: about 39 ms more than flat shading for the
  950565 pixels covered, some 40 ns a pixel. Much of what is left of the flat figure is not shading:
  clearing the canvas and making a depth buffer of 16.6 MB, as in the filled demo.
- The three shading references differ from one another on nearly every covered pixel,
  if often by little: flat and Gouraud on 25475 of the 26216, by more than 8 levels on
  21829; Gouraud and Phong on 24457, by more than 8 levels on 14995, the pool of light
  on the floor and the highlights.
- The shading demo's geometry calls neither `cos` nor `sin` (section 9), so the margins
  that the camera and clipping demos needed are not what keeps it reproducible. They
  were measured all the same, in a scratch program that included the demo source: no
  projected coordinate of the 322 triangles drawn comes closer to a pixel boundary than
  7.6e-4 pixels at 320 x 180 and 3.2e-3 at 1920 x 1080, and the nearest vertex is 5.96
  in front of the camera. The same program checked the winding: every triangle of the
  balls, the bulb and the cube has its normal pointing away from the model's middle, the
  floor's point up, no triangle is without area, and every stated normal is the unit
  vector to its vertex.
- A ball of the shading demo is the octahedron `|x| + |y| + |z| = 6` normalized, not the
  book's sphere, which stacks rings of vertices by `cos` and `sin` and has a ring of
  coincident vertices and triangles without area at each pole. Lit triangles without
  area are skipped (3.13), so the book's sphere would draw as well.
- The cube of the shading demo is "nearly right" under flat shading by a number: at
  1920 x 1080 its top is one color, (26, 53, 88), its front (48, 96, 159) and
  (46, 92, 152) and its right side (47, 94, 155) and (44, 89, 147), the two triangles of
  a face 7 and 8 levels apart in blue. The lamp is about 6 units from those faces;
  nearer, the seam grows.
- Shading changed no picture that is drawn without lights: the fourteen earlier
  references and the fifteen pictures at 640 x 360 and at 321 x 181 are byte for byte
  what they were, compared with renders of the build before it. `TransformModel` now
  walks the normals of every triangle of every instance, which for those pictures are
  all zero and stay so, and a `Point` is 24 bytes larger; neither shows in the timings
  (the clipping demo 55 ms against 54, the filled demo 18.7 against 19, medians of nine
  single runs at 1920 x 1080).
- The `.clang-format` file is in place and clang-format 18 is now installed on the
  development machine, but the sources were formatted by hand to its rules and
  `clang-format --dry-run` still reports a handful of differences in older files. A
  reformat is a separate, mechanical change; new code follows the surrounding style.
