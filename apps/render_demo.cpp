// Command-line renderer: draws one of the demo pictures into a canvas and saves it to
// a file.

#include <cgfs/canvas.h>
#include <cgfs/demo_scenes.h>
#include <cgfs/raytracer.h>
#include <cgfs/viewport.h>

#include <chrono>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

namespace {

struct Options {
    std::string scene = "reflective";
    int width = 1920;
    int height = 1080;
    double distance = 1.0;
    std::string output = "render.png";
};

constexpr std::string_view kUsage =
    "usage: render_demo [--scene reflective|specular|diffuse|flat|mirrorball|lines|"
    "triangles|axes]\n"
    "                   [--width N] [--height N] [--distance D] [--output PATH]\n"
    "\n"
    "  --scene     reflective: shiny spheres on a floor that mirror each other and the floor\n"
    "              specular:   the same spheres and lights without reflections\n"
    "              diffuse:    the same scene with every surface matte\n"
    "              flat:       the spheres under full ambient light only\n"
    "              mirrorball: a chrome ball ringed by colored spheres under a night sky\n"
    "              lines:      colored lines drawn by the rasterizer (no ray tracing)\n"
    "              triangles:  filled and outlined triangles drawn by the rasterizer\n"
    "              axes:       the coordinate axes and corner dots (no ray tracing)\n"
    "              (default: reflective)\n"
    "  --width     canvas width in pixels (default: 1920)\n"
    "  --height    canvas height in pixels (default: 1080)\n"
    "  --distance  viewport distance; larger zooms in (default: 1)\n"
    "  --output    output file, .png or .ppm (default: render.png)\n";

double ParsePositiveDouble(std::string_view name, const std::string& text) {
    std::size_t consumed = 0;
    double value = 0.0;
    try {
        value = std::stod(text, &consumed);
    } catch (const std::exception&) {
        consumed = 0;
    }
    if (consumed != text.size() || value <= 0.0) {
        throw std::runtime_error(std::string(name) + " expects a positive number, got '" +
                                 text + "'");
    }
    return value;
}

int ParsePositiveInt(std::string_view name, const std::string& text) {
    std::size_t consumed = 0;
    int value = 0;
    try {
        value = std::stoi(text, &consumed);
    } catch (const std::exception&) {
        consumed = 0;
    }
    if (consumed != text.size() || value <= 0) {
        throw std::runtime_error(std::string(name) + " expects a positive integer, got '" +
                                 text + "'");
    }
    return value;
}

// Returns nullopt when --help was requested. Throws std::runtime_error on bad input.
std::optional<Options> ParseArgs(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            return std::nullopt;
        }
        if (i + 1 >= argc) {
            throw std::runtime_error("missing value for " + std::string(arg));
        }
        const std::string value = argv[++i];
        if (arg == "--scene") {
            options.scene = value;
        } else if (arg == "--width") {
            options.width = ParsePositiveInt(arg, value);
        } else if (arg == "--height") {
            options.height = ParsePositiveInt(arg, value);
        } else if (arg == "--distance") {
            options.distance = ParsePositiveDouble(arg, value);
        } else if (arg == "--output") {
            options.output = value;
        } else {
            throw std::runtime_error("unknown option " + std::string(arg));
        }
    }
    return options;
}

void DrawDot(cgfs::Canvas& canvas, int cx, int cy, cgfs::Color color, int radius = 6) {
    for (int dx = -radius; dx <= radius; ++dx) {
        for (int dy = -radius; dy <= radius; ++dy) {
            canvas.PutPixel(cx + dx, cy + dy, color);
        }
    }
}

// The axes through the origin, a dot at the origin and a colored dot in each corner:
// a quick visual check of the coordinate system.
void DrawAxes(cgfs::Canvas& canvas) {
    using namespace cgfs::colors;
    constexpr cgfs::Color kGray{160.0, 160.0, 160.0};
    constexpr cgfs::Color kDarkGreen{0.0, 160.0, 0.0};

    const int half_w = canvas.width() / 2;
    const int half_h = canvas.height() / 2;
    for (int x = -half_w; x < half_w; ++x) {
        canvas.PutPixel(x, 0, kGray);
    }
    for (int y = -half_h; y < half_h; ++y) {
        canvas.PutPixel(0, y, kGray);
    }
    DrawDot(canvas, 0, 0, kBlack);
    DrawDot(canvas, -half_w + 8, half_h - 9, kRed);        // top-left
    DrawDot(canvas, half_w - 9, half_h - 9, kDarkGreen);   // top-right
    DrawDot(canvas, -half_w + 8, -half_h + 8, kBlue);      // bottom-left
    DrawDot(canvas, half_w - 9, -half_h + 8, kBlack);      // bottom-right
}

void RenderScene(const cgfs::Scene& scene, double distance, cgfs::Canvas& canvas) {
    // The viewport width follows the canvas aspect ratio so the spheres stay round.
    const cgfs::Viewport viewport = cgfs::Viewport::MatchingCanvasAspect(canvas, 1.0, distance);
    cgfs::RayTracer(scene, viewport).Render(canvas);
}

void Draw(const Options& options, cgfs::Canvas& canvas) {
    if (options.scene == "reflective") {
        RenderScene(cgfs::demo::ReflectiveScene(), options.distance, canvas);
    } else if (options.scene == "specular") {
        RenderScene(cgfs::demo::SpecularScene(), options.distance, canvas);
    } else if (options.scene == "diffuse") {
        RenderScene(cgfs::demo::DiffuseScene(), options.distance, canvas);
    } else if (options.scene == "flat") {
        RenderScene(cgfs::demo::FlatScene(), options.distance, canvas);
    } else if (options.scene == "mirrorball") {
        RenderScene(cgfs::demo::MirrorBallScene(), options.distance, canvas);
    } else if (options.scene == "lines") {
        cgfs::demo::DrawLinesScene(canvas);
    } else if (options.scene == "triangles") {
        cgfs::demo::DrawTrianglesScene(canvas);
    } else if (options.scene == "axes") {
        DrawAxes(canvas);
    } else {
        throw std::runtime_error("unknown scene '" + options.scene +
                                 "' (expected reflective, specular, diffuse, flat, mirrorball, "
                                 "lines, triangles or axes)");
    }
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const std::optional<Options> options = ParseArgs(argc, argv);
        if (!options) {
            std::cout << kUsage;
            return EXIT_SUCCESS;
        }

        cgfs::Canvas canvas(options->width, options->height);
        const auto start = std::chrono::steady_clock::now();
        Draw(*options, canvas);
        const auto elapsed = std::chrono::steady_clock::now() - start;
        canvas.Save(options->output);

        std::cout << "rendered '" << options->scene << "' at " << canvas.width() << "x"
                  << canvas.height() << " in "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()
                  << " ms -> " << options->output << "\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "render_demo: " << error.what() << "\n" << kUsage;
        return EXIT_FAILURE;
    }
}
