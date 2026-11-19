#pragma once

#include <cgfs/image/canvas.h>

#include <optional>
#include <vector>

namespace cgfs {

/// For every pixel of a canvas, how near the nearest thing drawn there so far is: what
/// hidden-surface removal has to remember.
///
/// Triangles are drawn one after another. Without a memory a pixel shows whichever
/// triangle was drawn there last; with this one a triangle sets a pixel only where it is
/// nearer than everything drawn at that pixel before, and the picture no longer depends
/// on the order. The book's depth buffer.
///
/// The value kept for a pixel is not the depth z of the surface seen there but 1/z,
/// `inv_z`, for two reasons. Over the picture of a triangle 1/z changes by equal steps
/// from pixel to pixel while z does not (see DrawFilledTriangle), so it is the quantity
/// that the rasterizer can interpolate. And "nothing here yet" needs no special value:
/// in front of the camera z > 0 and 1/z > 0, nearer is larger, and as z grows without
/// bound 1/z falls to 0, so an empty pixel holds 0, infinitely far, and anything in
/// front of the camera is nearer than that.
///
/// A pixel is addressed by the centered coordinates of the Canvas, and exactly the
/// pixels a canvas of the same size has exist here (see PixelIndex). One buffer serves
/// one picture: everything drawn through it hides, and is hidden by, everything else
/// drawn through it.
class DepthBuffer {
public:
    /// A buffer for a canvas of `width` x `height` pixels, every pixel empty.
    /// Throws std::invalid_argument when width or height is not positive.
    DepthBuffer(int width, int height);
    /// A buffer of the size of `canvas`, every pixel empty.
    explicit DepthBuffer(const Canvas& canvas);

    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }

    /// Ask whether a surface at depth z = 1 / `inv_z` is the nearest so far at the pixel
    /// (x, y), and remember it if it is: when `inv_z` is greater than the stored value,
    /// store it and return true, and the caller sets the pixel; otherwise change nothing
    /// and return false. The book's UpdateDepthBufferIfCloser.
    ///
    /// The comparison is strict: of two surfaces equally deep at a pixel the one drawn
    /// first stays, and a value of 0 or less, at infinity or behind the camera, is never
    /// stored. A pixel outside the buffer is never the nearest, as PutPixel would ignore
    /// it anyway.
    bool UpdateIfCloser(int x, int y, double inv_z) noexcept;

    /// The stored 1/z at (x, y), 0 where nothing has been drawn yet, or nullopt when the
    /// pixel is outside the buffer.
    [[nodiscard]] std::optional<double> InvZ(int x, int y) const noexcept;

private:
    int width_;
    int height_;
    std::vector<double> inv_z_;
};

}  // namespace cgfs
