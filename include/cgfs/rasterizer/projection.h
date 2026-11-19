#pragma once

#include <cgfs/camera/viewport.h>
#include <cgfs/math/vec3.h>
#include <cgfs/rasterizer/point.h>

namespace cgfs {

/// The canvas point of the point (x, y) on the viewport: the viewport is `width` x
/// `height` units and the canvas `canvas_width` x `canvas_height` pixels, both centered
/// on the same axis, so
///
///     Cx = x * canvas_width / width,   Cy = y * canvas_height / height,
///
/// truncated to a pixel with ToPixel, as the book's code does. This is the inverse of
/// Viewport::CanvasToViewport, except for the truncation: a viewport point lands on the
/// pixel that looks through it, and all the viewport points of one pixel land on it.
/// A viewport point has no depth: the point's `inv_z` stays 0.
[[nodiscard]] Point ViewportToCanvas(const Viewport& viewport, double x, double y);

/// The canvas point where the vertex v appears: perspective projection.
///
/// The camera is at the origin looking along +Z, and v is seen along the ray from the
/// origin through it. That ray crosses the viewport plane z = d at
///
///     Px = v.x * d / v.z,   Py = v.y * d / v.z
///
/// (similar triangles: the point at depth v.z scales down by d / v.z), and that
/// viewport point is converted to a pixel with ViewportToCanvas. Vertices on the same ray
/// project to the same pixel; farther vertices land closer to the center.
///
/// The projection flattens the scene, and the pixel alone no longer says how far the
/// vertex was. The point therefore takes `inv_z = 1 / v.z` along (see Point), which is
/// what a depth buffer compares to tell which of two vertices on one ray is in front.
///
/// Precondition: v.z > 0, strictly in front of the camera; asserted. The projection
/// does not clip: a vertex outside the field of view projects to a point beyond the
/// canvas, which DrawLine and the triangles draw as far as the canvas reaches, and a
/// vertex very near the camera plane projects very far away. Clipping the scene first
/// (ClipScene, against the planes of MakeClippingPlanes) removes both, and guarantees
/// the precondition.
[[nodiscard]] Point ProjectVertex(const Viewport& viewport, Vec3 v);

}  // namespace cgfs
