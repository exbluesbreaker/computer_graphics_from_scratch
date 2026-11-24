#include <cgfs/lighting/illumination.h>

#include <cassert>
#include <cmath>
#include <variant>

namespace cgfs {

double ComputeIllumination(Vec3 point, Vec3 normal, Vec3 view, double specular,
                           std::span<const Light> lights) {
    assert(std::abs(Length(normal) - 1.0) < 1e-9 &&
           "ComputeIllumination: the normal must be unit length");

    // What one point or directional light that arrives along `to_light` adds.
    const auto reflection = [&](double light_intensity, Vec3 to_light) {
        double i = DiffuseReflection(light_intensity, normal, to_light);
        if (specular != kMatte) {
            i += SpecularReflection(light_intensity, normal, to_light, view, specular);
        }
        return i;
    };

    double intensity = 0.0;
    for (const Light& light : lights) {
        std::visit(Overloaded{
                       [&](const AmbientLight& l) { intensity += l.intensity; },
                       [&](const PointLight& l) {
                           intensity += reflection(l.intensity, l.position - point);
                       },
                       [&](const DirectionalLight& l) {
                           intensity += reflection(l.intensity, l.direction);
                       },
                   },
                   light);
    }
    return intensity;
}

}  // namespace cgfs
