#include <cgfs/image/canvas.h>

#include <cgfs/image/image_io.h>
#include <cgfs/image/pixel_index.h>

#include <cctype>
#include <stdexcept>
#include <string>

namespace cgfs {

Canvas::Canvas(int width, int height, Color background)
    : width_(width), height_(height), background_(background) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("Canvas dimensions must be positive, got " +
                                    std::to_string(width) + "x" + std::to_string(height));
    }
    pixels_.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3);
    Clear();
}

void Canvas::Clear() {
    const Rgb8 fill = ToRgb8(background_);
    for (std::size_t i = 0; i < pixels_.size(); i += 3) {
        pixels_[i] = fill.r;
        pixels_[i + 1] = fill.g;
        pixels_[i + 2] = fill.b;
    }
}

void Canvas::Clear(Color color) {
    background_ = color;
    Clear();
}

void Canvas::PutPixel(int x, int y, Color color) {
    if (const auto offset = Offset(x, y)) {
        const Rgb8 rgb = ToRgb8(color);
        pixels_[*offset] = rgb.r;
        pixels_[*offset + 1] = rgb.g;
        pixels_[*offset + 2] = rgb.b;
    }
}

std::optional<Rgb8> Canvas::GetPixel(int x, int y) const {
    const auto offset = Offset(x, y);
    if (!offset) {
        return std::nullopt;
    }
    return Rgb8{pixels_[*offset], pixels_[*offset + 1], pixels_[*offset + 2]};
}

std::optional<std::size_t> Canvas::Offset(int x, int y) const noexcept {
    const auto index = PixelIndex(width_, height_, x, y);
    if (!index) {
        return std::nullopt;
    }
    return *index * 3;  // three bytes per pixel
}

void Canvas::Save(const std::filesystem::path& path) const {
    std::string extension = path.extension().string();
    for (char& c : extension) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (extension == ".ppm") {
        WritePpm(path, width_, height_, pixels_);
    } else if (extension == ".png") {
        WritePng(path, width_, height_, pixels_);
    } else {
        throw std::runtime_error("unsupported image format '" + extension + "' for " +
                                 path.string() + " (use .ppm or .png)");
    }
}

}  // namespace cgfs
