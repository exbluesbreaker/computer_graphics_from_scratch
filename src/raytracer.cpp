#include <cgfs/raytracer.h>

#include <cassert>
#include <cmath>
#include <stdexcept>
#include <string>
#include <variant>

namespace cgfs {

namespace {

template <class... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};

/// The diffuse contribution of one light of intensity `light_intensity` arriving along
/// `to_light` at a surface with normal `normal`. Zero when the light is behind the surface.
double Diffuse(double light_intensity, Vec3 normal, Vec3 to_light) {
    const double n_dot_l = Dot(normal, to_light);
    if (n_dot_l <= 0) {
        return 0.0;
    }
    return light_intensity * n_dot_l / (Length(normal) * Length(to_light));
}

}  // namespace

SphereIntersection IntersectRaySphere(Vec3 origin, Vec3 direction, const Sphere& sphere) {
    const Vec3 co = origin - sphere.center;

    const double a = Dot(direction, direction);
    const double b = 2 * Dot(co, direction);
    const double c = Dot(co, co) - sphere.radius * sphere.radius;

    const double discriminant = b * b - 4 * a * c;
    if (discriminant < 0) {
        return {};
    }

    const double sqrt_disc = std::sqrt(discriminant);
    return {(-b + sqrt_disc) / (2 * a), (-b - sqrt_disc) / (2 * a)};
}

void RayTracer::Render(Canvas& canvas) const {
    if (canvas.width() != viewport_.canvas_width() ||
        canvas.height() != viewport_.canvas_height()) {
        throw std::invalid_argument(
            "RayTracer::Render: canvas is " + std::to_string(canvas.width()) + "x" +
            std::to_string(canvas.height()) + " but the viewport was built for " +
            std::to_string(viewport_.canvas_width()) + "x" +
            std::to_string(viewport_.canvas_height()));
    }

    const Vec3 origin{};  // the camera
    const int half_w = canvas.width() / 2;
    const int half_h = canvas.height() / 2;
    // Rows from the top down so the canvas is written sequentially.
    for (int y = half_h - 1; y >= -half_h; --y) {
        for (int x = -half_w; x < half_w; ++x) {
            const Vec3 direction = viewport_.CanvasToViewport(x, y);
            canvas.PutPixel(x, y, TraceRay(origin, direction, 1.0, kInfinity));
        }
    }
}

Color RayTracer::TraceRay(Vec3 origin, Vec3 direction, double t_min, double t_max) const {
    double closest_t = kInfinity;
    const Sphere* closest_sphere = nullptr;
    for (const Sphere& sphere : scene_.spheres) {
        const auto [t1, t2] = IntersectRaySphere(origin, direction, sphere);
        if (t_min <= t1 && t1 <= t_max && t1 < closest_t) {
            closest_t = t1;
            closest_sphere = &sphere;
        }
        if (t_min <= t2 && t2 <= t_max && t2 < closest_t) {
            closest_t = t2;
            closest_sphere = &sphere;
        }
    }
    if (closest_sphere == nullptr) {
        return scene_.background_color;
    }

    const Vec3 point = origin + closest_t * direction;        // intersection point
    const Vec3 normal = Normalized(point - closest_sphere->center);  // sphere normal there
    return closest_sphere->color * ComputeLighting(point, normal);
}

double RayTracer::ComputeLighting(Vec3 point, Vec3 normal) const {
    assert(std::abs(Length(normal) - 1.0) < 1e-9 && "normal must be unit length");

    double intensity = 0.0;
    for (const Light& light : scene_.lights) {
        std::visit(Overloaded{
                       [&](const AmbientLight& l) { intensity += l.intensity; },
                       [&](const PointLight& l) {
                           intensity += Diffuse(l.intensity, normal, l.position - point);
                       },
                       [&](const DirectionalLight& l) {
                           intensity += Diffuse(l.intensity, normal, l.direction);
                       },
                   },
                   light);
    }
    return intensity;
}

}  // namespace cgfs
