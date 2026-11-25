#pragma once

#include <cgfs/image/color.h>
#include <cgfs/image/image_io.h>

namespace cgfs {

/// A point of a texture, in the texture's own coordinates: the book's (u, v).
///
/// A texture is an image, and a point of it could be named by a column and a row. But x
/// and y are taken, by the canvas, and a model that named texels would be tied to one
/// size of image. So the coordinates are real numbers from 0 to 1 whatever the size of
/// the texture is: u runs from its left edge, 0, to its right edge, 1, and v from its
/// top edge, 0, to its bottom edge, 1. The origin is the top-left corner. A model that
/// says "this corner of the triangle shows (1, 0)" means the top-right corner of
/// whatever image it is painted with.
///
/// A triangle states one at each of its corners (see Triangle), and the rasterizer
/// carries them to the pixels in between (see DrawTexturedTriangle). The default is the
/// origin.
struct Uv {
    double u = 0.0;
    double v = 0.0;

    bool operator==(const Uv&) const = default;
};

/// An image to paint triangles with: the book's texture.
///
/// A crate could be modelled plank by plank and nail by nail, with a great many
/// triangles. Painting a picture of planks and nails onto the twelve triangles of a cube
/// is far cheaper and, from a little way off, looks the same. The picture is the
/// texture, and its pixels are called texels, texture elements, to tell them from the
/// pixels of the canvas.
///
/// The texels are the bytes of an Image: row by row from the top, three bytes each. A
/// texture is looked up by the coordinates of Uv, never by column and row, so a model
/// does not depend on its size.
class Texture {
public:
    /// The texture with the texels of `image`.
    /// Throws std::invalid_argument when a dimension of the image is not positive or its
    /// buffer does not hold width x height x 3 bytes.
    explicit Texture(Image image);

    [[nodiscard]] int width() const noexcept { return image_.width; }
    [[nodiscard]] int height() const noexcept { return image_.height; }

    /// The color of the texture at (u, v): the texel that the point falls in.
    ///
    ///     tx = u * width,   ty = v * height,   each rounded down to a whole number
    ///
    /// and the texel is the one in column tx of row ty, counted from the top-left
    /// corner. A texture of 4 texels across has its first texel from u = 0 to 0.25 and
    /// its last from 0.75 to 1. Rounding down picks one texel and ignores how near the
    /// point is to the next: the book calls this nearest neighbor filtering, the
    /// simplest there is. Every point of one texel has that texel's color, so a texture
    /// seen from close up shows its texels as blocks of one color, and one seen from far
    /// away shows, pixel by pixel, whichever of the many texels under a pixel happened to
    /// be picked. Mending the first takes bilinear filtering and the second mipmapping,
    /// the next steps of the book's chapter; neither is done here.
    ///
    /// The right and bottom edges, u = 1 and v = 1, would be column `width` and row
    /// `height`, one past the last, and belong to the last texel instead. So does
    /// everything beyond an edge: a coordinate outside [0, 1] is taken to the nearest
    /// edge. The corners of a model are at exactly 0 and 1, and a coordinate interpolated
    /// between them can come out a rounding error outside.
    ///
    /// Precondition: u and v are numbers, not NaN; asserted.
    [[nodiscard]] Color GetTexel(double u, double v) const;

private:
    Image image_;
};

}  // namespace cgfs
