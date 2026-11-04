#include <cgfs/raytracer/raytracer.h>

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

/// The specular contribution of the same light seen from `view`: the light bounces off
/// in the mirror direction R = 2 N <N, L> - L, and the highlight fades with the angle
/// between R and V as cos^exponent. Zero when the viewer is not on the reflected side.
double Specular(double light_intensity, Vec3 normal, Vec3 to_light, Vec3 view,
                double exponent) {
    const Vec3 reflected = Reflect(to_light, normal);
    const double r_dot_v = Dot(reflected, view);
    if (r_dot_v <= 0) {
        return 0.0;
    }
    return light_intensity * std::pow(r_dot_v / (Length(reflected) * Length(view)), exponent);
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
            canvas.PutPixel(x, y, TraceRay(origin, direction, 1.0, kInfinity, kReflectionDepth));
        }
    }
}

Hit RayTracer::ClosestIntersection(Vec3 origin, Vec3 direction, double t_min,
                                   double t_max) const {
    Hit closest;
    for (const Sphere& sphere : scene_.spheres) {
        const auto [t1, t2] = IntersectRaySphere(origin, direction, sphere);
        if (t_min <= t1 && t1 <= t_max && t1 < closest.t) {
            closest = {&sphere, t1};
        }
        if (t_min <= t2 && t2 <= t_max && t2 < closest.t) {
            closest = {&sphere, t2};
        }
    }
    return closest;
}

Color RayTracer::TraceRay(Vec3 origin, Vec3 direction, double t_min, double t_max,
                          int recursion_depth) const {
    const Hit hit = ClosestIntersection(origin, direction, t_min, t_max);
    if (hit.sphere == nullptr) {
        return scene_.background_color;
    }

    const Vec3 point = origin + hit.t * direction;                 // intersection point
    const Vec3 normal = Normalized(point - hit.sphere->center);    // sphere normal there
    const Vec3 view = -direction;                                  // back toward the eye
    const Color local_color =
        hit.sphere->color * ComputeLighting(point, normal, view, hit.sphere->specular);

    // Done when the bounces are used up or the surface reflects nothing.
    const double r = hit.sphere->reflective;
    if (recursion_depth <= 0 || r <= 0) {
        return local_color;
    }

    // The ray bounces off in the mirror direction of the view vector. Whatever it sees
    // there is blended into the local color in proportion to the reflectivity.
    const Vec3 reflected = Reflect(view, normal);
    const Color reflected_color =
        TraceRay(point, reflected, kSurfaceEpsilon, kInfinity, recursion_depth - 1);
    return local_color * (1 - r) + reflected_color * r;
}

double RayTracer::ComputeLighting(Vec3 point, Vec3 normal, Vec3 view, double specular) const {
    assert(std::abs(Length(normal) - 1.0) < 1e-9 && "normal must be unit length");

    // One point or directional light arriving along `to_light`, which reaches the light
    // itself at t = t_max (1 for a point light, never for a directional one). Any sphere
    // on that stretch of the ray puts the point in shadow and the light is skipped
    // entirely. As in the book, the highlight is not gated on <N, L> > 0: only the
    // diffuse term checks that the light is in front of the surface.
    const auto shade = [&](double light_intensity, Vec3 to_light, double t_max) {
        const Hit blocker = ClosestIntersection(point, to_light, kSurfaceEpsilon, t_max);
        if (blocker.sphere != nullptr) {
            return 0.0;
        }
        double i = Diffuse(light_intensity, normal, to_light);
        if (specular != kMatte) {
            i += Specular(light_intensity, normal, to_light, view, specular);
        }
        return i;
    };

    double intensity = 0.0;
    for (const Light& light : scene_.lights) {
        std::visit(Overloaded{
                       [&](const AmbientLight& l) { intensity += l.intensity; },
                       [&](const PointLight& l) {
                           intensity += shade(l.intensity, l.position - point, 1.0);
                       },
                       [&](const DirectionalLight& l) {
                           intensity += shade(l.intensity, l.direction, kInfinity);
                       },
                   },
                   light);
    }
    return intensity;
}

}  // namespace cgfs
