#include <cgfs/viewport.h>

#include <cmath>
#include <numbers>
#include <stdexcept>
#include <string>

namespace cgfs {

Viewport::Viewport(int canvas_width, int canvas_height, double width, double height,
                   double distance)
    : canvas_width_(canvas_width),
      canvas_height_(canvas_height),
      width_(width),
      height_(height),
      distance_(distance) {
    if (canvas_width <= 0 || canvas_height <= 0) {
        throw std::invalid_argument("Viewport canvas dimensions must be positive, got " +
                                    std::to_string(canvas_width) + "x" +
                                    std::to_string(canvas_height));
    }
    if (width <= 0 || height <= 0 || distance <= 0) {
        throw std::invalid_argument("Viewport width, height and distance must be positive");
    }
}

Viewport::Viewport(const Canvas& canvas, double width, double height, double distance)
    : Viewport(canvas.width(), canvas.height(), width, height, distance) {}

Viewport Viewport::MatchingCanvasAspect(const Canvas& canvas, double height, double distance) {
    return Viewport(canvas, height * canvas.width() / canvas.height(), height, distance);
}

Vec3 Viewport::CanvasToViewport(int x, int y) const noexcept {
    return {x * width_ / canvas_width_, y * height_ / canvas_height_, distance_};
}

Viewport::FieldOfView Viewport::Fov() const noexcept {
    constexpr double kRadiansToDegrees = 180.0 / std::numbers::pi;
    return {2 * std::atan(width_ / 2 / distance_) * kRadiansToDegrees,
            2 * std::atan(height_ / 2 / distance_) * kRadiansToDegrees};
}

}  // namespace cgfs
