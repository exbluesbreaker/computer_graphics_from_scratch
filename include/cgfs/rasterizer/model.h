#pragma once

#include <cgfs/camera/viewport.h>
#include <cgfs/image/canvas.h>
#include <cgfs/lighting/illumination.h>
#include <cgfs/math/bounding_sphere.h>
#include <cgfs/math/mat4.h>
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
///
/// `bounding_sphere` is a sphere around the vertices, in the same coordinates as they
/// are, the book's `bounds_center` and `bounds_radius`. Clipping reads it to decide about
/// the whole model at once (see BoundingSphere and ClipInstanceAgainstPlane). The default
/// is the sphere that contains everything, with which every triangle is examined;
/// `MakeBoundingSphere(vertices)` gives a model a real one.
///
/// `specular` is how shiny the model's surface is: the exponent s of the specular term of
/// the illumination equation (see SpecularReflection), as for a sphere of the ray tracer.
/// A higher value gives a smaller, sharper highlight. The default, kMatte, is a surface
/// without one. Only the drawing that lights reads it (see shading.h).
struct Model {
    std::vector<Vec3> vertices;
    std::vector<Triangle> triangles;
    BoundingSphere bounding_sphere;
    double specular = kMatte;

    bool operator==(const Model&) const = default;
};

/// The model under a transform, as a model of its own: every vertex V becomes
/// `transform * V`, the triangles name the same corners in the same colors, and the
/// bounding sphere moves along with the vertices. Its center is transformed like a
/// vertex, and its radius is multiplied by the factor by which the transform scales
/// lengths, the book's `radius * scale`.
///
/// The normals at the triangles' corners turn with the model and come out at unit
/// length: each becomes `Normalized(TransformDirection(transform, normal))`. The
/// transform is applied without its translation, since a normal is a direction and
/// moving a surface does not change which way it faces; and its scale, which would
/// stretch a normal like any other direction, is taken out again, since a normal says
/// which way and not how far. However long the model stated its normals, from here on
/// they are unit vectors, which is what lets them be interpolated with one another: by
/// the clipping, where a triangle is cut, and by Phong shading, across a triangle. A
/// corner without a normal, the zero vector, stays without one. The specular exponent is
/// the model's.
///
/// That factor is read off the matrix: the first column of the matrix is where the unit
/// vector along X ends up (before the translation), so its length is how much longer a
/// length of 1 has become. The transforms of this pipeline are a rotation, a scaling
/// that is the same in every direction and a translation, and for those every direction
/// is scaled by that one factor: the instance's `scale`, whatever the rotation.
///
/// With the camera matrix times an instance's model matrix as the transform, the result
/// is the instance as geometry in the camera's coordinates, "placed in the scene" in the
/// book's words: what the clipping step works on, and what RenderObject projects.
///
/// Precondition: `transform` scales uniformly, as described; otherwise the sphere may
/// not contain the transformed vertices, and the normals are no longer perpendicular to
/// the surface.
[[nodiscard]] Model TransformModel(const Model& model, const Mat4& transform);

/// Draw the wireframe of a model under a transform: the vertices of
/// `TransformModel(model, transform)` are drawn with the model's triangles by
/// RenderObject, which projects each of them with ProjectVertex and outlines each
/// triangle; the book's RenderModel. `transform` is the whole chain from the model's
/// coordinates to the camera's, usually the camera matrix times the instance's model
/// matrix (see RenderScene). The projection is not part of it, on purpose (see Mat4).
/// Nothing is clipped; RenderScene does that.
///
/// Preconditions: every transformed vertex has z > 0, asserted by ProjectVertex; every
/// triangle's indices are less than `model.vertices.size()`, asserted by RenderObject.
void RenderModel(Canvas& canvas, const Viewport& viewport, const Model& model,
                 const Mat4& transform);

}  // namespace cgfs
