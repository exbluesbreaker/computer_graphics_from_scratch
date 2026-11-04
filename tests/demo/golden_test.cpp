// Golden-image tests: the renderer must reproduce the reference renders of the demo
// scenes in tests/demo/golden/. The README explains how to regenerate them.

#include <cgfs/camera/viewport.h>
#include <cgfs/demo/rasterizer_scenes.h>
#include <cgfs/demo/raytracer_scenes.h>
#include <cgfs/image/canvas.h>
#include <cgfs/image/image_io.h>
#include <cgfs/raytracer/raytracer.h>

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <filesystem>
#include <string>

#include "support/test_helpers.h"

namespace {

// Every channel of every pixel must be within this distance of the reference. The slack
// absorbs floating-point differences between compilers and standard libraries.
constexpr int kTolerance = 1;

std::filesystem::path GoldenPath(const std::string& file) {
    return std::filesystem::path(CGFS_GOLDEN_DIR) / file;
}

// Every channel of every pixel of `canvas` must match `golden` within kTolerance.
void CheckCanvasMatches(const cgfs::Canvas& canvas, const cgfs::Image& golden,
                        const std::filesystem::path& path) {
    INFO("golden image: " << path.string());
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

// Ray-traces `scene` at the reference image's size with the aspect-matched viewport.
void CheckRenderMatchesGolden(const cgfs::Scene& scene, const std::string& file) {
    const std::filesystem::path path = GoldenPath(file);
    const cgfs::Image golden = cgfs::ReadPpm(path);

    cgfs::Canvas canvas(golden.width, golden.height);
    const cgfs::Viewport viewport = cgfs::Viewport::MatchingCanvasAspect(canvas);
    cgfs::RayTracer(scene, viewport).Render(canvas);
    CheckCanvasMatches(canvas, golden, path);
}

// Lets `draw` paint a canvas of the reference image's size.
void CheckDrawingMatchesGolden(void (*draw)(cgfs::Canvas&), const std::string& file) {
    const std::filesystem::path path = GoldenPath(file);
    const cgfs::Image golden = cgfs::ReadPpm(path);

    cgfs::Canvas canvas(golden.width, golden.height);
    draw(canvas);
    CheckCanvasMatches(canvas, golden, path);
}

}  // namespace

TEST_CASE("flat scene matches the reference render", "[golden]") {
    CheckRenderMatchesGolden(cgfs::demo::FlatScene(), "flat_320x180.ppm");
}

TEST_CASE("diffuse scene matches the reference render", "[golden]") {
    CheckRenderMatchesGolden(cgfs::demo::DiffuseScene(), "diffuse_320x180.ppm");
}

TEST_CASE("specular scene matches the reference render", "[golden]") {
    CheckRenderMatchesGolden(cgfs::demo::SpecularScene(), "specular_320x180.ppm");
}

TEST_CASE("reflective scene matches the reference render", "[golden]") {
    CheckRenderMatchesGolden(cgfs::demo::ReflectiveScene(), "reflective_320x180.ppm");
}

TEST_CASE("mirror ball scene matches the reference render", "[golden]") {
    CheckRenderMatchesGolden(cgfs::demo::MirrorBallScene(), "mirrorball_320x180.ppm");
}

TEST_CASE("lines scene matches the reference drawing", "[golden]") {
    CheckDrawingMatchesGolden(cgfs::demo::DrawLinesScene, "lines_320x180.ppm");
}

TEST_CASE("triangles scene matches the reference drawing", "[golden]") {
    CheckDrawingMatchesGolden(cgfs::demo::DrawTrianglesScene, "triangles_320x180.ppm");
}
