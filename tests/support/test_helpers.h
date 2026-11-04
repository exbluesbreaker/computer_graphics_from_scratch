#pragma once

// Teaches Catch2 how to print the project's value types in assertion messages.

#include <cgfs/image/color.h>
#include <cgfs/math/vec3.h>
#include <cgfs/rasterizer/point.h>

#include <catch2/catch_tostring.hpp>
#include <string>

namespace Catch {

template <>
struct StringMaker<cgfs::Vec3> {
    static std::string convert(const cgfs::Vec3& v) {
        return "(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ", " +
               std::to_string(v.z) + ")";
    }
};

template <>
struct StringMaker<cgfs::Color> {
    static std::string convert(const cgfs::Color& c) {
        return "Color(" + std::to_string(c.r) + ", " + std::to_string(c.g) + ", " +
               std::to_string(c.b) + ")";
    }
};

template <>
struct StringMaker<cgfs::Rgb8> {
    static std::string convert(const cgfs::Rgb8& c) {
        return "Rgb8(" + std::to_string(c.r) + ", " + std::to_string(c.g) + ", " +
               std::to_string(c.b) + ")";
    }
};

template <>
struct StringMaker<cgfs::Point> {
    static std::string convert(const cgfs::Point& p) {
        return "(" + std::to_string(p.x) + ", " + std::to_string(p.y) + ")";
    }
};

}  // namespace Catch

#include <chrono>
#include <filesystem>
#include <string>

namespace cgfs_test {

/// A path in the system temp directory that is removed when the object goes away.
class TempFile {
public:
    explicit TempFile(const std::string& suffix)
        : path_(std::filesystem::temp_directory_path() /
                ("cgfs_test_" +
                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                 suffix)) {}
    TempFile(const TempFile&) = delete;
    TempFile& operator=(const TempFile&) = delete;
    ~TempFile() {
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
    }

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

}  // namespace cgfs_test
