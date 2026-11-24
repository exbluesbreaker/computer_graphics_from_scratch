#include <cgfs/lighting/light.h>

namespace cgfs {

Light TransformLight(const Light& light, const Mat4& transform) {
    return std::visit(
        Overloaded{
            [](const AmbientLight& l) -> Light { return l; },
            [&](const PointLight& l) -> Light {
                return PointLight{.intensity = l.intensity, .position = transform * l.position};
            },
            [&](const DirectionalLight& l) -> Light {
                return DirectionalLight{.intensity = l.intensity,
                                        .direction = TransformDirection(transform, l.direction)};
            },
        },
        light);
}

}  // namespace cgfs
