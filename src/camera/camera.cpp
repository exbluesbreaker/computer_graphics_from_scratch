#include <cgfs/camera/camera.h>

namespace cgfs {

Mat4 MakeCameraMatrix(Vec3 position, const Mat4& orientation) {
    return Transposed(orientation) * MakeTranslationMatrix(-position);
}

}  // namespace cgfs
