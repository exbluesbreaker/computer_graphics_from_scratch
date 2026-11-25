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

/// The index of the texel that `coordinate`, a u or a v, falls in, of `count` texels in
/// a row or in a column: the coordinate times the count, rounded down, and the first or
/// the last texel for a coordinate beyond an edge.
std::size_t TexelIndex(double coordinate, int count) {
    assert(!std::isnan(coordinate) && "Texture::GetTexel: a texture coordinate is not a number");
    // Clamped while it is still a double: a value far outside the range of an int could
    // not be converted to one.
    const double last = count - 1;
    return static_cast<std::size_t>(std::clamp(coordinate * count, 0.0, last));
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

Color Texture::GetTexel(double u, double v) const {
    const std::size_t tx = TexelIndex(u, image_.width);
    const std::size_t ty = TexelIndex(v, image_.height);
    const std::size_t offset = (ty * static_cast<std::size_t>(image_.width) + tx) * 3;
    return {static_cast<double>(image_.rgb[offset]), static_cast<double>(image_.rgb[offset + 1]),
            static_cast<double>(image_.rgb[offset + 2])};
}

}  // namespace cgfs
