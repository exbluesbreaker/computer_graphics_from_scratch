#include <cgfs/image/texture.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>

namespace cgfs {

namespace {

/// Where `coordinate`, a u or a v, lies among `count` texels in a row or in a column: the
/// coordinate times the count, the book's tx or ty, as a real number, and the place of
/// the first or of the last texel for a coordinate beyond an edge.
double TexelCoordinate(double coordinate, int count) {
    assert(!std::isnan(coordinate) && "Texture::GetTexel: a texture coordinate is not a number");
    // Kept inside the texture while it is still a double: a value far outside the range
    // of an integer could not be converted to one.
    const double last = count - 1;
    return std::clamp(coordinate * count, 0.0, last);
}

/// The texel in column `tx` of row `ty` of `image`, counted from the top-left corner, as
/// a color. Precondition: both are inside the image.
Color Texel(const Image& image, std::size_t tx, std::size_t ty) {
    const std::size_t offset = (ty * static_cast<std::size_t>(image.width) + tx) * 3;
    return {static_cast<double>(image.rgb[offset]), static_cast<double>(image.rgb[offset + 1]),
            static_cast<double>(image.rgb[offset + 2])};
}

/// The color the fraction `f` of the way from `a` to `b`: the book's (1 - f) a + f b,
/// written so that two equal colors give exactly that color and nothing comes out
/// outside the two (see Texture::GetTexel).
Color Blend(Color a, Color b, double f) {
    const auto channel = [f](double from, double to) { return from + f * (to - from); };
    return {channel(a.r, b.r), channel(a.g, b.g), channel(a.b, b.b)};
}

/// The color at (tx, ty) blended from the four texels around it: the book's GetTexel
/// with bilinear filtering. Precondition: tx and ty are inside the image, from 0 to its
/// last column and row.
Color BilinearTexel(const Image& image, double tx, double ty) {
    // The whole parts say which texel is at the top left, and the fractions how far the
    // point is from it toward the next column and the next row. Neither coordinate is
    // negative, so cutting the fraction off rounds down.
    const auto left = static_cast<std::size_t>(tx);
    const auto top = static_cast<std::size_t>(ty);
    const double fx = tx - static_cast<double>(left);
    const double fy = ty - static_cast<double>(top);

    // The last column has none to its right and the last row none below it: they stand
    // in for the missing one, where fx or fy is 0 and its weight with it.
    const std::size_t right = std::min(left + 1, static_cast<std::size_t>(image.width) - 1);
    const std::size_t bottom = std::min(top + 1, static_cast<std::size_t>(image.height) - 1);

    const Color tl = Texel(image, left, top);
    const Color tr = Texel(image, right, top);
    const Color bl = Texel(image, left, bottom);
    const Color br = Texel(image, right, bottom);

    // Interpolate across the top and across the bottom, then between the two.
    const Color ct = Blend(tl, tr, fx);
    const Color cb = Blend(bl, br, fx);
    return Blend(ct, cb, fy);
}

}  // namespace

Texture::Texture(Image image) : image_(std::move(image)) {
    if (image_.width <= 0 || image_.height <= 0) {
        throw std::invalid_argument("Texture dimensions must be positive, got " +
                                    std::to_string(image_.width) + "x" +
                                    std::to_string(image_.height));
    }
    const std::size_t byte_count =
        static_cast<std::size_t>(image_.width) * static_cast<std::size_t>(image_.height) * 3;
    if (image_.rgb.size() != byte_count) {
        throw std::invalid_argument("Texture buffer has " + std::to_string(image_.rgb.size()) +
                                    " bytes, which does not match " + std::to_string(image_.width) +
                                    "x" + std::to_string(image_.height) + " RGB");
    }
}

Color Texture::GetTexel(double u, double v, TextureFilter filter) const {
    const double tx = TexelCoordinate(u, image_.width);
    const double ty = TexelCoordinate(v, image_.height);
    if (filter == TextureFilter::kBilinear) {
        return BilinearTexel(image_, tx, ty);
    }
    // Nearest neighbor filtering: round down, and the texel there is the color.
    return Texel(image_, static_cast<std::size_t>(tx), static_cast<std::size_t>(ty));
}

}  // namespace cgfs
