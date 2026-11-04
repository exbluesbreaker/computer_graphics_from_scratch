#include <cgfs/rasterizer/interpolate.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cstddef>
#include <vector>

using Catch::Matchers::WithinAbs;
using cgfs::Interpolate;

TEST_CASE("Interpolate: a single point gives the start value only", "[rasterizer]") {
    CHECK(Interpolate(3, 7.5, 3, 100.0) == std::vector<double>{7.5});
}

TEST_CASE("Interpolate: one value per integer step, both ends included", "[rasterizer]") {
    const std::vector<double> values = Interpolate(0, 0.0, 4, 8.0);
    REQUIRE(values.size() == 5);
    CHECK(values == std::vector<double>{0.0, 2.0, 4.0, 6.0, 8.0});
}

TEST_CASE("Interpolate: fractional slopes give fractional values", "[rasterizer]") {
    CHECK(Interpolate(0, 0.0, 4, 2.0) == std::vector<double>{0.0, 0.5, 1.0, 1.5, 2.0});
}

TEST_CASE("Interpolate: the dependent variable may decrease", "[rasterizer]") {
    CHECK(Interpolate(-2, 10.0, 2, 2.0) == std::vector<double>{10.0, 8.0, 6.0, 4.0, 2.0});
}

TEST_CASE("Interpolate: equal end values give a constant", "[rasterizer]") {
    CHECK(Interpolate(5, 3.0, 8, 3.0) == std::vector<double>{3.0, 3.0, 3.0, 3.0});
}

TEST_CASE("Interpolate: the independent variable may be negative", "[rasterizer]") {
    CHECK(Interpolate(-5, 1.0, -1, 5.0) == std::vector<double>{1.0, 2.0, 3.0, 4.0, 5.0});
}

TEST_CASE("Interpolate: values[k] is d0 + k * slope", "[rasterizer]") {
    // A slope that is not exactly representable: 1/3. The running sum drifts by a few
    // ulps at most.
    const std::vector<double> values = Interpolate(10, 2.0, 40, 12.0);
    REQUIRE(values.size() == 31);
    for (std::size_t k = 0; k < values.size(); ++k) {
        CHECK_THAT(values[k], WithinAbs(2.0 + static_cast<double>(k) / 3.0, 1e-12));
    }
}

