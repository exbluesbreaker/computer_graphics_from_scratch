#pragma once

#include <cgfs/math/vec3.h>
#include <cgfs/rasterizer/model.h>

namespace cgfs {

// Back-face culling: the triangles of an object that are turned away from the camera are
// not drawn at all.
//
// A closed object, one whose surface has no opening, shows a camera outside it only the
// faces turned toward the camera, its front faces: they cover the faces turned away, the
// back faces, completely. A depth buffer finds that out pixel by pixel, after the work of
// drawing them. Here a back face is recognised from its three corners and removed before
// anything is drawn, which is about half the triangles of a scene, and the picture is the
// same, give or take a pixel on an outline that only a back face happened to reach (the
// spec, section 3.12, says how).
//
// Which side of a triangle is its outside is not something three points can say: a flat
// triangle has two sides, and a normal to it can be taken either way. The model says it,
// by the order in which a triangle names its corners, and that order has to be right when
// the model is made: clockwise as seen from outside the object, the book's rule, in the
// book's coordinates (x to the right, y up, z away from the viewer). With the corners A,
// B and C in that order the cross product (B - A) x (C - A) points out of the object;
// with two of them swapped it is the same triangle in space and the opposite normal, a
// face that is seen from inside the object and not from outside. A, B, C and B, C, A and
// C, A, B are one triangle and one normal; A, C, B is the other.
//
// The order survives everything the pipeline does to a triangle on its way to this step
// and after it: a rotation, a translation and a scaling by a positive factor turn and
// move the normal along with the triangle, and ClipTriangle rotates the corners of the
// pieces it cuts and never swaps them. A scale below zero would mirror the model and
// turn every triangle inside out; nothing here corrects for that.

/// Whether RenderScene removes the back faces of a scene before it draws it. On is the
/// default; off draws every triangle, which is what an object needs that is not closed (a
/// lone triangle, a floor seen from below) or one that is to be looked into.
enum class BackFaceCulling { kOff, kOn };

/// Whether the triangle with the corners A, B and C, in that order, is turned away from
/// a camera at `camera_position`: the book's classification of a triangle.
///
///     N = (B - A) x (C - A)        the triangle's normal, pointing out of the object
///     V = camera_position - A      from a corner of the triangle to the camera
///
///     <N, V> >  0     front-facing: the angle between N and V is less than 90 degrees
///     <N, V> <= 0     back-facing
///
/// <N, V> = |N| |V| cos(angle) and the lengths are positive, so the sign of the dot
/// product is the sign of the cosine and neither vector has to be normalized. V runs to
/// the camera from a point of the triangle, and it does not matter which: every point P
/// of the triangle's plane gives the same <N, camera_position - P>, the distance of the
/// camera from that plane times |N|. What it must not be is the direction the camera
/// looks in: under perspective a wall beside the camera is seen though it is turned
/// partly away from the line of sight.
///
/// A triangle seen exactly edge-on (<N, V> = 0) is back-facing, the book's choice: its
/// picture has no area. So is a triangle without area, whose normal is the zero vector.
///
/// All four points are in the same coordinates, whichever those are.
[[nodiscard]] bool IsBackFacing(Vec3 a, Vec3 b, Vec3 c, Vec3 camera_position);

/// The object without its back faces, the book's CullBackFaces: every triangle of
/// `object.triangles` that IsBackFacing says is turned away from `camera_position` is
/// removed, and the others stay as they are and in their order.
///
/// `object` is a model whose vertices and the camera's position are in the same
/// coordinates. RenderScene calls this on an instance placed in the scene, in the
/// camera's coordinates, where the camera is at the origin.
///
/// Only the triangle list changes. The vertices stay, those that no triangle names any
/// more among them, and so does the bounding sphere: what is left of the object is
/// still inside it.
///
/// Preconditions: the object is closed and its triangles list their corners clockwise
/// as seen from outside, neither of which can be checked from here: a triangle listed
/// the other way is missing from the picture when it should be seen, and an object
/// with an opening shows nothing through it but what is behind the object. Every
/// triangle's indices are less than `object.vertices.size()`, asserted.
[[nodiscard]] Model CullBackFaces(Model object, Vec3 camera_position);

}  // namespace cgfs
