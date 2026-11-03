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
directional lights, diffuse shading, specular highlights, shadows and reflections;
section 13 lists what comes next.

## 2. Goals and non-goals

Goals:

- G1. Behaviour pinned by tests: unit tests for every module and golden-image tests that
  fix the renderer's output pixel for pixel.
- G2. A structure that accommodates the planned next steps (a movable camera, and later
  a rasterizer) without rewrites.
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

### 3.6 Numeric conventions

- All geometry and color math uses `double`. Switching to `float` would be a deliberate
  decision rather than a detail, because it changes the golden images.
- Colors are `double` in the nominal range 0..255 until they reach the canvas.
- There is no display. The canvas is saved to a file and viewed with any image viewer.

## 4. Architecture

### 4.1 Repository layout

```
CMakeLists.txt            top-level project: library, apps, tests
CMakePresets.json         dev / release configure, build and test presets
.clang-format
README.md                 build instructions and project overview
docs/SPEC.md              this document
include/cgfs/             public headers, one per module
    vec3.h  color.h  canvas.h  image_io.h  viewport.h  scene.h  raytracer.h
    demo_scenes.h         the demo scenes shared by render_demo and the golden tests
src/                      implementation files matching the headers
apps/
    render_demo.cpp       command-line renderer for the demo scenes
tests/
    CMakeLists.txt
    test_helpers.h        Catch2 printers for Vec3/Color/Rgb8, a temp-file helper
    vec3_test.cpp  color_test.cpp  canvas_test.cpp  image_io_test.cpp
    viewport_test.cpp  raytracer_test.cpp  golden_test.cpp
    golden/               reference renders of the demo scenes (PPM)
third_party/
    CMakeLists.txt
    stb/stb_image_write.h single-header PNG writer (public domain), vendored
    stb/stb_image_write.cpp  the one translation unit with the implementation
```

`build/` is the out-of-source build directory and is git-ignored. In-source builds are
rejected by the top-level `CMakeLists.txt`.

### 4.2 Modules and dependency order

Everything lives in namespace `cgfs`. Dependencies point only to the left:

```
vec3, color  <-  canvas  <-  viewport  <-  raytracer
                  ^                          ^
              image_io                     scene
```

- `vec3`, `color`: value types and free functions, header-only, `constexpr` where possible.
- `canvas`: the pixel buffer and centered coordinate system.
- `image_io`: PPM writer and reader, PNG writer. Used by `Canvas::Save` and the tests.
- `viewport`: pixel to viewport-point mapping and field of view.
- `scene`: plain data: spheres, lights, background color.
- `raytracer`: intersection, lighting, the render loop.
- `demo_scenes`: the demo scenes as functions, so the app and the golden tests render
  exactly the same thing.

The canvas knows nothing about 3D. The viewport knows nothing about spheres. The tracer
writes to the canvas only through `PutPixel`. A future rasterizer reuses `canvas`,
`vec3`, `color` and `image_io` unchanged.

### 4.3 Targets

| Target            | Kind               | Contents                                                                          |
|-------------------|--------------------|-----------------------------------------------------------------------------------|
| `cgfs`            | static library     | everything under `src/` and `include/`                                            |
| `cgfs::cgfs`      | alias              | the name apps and tests link against                                              |
| `stb_image_write` | static library     | the stb implementation, built without the project's warning flags; system include |
| `render_demo`     | executable         | `apps/render_demo.cpp`                                                            |
| `cgfs_tests`      | executable (CTest) | all `tests/*_test.cpp`                                                            |

`BUILD_SHARED_LIBS` is respected but not used by default.

## 5. Public API

Signatures below are the intended shape. Names, parameter order and semantics are
normative; exact qualifiers and overload sets may be adjusted during implementation.

### 5.1 `vec3.h`

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

No matrices yet. A `Mat4` arrives with the camera-transform step (section 13).

### 5.2 `color.h`

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

### 5.3 `canvas.h`

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

### 5.4 `image_io.h`

```cpp
struct Image { int width = 0, height = 0; std::vector<std::uint8_t> rgb; };
void  WritePpm(const std::filesystem::path&, int width, int height, std::span<const std::uint8_t> rgb);
void  WritePng(const std::filesystem::path&, int width, int height, std::span<const std::uint8_t> rgb);
Image ReadPpm(const std::filesystem::path&);   // binary P6 with maxval 255; used by golden tests
```

### 5.5 `viewport.h`

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

### 5.6 `scene.h`

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

### 5.7 `raytracer.h`

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

All tests live in `tests/` and run through CTest. They must pass in both presets.

Unit tests:

- `vec3`: arithmetic, `Dot`, `Cross`, `Length`, `Normalized`, `Reflect` (a 45-degree
  vector mirrors to the other side, the normal maps to itself, a grazing vector reverses).
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

Golden-image tests:

- `tests/golden/` holds 320 x 180 renders of the five demo scenes (flat ambient, the
  diffuse scene with the yellow floor sphere, the specular scene with the same geometry
  and lights but shiny surfaces, the reflective scene with the same again plus a
  reflectivity per sphere, and the mirror ball scene: fourteen spheres of mixed finishes
  under two point lights, which exercises every feature at once), made with the
  aspect-matched viewport and stored as binary PPM. They are the reference output of the
  renderer and are committed. The diffuse, specular, reflective and mirror ball
  references show the shadows the spheres cast on the floor and on each other; the flat
  reference has no shadows because ambient light is never blocked; only the reflective
  and mirror ball references show reflections.
- `golden_test.cpp` renders the same scenes and compares every channel of every pixel
  with the reference image. The test passes when no channel differs by more than 1. The
  tolerance absorbs floating-point differences between compilers and standard
  libraries; everything else is exact.
- The scenes are defined once, in `demo_scenes`, shared by `render_demo` and the
  golden test, so the app and the test cannot drift apart.
- The references are regenerated only when rendering behaviour changes on purpose, with
  `render_demo --scene <flat|diffuse|specular|reflective|mirrorball> --width 320 --height 180 --output tests/golden/...`
  from a Release build, and the pictures are reviewed before they are committed. The
  `flat` reference doubles as a regression test: neither the specular nor the shadow code
  path may change the output of an ambient-only scene, `diffuse` pins the shadows of
  matte surfaces independently of the highlights, and `specular` pins the output of
  non-reflective surfaces under the recursive `TraceRay`.

## 9. Demo application

`render_demo` renders one of the demo pictures to a file:

```
render_demo [--scene reflective|specular|diffuse|flat|mirrorball|axes] [--width N] [--height N] [--distance D] [--output PATH]
```

Defaults: `reflective`, 1920 x 1080, distance 1, `render.png`. The `mirrorball` scene is
the project's own composition, built from the same features as the book's scene: a
chrome ball ringed by six colored spheres of different finishes on a dark polished floor,
marbles in front, spheres floating above, two point lights for crossing shadows and a
dusk-blue sky. The `axes` scene draws the coordinate axes and corner dots; it exercises
the canvas without ray tracing. The
viewport width always matches the canvas aspect ratio. The output format follows the
extension. The program prints the render time in milliseconds and exits non-zero with a
message on bad arguments or I/O failure. Argument parsing is hand-written; no library is
added for it.

## 10. Code style and conventions

- Naming: types `PascalCase`; functions and methods `PascalCase` (this matches the
  textbook's listings: `PutPixel`, `TraceRay`); variables and parameters `snake_case`;
  private data members `snake_case_`; constants `kPascalCase`; trivial accessors
  `width()`-style lowercase.
- Formatting is enforced by `.clang-format`: `BasedOnStyle: Google`, `ColumnLimit: 100`,
  `IndentWidth: 4`. No formatting debates in review; run the tool.
- Headers use `#pragma once` and live under `include/cgfs/`; one module per header.
  Headers include what they use and nothing more.
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
- The render loop, intersection and lighting code never throw and never allocate.
- Programming errors inside the library (a non-unit normal, a dimension mismatch that
  slipped past the constructor check) are `assert`ed in Debug builds.

## 12. Performance

Targets apply to Release builds on a single core. Debug builds have no targets.

| Scene                               | Size        | Target                                           |
|-------------------------------------|-------------|--------------------------------------------------|
| reflective demo (4 spheres, 3 lights, 3 bounces) | 1920 x 1080 | under 1 second; around 250 ms is the expectation |
| reflective demo                                  | 320 x 180   | under 50 ms, so the golden test stays cheap      |

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

- Arbitrary camera: `Mat4` (or a rotation matrix plus position) in a `mat.h` header
  next to `vec3.h`; `Camera` struct in `scene.h`; `Render` transforms `D` before tracing.
- Row-parallel rendering with `std::thread` or a parallel algorithm, with golden tests
  proving the output is unchanged.
- Rasterizer: new modules `rasterizer.h` (lines, triangles, shaded triangles),
  `model.h` (vertices, triangles, instances), projection, clipping, depth buffer,
  shading and textures. It reuses `canvas`, `vec3`, `color` and `image_io` as they are.
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

## 15. Implementation notes

Measurements and known behaviours of the implementation, recorded 2026-11-02 and
updated 2026-11-03 with shadows and reflections.

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
- The `.clang-format` file is in place and clang-format 18 is now installed on the
  development machine, but the sources were formatted by hand to its rules and
  `clang-format --dry-run` still reports a handful of differences in older files. A
  reformat is a separate, mechanical change; new code follows the surrounding style.
