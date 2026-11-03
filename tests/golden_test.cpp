// Golden-image tests: the renderer must reproduce the reference renders of the demo
// scenes in tests/golden/. The README explains how to regenerate them.

#include <cgfs/canvas.h>
#include <cgfs/demo_scenes.h>
#include <cgfs/image_io.h>
#include <cgfs/raytracer.h>
#include <cgfs/viewport.h>

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <filesystem>
#include <string>

#include "test_helpers.h"

namespace {

// Every channel of every pixel must be within this distance of the reference. The slack
// absorbs floating-point differences between compilers and standard libraries.
constexpr int kTolerance = 1;

void CheckMatchesGolden(const cgfs::Scene& scene, const std::string& file) {
    const std::filesystem::path path = std::filesystem::path(CGFS_GOLDEN_DIR) / file;
    INFO("golden image: " << path.string());
    const cgfs::Image golden = cgfs::ReadPpm(path);

    cgfs::Canvas canvas(golden.width, golden.height);
    const cgfs::Viewport viewport = cgfs::Viewport::MatchingCanvasAspect(canvas);
    cgfs::RayTracer(scene, viewport).Render(canvas);

    const auto data = canvas.Data();
    REQUIRE(data.size() == golden.rgb.size());

    std::size_t mismatches = 0;
    std::size_t first_mismatch = 0;
    int max_difference = 0;
    for (std::size_t i = 0; i < data.size(); ++i) {
        const int difference = std::abs(static_cast<int>(data[i]) - static_cast<int>(golden.rgb[i]));
        max_difference = std::max(max_difference, difference);
        if (difference > kTolerance) {
            if (mismatches == 0) {
                first_mismatch = i;
            }
            ++mismatches;
        }
    }

    const auto pixel = first_mismatch / 3;
    const auto row = static_cast<int>(pixel) / golden.width;
    const auto column = static_cast<int>(pixel) % golden.width;
    INFO("channels off by more than " << kTolerance << ": " << mismatches
                                      << ", largest difference: " << max_difference
                                      << ", first mismatch at row " << row << ", column "
                                      << column);
    CHECK(mismatches == 0);
}

}  // namespace

TEST_CASE("flat scene matches the reference render", "[golden]") {
    CheckMatchesGolden(cgfs::demo::FlatScene(), "flat_320x180.ppm");
}

TEST_CASE("diffuse scene matches the reference render", "[golden]") {
    CheckMatchesGolden(cgfs::demo::DiffuseScene(), "diffuse_320x180.ppm");
}

TEST_CASE("specular scene matches the reference render", "[golden]") {
    CheckMatchesGolden(cgfs::demo::SpecularScene(), "specular_320x180.ppm");
}

TEST_CASE("reflective scene matches the reference render", "[golden]") {
    CheckMatchesGolden(cgfs::demo::ReflectiveScene(), "reflective_320x180.ppm");
}
