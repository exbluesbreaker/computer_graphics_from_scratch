#pragma once

#include <algorithm>
#include <cstdint>

namespace cgfs {

/// An RGB color with floating-point channels in the nominal range 0..255.
///
/// Lighting scales colors by an intensity, which can push channels past 255 (or, with
/// later features, below 0). Nothing is clamped until the color is written to the
/// canvas, see ToRgb8.
struct Color {
    double r = 0.0;
    double g = 0.0;
    double b = 0.0;

    constexpr Color& operator*=(double s) {
        r *= s;
        g *= s;
        b *= s;
        return *this;
    }
    constexpr Color& operator+=(Color o) {
        r += o.r;
        g += o.g;
        b += o.b;
        return *this;
    }

    bool operator==(const Color&) const = default;
};

[[nodiscard]] constexpr Color operator*(Color c, double s) { return c *= s; }
[[nodiscard]] constexpr Color operator*(double s, Color c) { return c *= s; }
[[nodiscard]] constexpr Color operator+(Color a, Color b) { return a += b; }

/// A color as stored in the canvas: 8 bits per channel.
struct Rgb8 {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;

    bool operator==(const Rgb8&) const = default;
};

/// Clamp every channel to [0, 255], then truncate toward zero.
///
/// Truncation (not rounding) is the project's convention; the golden images depend on it.
[[nodiscard]] constexpr Rgb8 ToRgb8(Color c) {
    const auto channel = [](double v) {
        return static_cast<std::uint8_t>(std::clamp(v, 0.0, 255.0));
    };
    return {channel(c.r), channel(c.g), channel(c.b)};
}

namespace colors {

inline constexpr Color kWhite{255.0, 255.0, 255.0};
inline constexpr Color kBlack{0.0, 0.0, 0.0};
inline constexpr Color kRed{255.0, 0.0, 0.0};
inline constexpr Color kGreen{0.0, 255.0, 0.0};
inline constexpr Color kBlue{0.0, 0.0, 255.0};
inline constexpr Color kYellow{255.0, 255.0, 0.0};

}  // namespace colors

}  // namespace cgfs
