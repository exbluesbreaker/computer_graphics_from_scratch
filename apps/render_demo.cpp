// Command-line renderer: draws one of the demo pictures into a canvas and saves it to
// a file. The scene table is the one place that knows the pictures by name; the usage
// text, the default and the dispatch are derived from it, so adding a picture is adding
// a row.

#include <cgfs/camera/viewport.h>
#include <cgfs/demo/rasterizer_scenes.h>
#include <cgfs/demo/raytracer_scenes.h>
#include <cgfs/image/canvas.h>
#include <cgfs/raytracer/raytracer.h>
#include <cgfs/raytracer/scene.h>

#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

// --- The pictures ---------------------------------------------------------------------

/// Ray-traces `scene` into the canvas. The viewport width follows the canvas aspect
/// ratio so the spheres stay round; a larger `distance` narrows the field of view.
void RenderScene(const cgfs::Scene& scene, double distance, cgfs::Canvas& canvas) {
    const cgfs::Viewport viewport = cgfs::Viewport::MatchingCanvasAspect(canvas, 1.0, distance);
    cgfs::RayTracer(scene, viewport).Render(canvas);
}

/// A ray-traced demo scene in the shape the table wants.
template <cgfs::Scene (*Build)()>
void RayTraced(cgfs::Canvas& canvas, double distance) {
    RenderScene(Build(), distance, canvas);
}

/// A rasterized demo picture in the shape the table wants; it has no use for the distance.
template <void (*Draw)(cgfs::Canvas&)>
void Rasterized(cgfs::Canvas& canvas, double /*distance*/) {
    Draw(canvas);
}

/// A rasterized 3D picture in the shape the table wants: it is drawn through the
/// aspect-matched viewport, so `distance` zooms it like the ray-traced scenes.
template <void (*Draw)(cgfs::Canvas&, const cgfs::Viewport&)>
void Projected(cgfs::Canvas& canvas, double distance) {
    Draw(canvas, cgfs::Viewport::MatchingCanvasAspect(canvas, 1.0, distance));
}

/// One picture: its name on the command line, its line in the usage text, how to draw it.
struct DemoScene {
    std::string_view name;
    std::string_view description;
    void (*draw)(cgfs::Canvas& canvas, double distance);
};

/// The first row is the default.
constexpr DemoScene kScenes[] = {
    {"reflective", "shiny spheres on a floor that mirror each other and the floor",
     RayTraced<cgfs::demo::ReflectiveScene>},
    {"specular", "the same spheres and lights without reflections",
     RayTraced<cgfs::demo::SpecularScene>},
    {"diffuse", "the same scene with every surface matte", RayTraced<cgfs::demo::DiffuseScene>},
    {"flat", "the spheres under full ambient light only", RayTraced<cgfs::demo::FlatScene>},
    {"mirrorball", "a chrome ball ringed by colored spheres under a night sky",
     RayTraced<cgfs::demo::MirrorBallScene>},
    {"lines", "colored lines drawn by the rasterizer (no ray tracing)",
     Rasterized<cgfs::demo::DrawLinesScene>},
    {"triangles", "filled and outlined triangles drawn by the rasterizer",
     Rasterized<cgfs::demo::DrawTrianglesScene>},
    {"shaded", "shaded triangles drawn by the rasterizer: an intensity per vertex",
     Rasterized<cgfs::demo::DrawShadedTrianglesScene>},
    {"cube", "the wireframe of a cube in perspective, the rasterizer's first 3D picture",
     Projected<cgfs::demo::DrawCubeScene>},
    {"objects", "objects made of triangles, the book's cube and a pyramid, as wireframes",
     Projected<cgfs::demo::DrawObjectsScene>},
    {"instances", "a wireframe street: two models, a cube and a pyramid, placed sixteen times",
     Projected<cgfs::demo::DrawInstancesScene>},
    {"camera", "two cubes, one scaled and one turned, seen from a camera that moved and turned",
     Projected<cgfs::demo::DrawCameraScene>},
    {"axes", "the coordinate axes and corner dots (no ray tracing)",
     Rasterized<cgfs::demo::DrawAxesScene>},
};

const DemoScene* FindScene(std::string_view name) {
    for (const DemoScene& scene : kScenes) {
        if (scene.name == name) {
            return &scene;
        }
    }
    return nullptr;
}

/// "a, b, c or d": the scene names, for the error message.
std::string SceneNames() {
    std::string names;
    const std::size_t count = std::size(kScenes);
    for (std::size_t i = 0; i < count; ++i) {
        if (i > 0) {
            names += (i + 1 == count) ? " or " : ", ";
        }
        names += kScenes[i].name;
    }
    return names;
}

// --- The command line -----------------------------------------------------------------

struct Options {
    std::string scene = std::string(kScenes[0].name);
    int width = 1920;
    int height = 1080;
    double distance = 1.0;
    std::string output = "render.png";
};

std::string Usage() {
    const Options defaults;
    std::ostringstream out;
    out << "usage: render_demo [--scene ";
    for (std::size_t i = 0; i < std::size(kScenes); ++i) {
        out << (i > 0 ? "|" : "") << kScenes[i].name;
    }
    out << "]\n"
           "                   [--width N] [--height N] [--distance D] [--output PATH]\n"
           "\n";
    std::string_view label = "  --scene     ";
    for (const DemoScene& scene : kScenes) {
        out << label << std::left << std::setw(12) << (std::string(scene.name) + ":")
            << scene.description << "\n";
        label = "              ";
    }
    out << "              (default: " << defaults.scene << ")\n"
        << "  --width     canvas width in pixels (default: " << defaults.width << ")\n"
        << "  --height    canvas height in pixels (default: " << defaults.height << ")\n"
        << "  --distance  viewport distance; larger zooms in (default: " << defaults.distance
        << ")\n"
        << "  --output    output file, .png or .ppm (default: " << defaults.output << ")\n";
    return out.str();
}

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

}  // namespace

int main(int argc, char** argv) {
    try {
        const std::optional<Options> options = ParseArgs(argc, argv);
        if (!options) {
            std::cout << Usage();
            return EXIT_SUCCESS;
        }
        const DemoScene* scene = FindScene(options->scene);
        if (scene == nullptr) {
            throw std::runtime_error("unknown scene '" + options->scene + "' (expected " +
                                     SceneNames() + ")");
        }

        cgfs::Canvas canvas(options->width, options->height);
        const auto start = std::chrono::steady_clock::now();
        scene->draw(canvas, options->distance);
        const auto elapsed = std::chrono::steady_clock::now() - start;
        canvas.Save(options->output);

        std::cout << "rendered '" << scene->name << "' at " << canvas.width() << "x"
                  << canvas.height() << " in "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()
                  << " ms -> " << options->output << "\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "render_demo: " << error.what() << "\n" << Usage();
        return EXIT_FAILURE;
    }
}
