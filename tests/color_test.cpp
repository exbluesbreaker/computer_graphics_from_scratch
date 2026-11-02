#include <cgfs/color.h>

#include <catch2/catch_test_macros.hpp>

#include "test_helpers.h"

using cgfs::Color;
using cgfs::Rgb8;
using cgfs::ToRgb8;

TEST_CASE("Color scales by an intensity", "[color]") {
    const Color c{10.0, 20.0, 30.0};
    CHECK(c * 2.0 == Color{20.0, 40.0, 60.0});
    CHECK(0.5 * c == Color{5.0, 10.0, 15.0});
    CHECK(c * 0.0 == Color{0.0, 0.0, 0.0});
}

TEST_CASE("Color addition", "[color]") {
    CHECK(Color{1.0, 2.0, 3.0} + Color{10.0, 20.0, 30.0} == Color{11.0, 22.0, 33.0});
}

TEST_CASE("ToRgb8 clamps to [0, 255] and truncates toward zero", "[color]") {
    CHECK(ToRgb8(Color{254.9, 0.6, 300.0}) == Rgb8{254, 0, 255});
    CHECK(ToRgb8(Color{-5.0, 127.5, 255.0}) == Rgb8{0, 127, 255});
    CHECK(ToRgb8(Color{}) == Rgb8{0, 0, 0});
    static_assert(ToRgb8(Color{255.0, 255.0, 255.0}) == Rgb8{255, 255, 255},
                  "ToRgb8 is constexpr");
}

TEST_CASE("Named colors", "[color]") {
    CHECK(cgfs::colors::kWhite == Color{255.0, 255.0, 255.0});
    CHECK(cgfs::colors::kBlack == Color{0.0, 0.0, 0.0});
    CHECK(cgfs::colors::kRed == Color{255.0, 0.0, 0.0});
    CHECK(cgfs::colors::kGreen == Color{0.0, 255.0, 0.0});
    CHECK(cgfs::colors::kBlue == Color{0.0, 0.0, 255.0});
    CHECK(cgfs::colors::kYellow == Color{255.0, 255.0, 0.0});
}
