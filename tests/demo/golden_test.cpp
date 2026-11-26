// Golden-image tests: the renderer must reproduce the reference renders of the demo
// scenes in tests/demo/golden/. The README explains how to regenerate them.

#include <cgfs/camera/viewport.h>
#include <cgfs/demo/rasterizer_scenes.h>
#include <cgfs/demo/raytracer_scenes.h>
#include <cgfs/demo/textures.h>
#include <cgfs/image/canvas.h>
#include <cgfs/image/image_io.h>
#include <cgfs/image/texture.h>
#include <cgfs/rasterizer/shading.h>
#include <cgfs/rasterizer/triangle.h>
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

// How a painted picture is drawn: what carries its texture across a triangle, what makes
// a color of the texels, and whether the texture has its mipmap levels.
struct Painting {
    cgfs::TextureMapping texture_mapping = cgfs::TextureMapping::kPerspectiveCorrect;
    cgfs::TextureFilter texture_filter = cgfs::TextureFilter::kNearestNeighbor;
    cgfs::Mipmapping mipmapping = cgfs::Mipmapping::kOff;
};

// Draws the textures scene with the crate texture, as `painting` says, on a canvas of
// the reference image's size.
void CheckTexturesSceneMatchesGolden(const Painting& painting, const std::string& file) {
    const std::filesystem::path path = GoldenPath(file);
    const cgfs::Image golden = cgfs::ReadPpm(path);
    const cgfs::Texture crate = cgfs::demo::CrateTexture(painting.mipmapping);

    cgfs::Canvas canvas(golden.width, golden.height);
    cgfs::demo::DrawTexturesScene(canvas, cgfs::Viewport::MatchingCanvasAspect(canvas), crate,
                                  painting.texture_mapping, painting.texture_filter);
    CheckCanvasMatches(canvas, golden, path);
}

// Draws the checkerboard scene with the checkerboard texture, as `painting` says, on a
// canvas of the reference image's size.
void CheckCheckerboardSceneMatchesGolden(const Painting& painting, const std::string& file) {
    const std::filesystem::path path = GoldenPath(file);
    const cgfs::Image golden = cgfs::ReadPpm(path);
    const cgfs::Texture checkerboard = cgfs::demo::CheckerboardTexture(painting.mipmapping);

    cgfs::Canvas canvas(golden.width, golden.height);
    cgfs::demo::DrawCheckerboardScene(canvas, cgfs::Viewport::MatchingCanvasAspect(canvas),
                                      checkerboard, painting.texture_mapping,
                                      painting.texture_filter);
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

TEST_CASE("shaded triangles scene matches the reference drawing", "[golden]") {
    CheckDrawingMatchesGolden(cgfs::demo::DrawShadedTrianglesScene, "shaded_320x180.ppm");
}

TEST_CASE("cube scene matches the reference drawing", "[golden]") {
    CheckDrawingMatchesGolden(
        [](cgfs::Canvas& canvas) {
            cgfs::demo::DrawCubeScene(canvas, cgfs::Viewport::MatchingCanvasAspect(canvas));
        },
        "cube_320x180.ppm");
}

TEST_CASE("objects scene matches the reference drawing", "[golden]") {
    CheckDrawingMatchesGolden(
        [](cgfs::Canvas& canvas) {
            cgfs::demo::DrawObjectsScene(canvas, cgfs::Viewport::MatchingCanvasAspect(canvas));
        },
        "objects_320x180.ppm");
}

TEST_CASE("instances scene matches the reference drawing", "[golden]") {
    CheckDrawingMatchesGolden(
        [](cgfs::Canvas& canvas) {
            cgfs::demo::DrawInstancesScene(canvas, cgfs::Viewport::MatchingCanvasAspect(canvas));
        },
        "instances_320x180.ppm");
}

TEST_CASE("camera scene matches the reference drawing", "[golden]") {
    CheckDrawingMatchesGolden(
        [](cgfs::Canvas& canvas) {
            cgfs::demo::DrawCameraScene(canvas, cgfs::Viewport::MatchingCanvasAspect(canvas));
        },
        "camera_320x180.ppm");
}

TEST_CASE("clipping scene matches the reference drawing", "[golden]") {
    CheckDrawingMatchesGolden(
        [](cgfs::Canvas& canvas) {
            cgfs::demo::DrawClippingScene(canvas, cgfs::Viewport::MatchingCanvasAspect(canvas));
        },
        "clipping_320x180.ppm");
}

TEST_CASE("filled cubes scene matches the reference drawing", "[golden]") {
    CheckDrawingMatchesGolden(
        [](cgfs::Canvas& canvas) {
            cgfs::demo::DrawFilledCubesScene(canvas, cgfs::Viewport::MatchingCanvasAspect(canvas));
        },
        "filled_320x180.ppm");
}

// The shading scene is one scene and four pictures, one per shading model.

TEST_CASE("shading scene matches the reference drawing under flat shading", "[golden]") {
    CheckDrawingMatchesGolden(
        [](cgfs::Canvas& canvas) {
            cgfs::demo::DrawShadingScene(canvas, cgfs::Viewport::MatchingCanvasAspect(canvas),
                                         cgfs::ShadingModel::kFlat);
        },
        "shading_flat_320x180.ppm");
}

TEST_CASE("shading scene matches the reference drawing under faceted Gouraud shading", "[golden]") {
    CheckDrawingMatchesGolden(
        [](cgfs::Canvas& canvas) {
            cgfs::demo::DrawShadingScene(canvas, cgfs::Viewport::MatchingCanvasAspect(canvas),
                                         cgfs::ShadingModel::kGouraudFaceted);
        },
        "shading_gouraud_faceted_320x180.ppm");
}

TEST_CASE("shading scene matches the reference drawing under Gouraud shading", "[golden]") {
    CheckDrawingMatchesGolden(
        [](cgfs::Canvas& canvas) {
            cgfs::demo::DrawShadingScene(canvas, cgfs::Viewport::MatchingCanvasAspect(canvas),
                                         cgfs::ShadingModel::kGouraud);
        },
        "shading_gouraud_320x180.ppm");
}

TEST_CASE("shading scene matches the reference drawing under Phong shading", "[golden]") {
    CheckDrawingMatchesGolden(
        [](cgfs::Canvas& canvas) {
            cgfs::demo::DrawShadingScene(canvas, cgfs::Viewport::MatchingCanvasAspect(canvas),
                                         cgfs::ShadingModel::kPhong);
        },
        "shading_phong_320x180.ppm");
}

// The textures scene is one scene and four pictures: one per way of carrying a texture
// across a triangle, both looked up by nearest neighbor filtering; the right one of the
// two looked up by bilinear filtering; and that one again with a texture that has its
// mipmap levels, which is everything the chapter has so far at once.

TEST_CASE("textures scene matches the reference drawing under perspective-correct mapping",
          "[golden]") {
    CheckTexturesSceneMatchesGolden({}, "textures_perspective_correct_320x180.ppm");
}

TEST_CASE("textures scene matches the reference drawing under linear mapping", "[golden]") {
    CheckTexturesSceneMatchesGolden({.texture_mapping = cgfs::TextureMapping::kLinear},
                                    "textures_linear_320x180.ppm");
}

TEST_CASE("textures scene matches the reference drawing under bilinear filtering", "[golden]") {
    CheckTexturesSceneMatchesGolden({.texture_filter = cgfs::TextureFilter::kBilinear},
                                    "textures_bilinear_320x180.ppm");
}

TEST_CASE("textures scene matches the reference drawing with mipmapping and bilinear filtering",
          "[golden]") {
    CheckTexturesSceneMatchesGolden(
        {.texture_filter = cgfs::TextureFilter::kBilinear, .mipmapping = cgfs::Mipmapping::kOn},
        "textures_mipmapped_bilinear_320x180.ppm");
}

// The checkerboard scene is one scene and two pictures: the texture without its mipmap
// levels and with them.

TEST_CASE("checkerboard scene matches the reference drawing without mipmapping", "[golden]") {
    CheckCheckerboardSceneMatchesGolden({}, "checkerboard_320x180.ppm");
}

TEST_CASE("checkerboard scene matches the reference drawing with mipmapping", "[golden]") {
    CheckCheckerboardSceneMatchesGolden({.mipmapping = cgfs::Mipmapping::kOn},
                                        "checkerboard_mipmapped_320x180.ppm");
}
