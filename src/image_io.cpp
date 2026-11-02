#include <cgfs/image_io.h>

#include <stb_image_write.h>

#include <cstddef>
#include <fstream>
#include <stdexcept>
#include <string>

namespace cgfs {

namespace {

std::size_t ByteCount(int width, int height) {
    return static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3;
}

void CheckDimensions(int width, int height, std::size_t byte_count) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("image dimensions must be positive, got " +
                                    std::to_string(width) + "x" + std::to_string(height));
    }
    if (byte_count != ByteCount(width, height)) {
        throw std::invalid_argument("pixel buffer has " + std::to_string(byte_count) +
                                    " bytes, which does not match " + std::to_string(width) +
                                    "x" + std::to_string(height) + " RGB");
    }
}

[[noreturn]] void ThrowIoError(const std::string& what, const std::filesystem::path& path) {
    throw std::runtime_error(what + ": " + path.string());
}

}  // namespace

void WritePpm(const std::filesystem::path& path, int width, int height,
              std::span<const std::uint8_t> rgb) {
    CheckDimensions(width, height, rgb.size());
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        ThrowIoError("cannot open file for writing", path);
    }
    out << "P6\n" << width << ' ' << height << "\n255\n";
    out.write(reinterpret_cast<const char*>(rgb.data()), static_cast<std::streamsize>(rgb.size()));
    if (!out) {
        ThrowIoError("failed to write PPM", path);
    }
}

void WritePng(const std::filesystem::path& path, int width, int height,
              std::span<const std::uint8_t> rgb) {
    CheckDimensions(width, height, rgb.size());
    const std::string file = path.string();
    if (stbi_write_png(file.c_str(), width, height, 3, rgb.data(), width * 3) == 0) {
        ThrowIoError("failed to write PNG", path);
    }
}

Image ReadPpm(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        ThrowIoError("cannot open file for reading", path);
    }
    std::string magic;
    int maxval = 0;
    Image image;
    in >> magic >> image.width >> image.height >> maxval;
    if (!in || magic != "P6") {
        ThrowIoError("not a binary PPM (P6) image", path);
    }
    if (image.width <= 0 || image.height <= 0) {
        ThrowIoError("invalid PPM dimensions", path);
    }
    if (maxval != 255) {
        ThrowIoError("only 8-bit PPM images are supported", path);
    }
    in.get();  // the single whitespace byte between the header and the pixel data

    const std::size_t byte_count = ByteCount(image.width, image.height);
    image.rgb.resize(byte_count);
    in.read(reinterpret_cast<char*>(image.rgb.data()), static_cast<std::streamsize>(byte_count));
    if (static_cast<std::size_t>(in.gcount()) != byte_count) {
        ThrowIoError("truncated PPM pixel data", path);
    }
    return image;
}

}  // namespace cgfs
