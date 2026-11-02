#pragma once

// Teaches Catch2 how to print the project's value types in assertion messages.

#include <cgfs/color.h>
#include <cgfs/vec3.h>

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

}  // namespace Catch
