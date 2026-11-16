#pragma once

#include <cgfs/math/mat4.h>
#include <cgfs/math/plane.h>
#include <cgfs/math/vec3.h>

#include <vector>

namespace cgfs {

/// Where the camera stands and which way it looks: the position of the eye in the world
/// and a rotation matrix, the camera's orientation, that turns the camera's own axes (x
/// to its right, y up, looking along its +Z) into the world's. The default camera is the
/// one every earlier chapter assumed, at the origin looking along +Z, so a picture drawn
/// without a camera is unchanged. Plain data; the Viewport stays a separate concern, the
/// window in front of this eye.
///
/// `clipping_planes` bound the part of space the camera draws, the clipping volume, as
/// in the book's demo, where the planes belong to the camera too. They are given in the
/// camera's own coordinates, with the eye at the origin looking along +Z, so they move
/// and turn with it, and each normal points into the volume. The list is empty by
/// default, which clips nothing: the camera of the chapters before clipping.
/// MakeClippingPlanes builds the five planes of what a viewport shows.
struct Camera {
    Vec3 position;
    Mat4 orientation = Mat4::Identity();
    std::vector<Plane> clipping_planes;
};

/// The matrix that brings a point of the world into the camera's coordinates, the book's
/// MakeCameraMatrix:
///
///     Transposed(orientation) * MakeTranslationMatrix(-position)
///
/// Moving the camera is the same as moving the whole world the opposite way: a point is
/// first moved by -position, which puts the camera at the origin, and then turned by the
/// inverse of the camera's rotation, which puts the camera's line of sight back on +Z;
/// for a rotation matrix the inverse is the transpose. After this, ProjectVertex applies
/// as if the camera had never moved. Precondition: `orientation` is a rotation
/// (orthonormal, as MakeOYRotationMatrix builds); not checked.
[[nodiscard]] Mat4 MakeCameraMatrix(Vec3 position, const Mat4& orientation);

}  // namespace cgfs
