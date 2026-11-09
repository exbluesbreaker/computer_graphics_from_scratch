#pragma once

#include <cgfs/math/vec3.h>
#include <cgfs/rasterizer/object.h>

#include <vector>

namespace cgfs {

/// The shape of an object, described once: its vertices in the object's own coordinates
/// (model space, with the object's center at the origin) and the triangles that name
/// them by index; exactly the two lists RenderObject takes.
///
/// A model says what an object looks like, not where it is. Where it is, and how many
/// times it appears, is the business of Instance: a scene with twenty cubes stores the
/// cube's eight vertices and twelve triangles once, as one Model, and twenty Instances
/// of it. This is the book's `model { vertices, triangles }`.
struct Model {
    std::vector<Vec3> vertices;
    std::vector<Triangle> triangles;
};

}  // namespace cgfs
