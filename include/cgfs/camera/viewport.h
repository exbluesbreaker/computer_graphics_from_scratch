#pragma once

#include <cgfs/image/canvas.h>
#include <cgfs/math/vec3.h>

namespace cgfs {

/// The window through which the camera sees the scene.
///
/// The camera sits at the origin (0, 0, 0) and looks along +Z. The viewport is a
/// `width` x `height` rectangle perpendicular to Z, centered on the Z axis at `distance`
/// from the camera. Every canvas pixel maps to one point on it; the ray from the origin
/// through that point is what the pixel "sees".
///
/// The viewport remembers the canvas dimensions by value, so it never refers back to a
/// Canvas object.
class Viewport {
public:
    /// Throws std::invalid_argument when a dimension or the distance is not positive.
    Viewport(int canvas_width, int canvas_height, double width = 1.0, double height = 1.0,
             double distance = 1.0);
    explicit Viewport(const Canvas& canvas, double width = 1.0, double height = 1.0,
                      double distance = 1.0);

    /// A viewport whose aspect ratio matches the canvas, so the image is not stretched:
    /// width = height * canvas_width / canvas_height.
    [[nodiscard]] static Viewport MatchingCanvasAspect(const Canvas& canvas, double height = 1.0,
                                                       double distance = 1.0);

    /// The 3D point on the viewport that canvas pixel (x, y) looks through:
    ///   Vx = x * width / canvas_width,  Vy = y * height / canvas_height,  Vz = distance.
    [[nodiscard]] Vec3 CanvasToViewport(int x, int y) const noexcept;

    struct FieldOfView {
        double horizontal_degrees;
        double vertical_degrees;
    };
    /// The angles the viewport subtends as seen from the camera.
    [[nodiscard]] FieldOfView Fov() const noexcept;

    [[nodiscard]] int canvas_width() const noexcept { return canvas_width_; }
    [[nodiscard]] int canvas_height() const noexcept { return canvas_height_; }
    [[nodiscard]] double width() const noexcept { return width_; }
    [[nodiscard]] double height() const noexcept { return height_; }
    [[nodiscard]] double distance() const noexcept { return distance_; }

private:
    int canvas_width_;
    int canvas_height_;
    double width_;
    double height_;
    double distance_;
};

}  // namespace cgfs
