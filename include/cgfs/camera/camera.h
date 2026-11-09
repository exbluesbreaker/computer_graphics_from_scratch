#pragma once

#include <cgfs/math/mat4.h>
#include <cgfs/math/vec3.h>

namespace cgfs {

/// Where the camera stands and which way it looks: the position of the eye in the world
/// and a rotation matrix, the camera's orientation, that turns the camera's own axes (x
/// to its right, y up, looking along its +Z) into the world's. The default camera is the
/// one every earlier chapter assumed, at the origin looking along +Z, so a picture drawn
/// without a camera is unchanged. Plain data; the Viewport stays a separate concern, the
/// window in front of this eye.
struct Camera {
    Vec3 position;
    Mat4 orientation = Mat4::Identity();
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
