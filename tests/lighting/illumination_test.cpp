#include <cgfs/lighting/illumination.h>
#include <cgfs/math/vec3.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>

#include "support/test_helpers.h"

using Catch::Matchers::WithinAbs;
using cgfs::DiffuseReflection;
using cgfs::kMatte;
using cgfs::SpecularReflection;
using cgfs::Vec3;

namespace {

const Vec3 kUp{0.0, 1.0, 0.0};

// The specular tests share one setup: a surface facing up, lit from 45 degrees along
// L = (1, 1, 0). The light mirrors off toward R = (-1, 1, 0).
const Vec3 kFromTheSide{1.0, 1.0, 0.0};
const Vec3 kMirror{-1.0, 1.0, 0.0};

}  // namespace

TEST_CASE("kMatte is the book's -1", "[lighting]") { CHECK(kMatte == -1.0); }

TEST_CASE("DiffuseReflection: a light straight above the surface arrives at full intensity",
          "[lighting]") {
    CHECK(DiffuseReflection(0.8, kUp, kUp) == 0.8);
    CHECK(DiffuseReflection(1.0, Vec3{0.0, 0.0, -1.0}, Vec3{0.0, 0.0, -1.0}) == 1.0);
}

TEST_CASE("DiffuseReflection: the intensity follows the cosine of the angle", "[lighting]") {
    const Vec3 tilted{0.0, 0.5, std::sqrt(3.0) / 2};  // 60 degrees away from the normal
    CHECK_THAT(DiffuseReflection(1.0, kUp, tilted), WithinAbs(0.5, 1e-12));
    CHECK_THAT(DiffuseReflection(0.6, kUp, kFromTheSide), WithinAbs(0.6 / std::sqrt(2.0), 1e-12));
}

TEST_CASE("DiffuseReflection: a light behind the surface, or grazing it, adds nothing",
          "[lighting]") {
    CHECK(DiffuseReflection(0.8, kUp, -kUp) == 0.0);
    CHECK(DiffuseReflection(0.8, kUp, Vec3{1.0, -0.001, 0.0}) == 0.0);
    CHECK(DiffuseReflection(0.8, kUp, Vec3{1.0, 0.0, 0.0}) == 0.0);  // <N, L> = 0
}

TEST_CASE("DiffuseReflection: neither vector has to be normalized", "[lighting]") {
    CHECK_THAT(DiffuseReflection(0.7, kUp, 10.0 * kUp), WithinAbs(0.7, 1e-12));
    CHECK_THAT(DiffuseReflection(0.7, 4.0 * kUp, 0.001 * kUp), WithinAbs(0.7, 1e-12));
    CHECK_THAT(DiffuseReflection(1.0, 3.0 * kUp, 5.0 * kFromTheSide),
               WithinAbs(1.0 / std::sqrt(2.0), 1e-12));
}

TEST_CASE("SpecularReflection: the full intensity is seen in the mirror direction", "[lighting]") {
    // cos 0 = 1 to any power.
    CHECK_THAT(SpecularReflection(1.0, kUp, kFromTheSide, kMirror, 10.0), WithinAbs(1.0, 1e-12));
    CHECK_THAT(SpecularReflection(0.5, kUp, kFromTheSide, kMirror, 500.0), WithinAbs(0.5, 1e-12));
    // A light straight above mirrors straight back up.
    CHECK_THAT(SpecularReflection(0.8, kUp, kUp, kUp, 50.0), WithinAbs(0.8, 1e-12));
}

TEST_CASE("SpecularReflection: the highlight fades as cos^s away from the mirror direction",
          "[lighting]") {
    // Looking straight down the normal is 45 degrees off the mirror direction.
    CHECK_THAT(SpecularReflection(1.0, kUp, kFromTheSide, kUp, 2.0),
               WithinAbs(0.5, 1e-12));  // (1 / sqrt(2))^2
    CHECK_THAT(SpecularReflection(1.0, kUp, kFromTheSide, kUp, 10.0),
               WithinAbs(1.0 / 32, 1e-12));  // (1 / sqrt(2))^10
    // A larger exponent gives a smaller highlight: less of it is left 45 degrees away.
    CHECK(SpecularReflection(1.0, kUp, kFromTheSide, kUp, 50.0) <
          SpecularReflection(1.0, kUp, kFromTheSide, kUp, 10.0));
}

TEST_CASE(
    "SpecularReflection: nothing is seen from more than 90 degrees off the mirror "
    "direction",
    "[lighting]") {
    // From the light's own side <R, V> < 0.
    CHECK(SpecularReflection(1.0, kUp, kFromTheSide, Vec3{1.0, 0.5, 0.0}, 10.0) == 0.0);
    CHECK(SpecularReflection(1.0, kUp, kFromTheSide, -kMirror, 10.0) == 0.0);
    CHECK(SpecularReflection(1.0, kUp, kFromTheSide, kFromTheSide, 10.0) == 0.0);  // <R, V> = 0
}

TEST_CASE("SpecularReflection: the lengths of L and V do not matter", "[lighting]") {
    CHECK_THAT(SpecularReflection(1.0, kUp, 10.0 * kFromTheSide, 0.25 * kMirror, 10.0),
               WithinAbs(1.0, 1e-12));
    CHECK_THAT(SpecularReflection(1.0, kUp, 10.0 * kFromTheSide, 3.0 * kUp, 2.0),
               WithinAbs(0.5, 1e-12));
}

TEST_CASE("SpecularReflection: the term is not gated on the light being in front of the surface",
          "[lighting]") {
    // The book's equation: only the diffuse term asks for <N, L> > 0. A light below the
    // surface mirrors to below it, where a viewer could still be asked about.
    const Vec3 from_below{1.0, -1.0, 0.0};
    const Vec3 mirror_below{-1.0, -1.0, 0.0};
    CHECK(DiffuseReflection(1.0, kUp, from_below) == 0.0);
    CHECK_THAT(SpecularReflection(1.0, kUp, from_below, mirror_below, 10.0), WithinAbs(1.0, 1e-12));
}
