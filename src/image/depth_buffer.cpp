#include <cgfs/image/depth_buffer.h>
#include <cgfs/image/pixel_index.h>

#include <cstddef>
#include <stdexcept>
#include <string>

namespace cgfs {

DepthBuffer::DepthBuffer(int width, int height) : width_(width), height_(height) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("DepthBuffer dimensions must be positive, got " +
                                    std::to_string(width) + "x" + std::to_string(height));
    }
    // 0 is 1/z of a surface infinitely far away.
    inv_z_.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0.0);
}

DepthBuffer::DepthBuffer(const Canvas& canvas) : DepthBuffer(canvas.width(), canvas.height()) {}

bool DepthBuffer::UpdateIfCloser(int x, int y, double inv_z) noexcept {
    const auto index = PixelIndex(width_, height_, x, y);
    if (!index || !(inv_z > inv_z_[*index])) {
        return false;
    }
    inv_z_[*index] = inv_z;
    return true;
}

std::optional<double> DepthBuffer::InvZ(int x, int y) const noexcept {
    const auto index = PixelIndex(width_, height_, x, y);
    if (!index) {
        return std::nullopt;
    }
    return inv_z_[*index];
}

}  // namespace cgfs
