#pragma once

// Teaches Catch2 how to print the project's value types in assertion messages.

#include <cgfs/image/color.h>
#include <cgfs/image/texture.h>
#include <cgfs/lighting/light.h>
#include <cgfs/math/mat4.h>
#include <cgfs/math/plane.h>
#include <cgfs/math/vec3.h>
#include <cgfs/rasterizer/object.h>
#include <cgfs/rasterizer/point.h>

#include <catch2/catch_tostring.hpp>
#include <string>
#include <variant>

namespace Catch {

template <>
struct StringMaker<cgfs::Vec3> {
    static std::string convert(const cgfs::Vec3& v) {
        return "(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ", " +
               std::to_string(v.z) + ")";
    }
};

template <>
struct StringMaker<cgfs::Mat4> {
    static std::string convert(const cgfs::Mat4& matrix) {
        std::string text = "Mat4[";
        for (int row = 0; row < 4; ++row) {
            text += row == 0 ? "[" : " [";
            for (int col = 0; col < 4; ++col) {
                text += (col == 0 ? "" : ", ") + std::to_string(matrix.m[row][col]);
            }
            text += "]";
        }
        return text + "]";
    }
};

template <>
struct StringMaker<cgfs::Plane> {
    static std::string convert(const cgfs::Plane& plane) {
        return "Plane(N=" + StringMaker<cgfs::Vec3>::convert(plane.normal) +
               ", D=" + std::to_string(plane.d) + ")";
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
struct StringMaker<cgfs::Uv> {
    static std::string convert(const cgfs::Uv& uv) {
        return "(u=" + std::to_string(uv.u) + ", v=" + std::to_string(uv.v) + ")";
    }
};

template <>
struct StringMaker<cgfs::Point> {
    static std::string convert(const cgfs::Point& p) {
        return "(" + std::to_string(p.x) + ", " + std::to_string(p.y) +
               ", h=" + std::to_string(p.h) + ", 1/z=" + std::to_string(p.inv_z) +
               ", uv=" + StringMaker<cgfs::Uv>::convert(p.uv) + ")";
    }
};

template <>
struct StringMaker<cgfs::Triangle> {
    static std::string convert(const cgfs::Triangle& t) {
        return "Triangle(v=" + std::to_string(t.v[0]) + " " + std::to_string(t.v[1]) + " " +
               std::to_string(t.v[2]) + ", " + StringMaker<cgfs::Color>::convert(t.color) +
               ", h=" + std::to_string(t.h[0]) + " " + std::to_string(t.h[1]) + " " +
               std::to_string(t.h[2]) +
               ", normals=" + StringMaker<cgfs::Vec3>::convert(t.normals[0]) + " " +
               StringMaker<cgfs::Vec3>::convert(t.normals[1]) + " " +
               StringMaker<cgfs::Vec3>::convert(t.normals[2]) +
               ", uvs=" + StringMaker<cgfs::Uv>::convert(t.uvs[0]) + " " +
               StringMaker<cgfs::Uv>::convert(t.uvs[1]) + " " +
               StringMaker<cgfs::Uv>::convert(t.uvs[2]) + ")";
    }
};

template <>
struct StringMaker<cgfs::Light> {
    static std::string convert(const cgfs::Light& light) {
        return std::visit(cgfs::Overloaded{
                              [](const cgfs::AmbientLight& l) {
                                  return "AmbientLight(" + std::to_string(l.intensity) + ")";
                              },
                              [](const cgfs::PointLight& l) {
                                  return "PointLight(" + std::to_string(l.intensity) + ", at " +
                                         StringMaker<cgfs::Vec3>::convert(l.position) + ")";
                              },
                              [](const cgfs::DirectionalLight& l) {
                                  return "DirectionalLight(" + std::to_string(l.intensity) +
                                         ", toward " +
                                         StringMaker<cgfs::Vec3>::convert(l.direction) + ")";
                              },
                          },
                          light);
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
