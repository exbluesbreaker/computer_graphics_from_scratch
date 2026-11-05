# CLAUDE.md

How to work in this repository. `README.md` says how to build and run it; `docs/SPEC.md`
is the design reference (formulas, architecture, public API, tests, roadmap, decisions).
A change is finished when the code, the tests, the spec and the README agree.

## What this is

A software 3D renderer written from scratch in C++20, following Gabriel Gambetta's
*Computer Graphics from Scratch* chapter by chapter: a ray tracer (complete through
reflections) and a rasterizer (lines, filled and shaded triangles, perspective
projection and the wireframes of objects made of triangles so far). It exists for
learning: every algorithm is written by hand, reads like the book's listing and is
pinned by unit tests and golden images. No graphics libraries, no GPU.

## Build, test, run

```
cmake --preset dev && cmake --build --preset dev && ctest --preset dev    # Debug, sanitizers
cmake --preset release && cmake --build --preset release && ctest --preset release
./build/release/apps/render_demo --scene lines --width 320 --height 180 --output out.png
```

Both presets build with `-Werror` and must pass before work is reported as done. Golden
images are regenerated only for an intended change of output, from a Release build, and
are looked at before they are committed; the README has the commands.

## Design principles

The code follows SOLID, applied to a small C++ library of value types and free
functions rather than to class hierarchies.

- **Single responsibility.** One concept per file and per type, named after it:
  `Canvas` stores pixels, `Viewport` maps pixels to the world, `Interpolate`
  interpolates, `DrawLine` draws a line. A file whose description needs "and" is two
  files. A function does one thing at one level of abstraction; helpers live in an
  anonymous namespace next to their only user until a second user appears.
- **Open/closed.** Extend by adding, not by editing what works: a new demo picture is a
  new function plus a row in the scene table of `render_demo`; a new light is a new
  `std::variant` alternative plus its `std::visit` case; a new primitive is a new file
  in `rasterizer/`. New fields and parameters get defaults, so existing callers and
  aggregate initializers keep compiling. The golden tests guard what already works.
- **Liskov substitution.** There are no class hierarchies, by decision: closed sets of
  alternatives are `std::variant`s, and interchangeable behaviours are functions with
  the same signature (the scene table). If a hierarchy ever becomes necessary, every
  derived type must honour the base's documented contract, with preconditions no
  stronger and postconditions no weaker, so a caller never needs to know which one it
  holds.
- **Interface segregation.** Take the narrowest thing that does the job: `DrawLine`
  takes a `Canvas&`, not the program's options; `Viewport` copies two dimensions
  instead of keeping a `Canvas&`; `ComputeLighting` takes a point, a normal and a view
  vector, not a `Hit`. Headers are small and include only what they use, so including
  one does not pull in a layer.
- **Dependency inversion.** Dependencies point from higher layers to lower ones, never
  back, and across a boundary they go through the smallest stable interface: both
  renderers touch the picture only through `Canvas::PutPixel`, scenes are plain data
  that renderers read, the demo pictures are functions that the app and the tests call.
  Nothing under `src/` knows about `apps/` or `tests/`.

And the rest of the usual discipline:

- **Separation of concerns, in layers.** Math, image, camera, the two renderers and the
  demo content are separate layers (next section). A layer never reaches up.
- **Don't repeat yourself.** One definition per fact: the demo pictures are defined once
  and shared by the app and the golden tests; the scene table drives the usage text,
  the default and the dispatch; the pixel conversion rule is one function (`ToPixel`).
- **Keep it simple; you aren't gonna need it.** Implement the chapter at hand in the
  book's shape and nothing speculative. Prefer a free function to a class without
  state, a `std::variant` to a hierarchy, a value to a pointer, a `struct` with public
  fields to getters over plain data. Add a class when there is state to protect.
- **Explicit over clever.** Write the formula the way the book writes it and let the
  doc comment explain the geometry. No template metaprogramming, no macros in the
  library, no implicit conversion that hides a truncation; the two deliberate
  truncations, colors and pixel coordinates, are named functions documented in the spec.
- **Value semantics and const.** `Vec3`, `Color` and `Point` are values, passed by
  value; `Scene`, `Canvas` and `Viewport` are passed by reference, `const` unless
  mutated. `[[nodiscard]]` on every function whose only effect is its return value. No
  owning raw pointers, no `new`.
- **Errors at the boundary, contracts inside.** Constructors validate their arguments
  and throw `std::invalid_argument`; file I/O throws `std::runtime_error` with the path
  in the message; the inner loops never throw. Preconditions inside the library are
  `assert`ed in Debug builds and stated in the doc comment.
- **Tests pin behaviour.** Every module has unit tests and every demo picture has a
  golden image. A refactor leaves every golden image byte for byte the same. A change of
  a formula is a change of the spec first, then of the code, then of the golden images.

## Folder structure: the architecture is visible from the tree

The source tree is organised by layer, and the same layer names appear under
`include/cgfs/`, `src/` and `tests/`. Reading the folder names from the bottom up is
reading the architecture:

```
math/         vec3                                            vector algebra; nothing above the numbers
image/        color, canvas, image_io                         the picture: colors, the pixel buffer, file formats
camera/       viewport                                        how canvas pixels map into the world; shared by both renderers
raytracer/    scene, raytracer                                the ray tracing renderer: spheres, lights, tracing
rasterizer/   point, interpolate, line, triangle, projection, object
                                                              the rasterizing renderer: 2D primitives, the
                                                              projection that feeds them and objects made of
                                                              triangles, one per file
demo/         raytracer_scenes, rasterizer_scenes             the demo pictures, built on everything above
```

Rules that keep it that way:

1. **One folder per layer, one file per concept, named after it.** Opening
   `include/cgfs/` shows the layers; opening a layer shows its concepts; opening a file
   shows exactly the concept its name promises, with a header comment that reads like a
   tutorial. A file that grows a second concept is split.
2. **Dependencies point down only.** A file includes from its own layer or a lower one,
   never from a higher one. The two renderers do not know of each other; the demo layer
   knows all of them; `apps/` knows the demo layer.
3. **Mirrored trees.** `src/<layer>/x.cpp` implements `include/cgfs/<layer>/x.h`, and
   `tests/<layer>/x_test.cpp` tests it. Test-only helpers live in `tests/support/` when
   shared by every layer, or next to the tests of their own layer. Reference images
   live in `tests/demo/golden/`.
4. **A new layer is a new folder** in all three trees, plus a line in the table above
   and in `docs/SPEC.md` sections 4.1 and 4.2. A new file goes into the layer it belongs
   to, which is the highest layer it depends on.
5. **Outside the library:** `apps/` holds executables (argument parsing, dispatch,
   timing, saving) and no rendering logic; `third_party/` is vendored code behind its
   own CMake target; `docs/` is the spec.
6. **Namespaces stay flat** (`cgfs`, `cgfs::demo`): the folders carry the structure,
   and `cgfs::Vec3` is what the book's listings read like. The include path carries the
   layer: `#include <cgfs/image/canvas.h>`.
7. **CMake targets follow the tree:** `cgfs` (the library: every layer but `demo`),
   `cgfs_demo` (the demo content), `render_demo`, `cgfs_tests`. Source lists in the
   CMake files are grouped by layer in dependency order.

## Conventions

- Naming: types and functions `PascalCase` (as in the book: `PutPixel`, `TraceRay`),
  variables `snake_case`, data members `snake_case_`, constants `kPascalCase`, trivial
  accessors lowercase. Formatting is `.clang-format` (Google style, 4 spaces, 100
  columns); new code follows it even where older files have not been reformatted.
- Doc comments in headers explain the geometry and the formula, not just the
  signature. Implementation comments say why, not what.
- Commits: one chapter step or one refactor per commit, imperative subject, a body that
  says what changed and why. Nothing is committed with failing tests or stale docs.
