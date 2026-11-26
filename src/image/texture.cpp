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

/// The color the fraction `f` of the way from `a` to `b`: the book's (1 - f) a + f b,
/// written so that two equal colors give exactly that color and nothing comes out
/// outside the two (see Texture::GetTexel).
Color Blend(Color a, Color b, double f) {
    const auto channel = [f](double from, double to) { return from + f * (to - from); };
    return {channel(a.r, b.r), channel(a.g, b.g), channel(a.b, b.b)};
}

}  // namespace

Texture::Texture(Image image, Mipmapping mipmapping) : image_(std::move(image)) {
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
    if (mipmapping == Mipmapping::kOn) {
        // Halve the last level until it is one texel: each level is made of the one
        // before it.
        while (LevelWidth(level_count() - 1) > 1 || LevelHeight(level_count() - 1) > 1) {
            reductions_.push_back(Halved(level_count() - 1));
        }
    }
}

int Texture::LevelWidth(int level) const {
    return level == 0 ? image_.width : reductions_[static_cast<std::size_t>(level) - 1].width;
}

int Texture::LevelHeight(int level) const {
    return level == 0 ? image_.height : reductions_[static_cast<std::size_t>(level) - 1].height;
}

Color Texture::Texel(int level, std::size_t tx, std::size_t ty) const {
    if (level == 0) {
        // The texture itself: three bytes of the image.
        const std::size_t offset = (ty * static_cast<std::size_t>(image_.width) + tx) * 3;
        return {static_cast<double>(image_.rgb[offset]),
                static_cast<double>(image_.rgb[offset + 1]),
                static_cast<double>(image_.rgb[offset + 2])};
    }
    const Reduction& reduction = reductions_[static_cast<std::size_t>(level) - 1];
    return reduction.texels[ty * static_cast<std::size_t>(reduction.width) + tx];
}

Color Texture::BilinearTexel(int level, double tx, double ty) const {
    // The whole parts say which texel is at the top left, and the fractions how far the
    // point is from it toward the next column and the next row. Neither coordinate is
    // negative, so cutting the fraction off rounds down.
    const auto left = static_cast<std::size_t>(tx);
    const auto top = static_cast<std::size_t>(ty);
    const double fx = tx - static_cast<double>(left);
    const double fy = ty - static_cast<double>(top);

    // The last column has none to its right and the last row none below it: they stand
    // in for the missing one, where fx or fy is 0 and its weight with it.
    const std::size_t right = std::min(left + 1, static_cast<std::size_t>(LevelWidth(level)) - 1);
    const std::size_t bottom = std::min(top + 1, static_cast<std::size_t>(LevelHeight(level)) - 1);

    const Color tl = Texel(level, left, top);
    const Color tr = Texel(level, right, top);
    const Color bl = Texel(level, left, bottom);
    const Color br = Texel(level, right, bottom);

    // Interpolate across the top and across the bottom, then between the two.
    const Color ct = Blend(tl, tr, fx);
    const Color cb = Blend(bl, br, fx);
    return Blend(ct, cb, fy);
}

Texture::Reduction Texture::Halved(int level) const {
    const int width = LevelWidth(level);
    const int height = LevelHeight(level);
    Reduction half{
        .width = std::max(width / 2, 1), .height = std::max(height / 2, 1), .texels = {}};
    half.texels.reserve(static_cast<std::size_t>(half.width) *
                        static_cast<std::size_t>(half.height));
    for (int y = 0; y < half.height; ++y) {
        for (int x = 0; x < half.width; ++x) {
            // The four texels this one covers. A level one texel wide has no second
            // column, and one that is one texel high no second row: the first is taken
            // again.
            const auto left = static_cast<std::size_t>(2 * x);
            const auto top = static_cast<std::size_t>(2 * y);
            const std::size_t right = std::min(left + 1, static_cast<std::size_t>(width) - 1);
            const std::size_t bottom = std::min(top + 1, static_cast<std::size_t>(height) - 1);
            const Color sum = Texel(level, left, top) + Texel(level, right, top) +
                              Texel(level, left, bottom) + Texel(level, right, bottom);
            half.texels.push_back(sum * 0.25);
        }
    }
    return half;
}

Color Texture::GetTexel(double u, double v, TextureFilter filter, int level) const {
    assert(level >= 0 && level < level_count() && "Texture::GetTexel: no such mipmap level");
    const double tx = TexelCoordinate(u, LevelWidth(level));
    const double ty = TexelCoordinate(v, LevelHeight(level));
    if (filter == TextureFilter::kBilinear) {
        return BilinearTexel(level, tx, ty);
    }
    // Nearest neighbor filtering: round down, and the texel there is the color.
    return Texel(level, static_cast<std::size_t>(tx), static_cast<std::size_t>(ty));
}

}  // namespace cgfs
