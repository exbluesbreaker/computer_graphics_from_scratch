# Specification

Software 3D renderer written from scratch in C++.

## 1. Purpose

This document is the design reference for the renderer: what it renders and with which
formulas, how the code is structured, how it is built and tested, and how it is expected
to grow. The current feature set is a ray tracer for spheres with ambient, point and
directional diffuse lighting; section 13 lists what comes next.

## 2. Goals and non-goals

Goals:

- G1. Behaviour pinned by tests: unit tests for every module and golden-image tests that
  fix the renderer's output pixel for pixel.
- G2. A structure that accommodates the planned next steps (specular highlights,
  shadows, reflections, a movable camera, and later a rasterizer) without rewrites.
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
  `D = CanvasToViewport(x, y)` with `t_min = 1` and `t_max = +inf`, and writes the
  returned color with `PutPixel`.
- `TraceRay(O, D, t_min, t_max)` tests every sphere, keeps the smallest root `t` with
  `t_min <= t <= t_max` across all spheres and both roots, and returns the scene's
  background color when nothing is hit. Infinite roots never count as hits.
- On a hit, `P = O + t * D`, `N = (P - C) / |P - C|`, and the result is
  `sphere.color * ComputeLighting(P, N)`. The product may exceed 255 per channel; it is
  clamped only when written to the canvas.
- Pixel order during rendering is an implementation detail. The output must not depend
  on it.

### 3.5 Lighting

Three light types, each with an `intensity`:

- Ambient: adds `intensity` to every point.
- Point: has a `position`; the light direction at `P` is `L = position - P`.
- Directional: has a `direction` that points toward the light; `L = direction`.

`ComputeLighting(P, N)` returns

`i = sum(ambient I) + sum over point and directional lights of I * <N, L> / (|N| |L|)`

where terms with `<N, L> <= 0` are skipped. `N` is unit length by construction; the
division by `|N|` is kept so the code reads like the formula.

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
struct Sphere { Vec3 center; double radius = 1.0; Color color; };

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
the light type. `Sphere` will gain `specular` and `reflective` fields in later steps;
aggregate initialization with designated initializers keeps existing scene code
compiling.

### 5.7 `raytracer.h`

```cpp
inline constexpr double kInfinity = std::numeric_limits<double>::infinity();

struct SphereIntersection { double t1 = kInfinity, t2 = kInfinity; };
SphereIntersection IntersectRaySphere(Vec3 origin, Vec3 direction, const Sphere& sphere);

class RayTracer {
public:
    RayTracer(const Scene& scene, const Viewport& viewport);   // stores references
    void   Render(Canvas& canvas) const;
    // precondition: canvas dimensions equal the viewport's; violation throws std::invalid_argument
    Color  TraceRay(Vec3 origin, Vec3 direction, double t_min, double t_max) const;
    double ComputeLighting(Vec3 point, Vec3 normal) const;     // normal must be unit length
};
```

The scene and viewport must outlive the tracer. `TraceRay` and `ComputeLighting` are
public so the tests can exercise them directly.

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

- `vec3`: arithmetic, `Dot`, `Cross`, `Length`, `Normalized`.
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
  - `ComputeLighting`: ambient only; a point light straight above a point lit at full
    intensity; a light behind the surface contributes nothing; the result does not
    depend on the length of `L`.
  - `TraceRay`: a single sphere with full ambient light returns its color on the axis
    and the background color for a ray that misses.

Golden-image tests:

- `tests/golden/` holds 320 x 180 renders of the two demo scenes (flat ambient, and the
  diffuse scene with the yellow floor sphere), made with the aspect-matched viewport and
  stored as binary PPM. They are the reference output of the renderer and are committed.
- `golden_test.cpp` renders the same scenes and compares every channel of every pixel
  with the reference image. The test passes when no channel differs by more than 1. The
  tolerance absorbs floating-point differences between compilers and standard
  libraries; everything else is exact.
- The two scenes are defined once, in `demo_scenes`, shared by `render_demo` and the
  golden test, so the app and the test cannot drift apart.
- The references are regenerated only when rendering behaviour changes on purpose, with
  `render_demo --scene <flat|diffuse> --width 320 --height 180 --output tests/golden/...`
  from a Release build, and the pictures are reviewed before they are committed.

## 9. Demo application

`render_demo` renders one of the demo pictures to a file:

```
render_demo [--scene diffuse|flat|axes] [--width N] [--height N] [--distance D] [--output PATH]
```

Defaults: `diffuse`, 1920 x 1080, distance 1, `render.png`. The `axes` scene draws the
coordinate axes and corner dots; it exercises the canvas without ray tracing. The
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

| Scene                              | Size        | Target                                           |
|------------------------------------|-------------|--------------------------------------------------|
| diffuse demo (4 spheres, 3 lights) | 1920 x 1080 | under 1 second; around 100 ms is the expectation |
| diffuse demo                       | 320 x 180   | under 50 ms, so the golden test stays cheap      |

The render loop iterates rows in the outer loop so the canvas is written sequentially.
Rows are independent, which keeps the door open for a row-parallel `Render` later; no
threading yet.

## 13. Roadmap

The project follows the structure of *Computer Graphics from Scratch* (Gabriel
Gambetta). The architecture in section 4 is meant to absorb these steps as additions:

- Specular reflection: `Sphere::specular`, a `view` parameter on `ComputeLighting`.
- Shadows: `ClosestIntersection` factored out of `TraceRay`, reused for shadow rays.
- Reflections: `Sphere::reflective`, recursion depth parameter on `TraceRay`.
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
