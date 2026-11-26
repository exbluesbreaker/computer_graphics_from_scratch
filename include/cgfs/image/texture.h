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

/// How the color of a point of a texture is made from its texels: the filters of the
/// book's chapter, in its order.
///
/// kNearestNeighbor takes the color of the one texel the point falls in. It is the
/// simplest filter there is, and a texture drawn larger than it is shows it: every
/// pixel inside a texel gets that texel's one color, and the texels are blocks.
///
/// kBilinear blends the four texels around the point, each by how near the point is to
/// it, so the color changes smoothly from one texel to the next and the blocks become
/// gradients. It reads four texels where the other reads one.
///
/// Neither helps a texture drawn smaller than it is, where a pixel covers many texels:
/// one of them or four neighbours of them are as much of an accident. That is what
/// mipmapping is for, the chapter's next step. Texture::GetTexel has the formulas.
enum class TextureFilter { kNearestNeighbor, kBilinear };

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

    /// The color of the texture at (u, v), made from its texels by `filter`.
    ///
    ///     tx = u * width,   ty = v * height
    ///
    /// say where the point is among the texels, counted from the top-left corner, as
    /// real numbers: on a texture of 4 texels across, u = 0.3 is tx = 1.2.
    ///
    /// kNearestNeighbor rounds tx and ty down to whole numbers and returns the texel in
    /// column tx of row ty. A texture of 4 texels across has its first texel from u = 0
    /// to 0.25 and its last from 0.75 to 1. Rounding down picks one texel and ignores how
    /// near the point is to the next: the book calls this nearest neighbor filtering.
    /// Every point of one texel has that texel's color, so a texture seen from close up
    /// shows its texels as blocks of one color, and one seen from far away shows, pixel
    /// by pixel, whichever of the many texels under a pixel happened to be picked.
    ///
    /// kBilinear uses what the rounding threw away. With tx and ty rounded down as
    /// before and fx and fy the fractions that were cut off, the point lies among four
    /// texels,
    ///
    ///     TL = texel(tx, ty)        TR = texel(tx + 1, ty)
    ///     BL = texel(tx, ty + 1)    BR = texel(tx + 1, ty + 1)
    ///
    /// the fraction fx of the way from the left two to the right two and fy of the way
    /// from the top two to the bottom two. Its color is interpolated linearly twice,
    /// which is the name: across the top and across the bottom, and then between the two
    /// results.
    ///
    ///     CT = (1 - fx) TL + fx TR
    ///     CB = (1 - fx) BL + fx BR
    ///     C  = (1 - fy) CT + fy CB
    ///
    /// The weight of TR is fx and not 1 - fx: the nearer the point is to the right, the
    /// more it shows of the texel on the right. With fx = fy = 0 the color is TL, the
    /// texel that nearest neighbor filtering returns, and from there it changes evenly
    /// toward the next texels instead of jumping at a border.
    ///
    /// Bilinear filtering takes a texel to be a point, the one at the whole numbers
    /// (tx, ty), which is the top-left corner of the square that nearest neighbor
    /// filtering paints in the texel's color. So the bilinear picture is not the blocks
    /// blurred in place: it lies half a texel nearer the texture's origin, as the book's
    /// listing has it.
    ///
    /// Each of the three is computed as a + f (b - a). That is the same number as
    /// (1 - f) a + f b, and in floating point it is exactly `a` where the two colors are
    /// equal and never outside the two, which the other form does not promise: two texels
    /// of 255 can blend to a rounding error less, and the canvas truncates.
    ///
    /// The right and bottom edges, u = 1 and v = 1, would be column `width` and row
    /// `height`, one past the last, and belong to the last texel instead. So does
    /// everything beyond an edge: a coordinate outside [0, 1] is taken to the nearest
    /// edge. The corners of a model are at exactly 0 and 1, and a coordinate interpolated
    /// between them can come out a rounding error outside. Bilinear filtering asks for a
    /// column right of the last, and a row below it, for every point of the last column
    /// and row: the last stands in for it, so from tx = width - 1 to the right edge the
    /// color is the last column's own, and nothing of the opposite edge shows.
    ///
    /// Precondition: u and v are numbers, not NaN; asserted.
    [[nodiscard]] Color GetTexel(double u, double v,
                                 TextureFilter filter = TextureFilter::kNearestNeighbor) const;

private:
    Image image_;
};

}  // namespace cgfs
