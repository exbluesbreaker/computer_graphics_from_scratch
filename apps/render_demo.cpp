// Command-line renderer: draws one of the demo pictures into a canvas, saves it to a
// file and reports how long the drawing took. The scene table is the one place that
// knows the pictures by name; the usage text, the default and the dispatch are derived
// from it, so adding a picture is adding a row.

#include <cgfs/camera/viewport.h>
#include <cgfs/demo/rasterizer_scenes.h>
#include <cgfs/demo/raytracer_scenes.h>
#include <cgfs/demo/textures.h>
#include <cgfs/image/canvas.h>
#include <cgfs/image/texture.h>
#include <cgfs/rasterizer/shading.h>
#include <cgfs/rasterizer/triangle.h>
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

/// What a picture is drawn with besides the canvas: what the command line says about how
/// it is to be drawn, and the texture read for it. A picture reads what applies to it:
/// the 2D pictures nothing, only the lit ones the shading model, and only the textured
/// ones the texture mapping, the texture filter and the texture.
struct Settings {
    double distance = 1.0;
    cgfs::ShadingModel shading_model = cgfs::ShadingModel::kPhong;
    cgfs::TextureMapping texture_mapping = cgfs::TextureMapping::kPerspectiveCorrect;
    cgfs::TextureFilter texture_filter = cgfs::TextureFilter::kNearestNeighbor;
    /// The texture of a textured picture, which main reads before it starts the clock;
    /// null for every other picture.
    const cgfs::Texture* texture = nullptr;
};

/// A ray-traced demo scene in the shape the table wants.
template <cgfs::Scene (*Build)()>
void RayTraced(cgfs::Canvas& canvas, const Settings& settings) {
    RenderScene(Build(), settings.distance, canvas);
}

/// A rasterized demo picture in the shape the table wants; it has no use for the settings.
template <void (*Draw)(cgfs::Canvas&)>
void Rasterized(cgfs::Canvas& canvas, const Settings& /*settings*/) {
    Draw(canvas);
}

/// A rasterized 3D picture in the shape the table wants: it is drawn through the
/// aspect-matched viewport, so the distance zooms it like the ray-traced scenes.
template <void (*Draw)(cgfs::Canvas&, const cgfs::Viewport&)>
void Projected(cgfs::Canvas& canvas, const Settings& settings) {
    Draw(canvas, cgfs::Viewport::MatchingCanvasAspect(canvas, 1.0, settings.distance));
}

/// A rasterized 3D picture under lights in the shape the table wants: projected like the
/// others and shaded with the model the command line chose.
template <void (*Draw)(cgfs::Canvas&, const cgfs::Viewport&, cgfs::ShadingModel)>
void Lit(cgfs::Canvas& canvas, const Settings& settings) {
    Draw(canvas, cgfs::Viewport::MatchingCanvasAspect(canvas, 1.0, settings.distance),
         settings.shading_model);
}

/// A rasterized 3D picture painted with a texture in the shape the table wants: projected
/// like the others, with the texture main read for it, mapped and filtered as the command
/// line chose.
template <void (*Draw)(cgfs::Canvas&, const cgfs::Viewport&, const cgfs::Texture&,
                       cgfs::TextureMapping, cgfs::TextureFilter)>
void Textured(cgfs::Canvas& canvas, const Settings& settings) {
    Draw(canvas, cgfs::Viewport::MatchingCanvasAspect(canvas, 1.0, settings.distance),
         *settings.texture, settings.texture_mapping, settings.texture_filter);
}

/// One picture: its name on the command line, its line in the usage text, how to draw
/// it, whether the shading model changes it, and whether it is painted with the crate
/// texture, which then has to be read first and whose mapping and filter change it.
struct DemoScene {
    std::string_view name;
    std::string_view description;
    void (*draw)(cgfs::Canvas& canvas, const Settings& settings);
    bool lit = false;
    bool textured = false;
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
    {"clipping", "a shaded city seen from inside it, clipped to what the camera can see",
     Projected<cgfs::demo::DrawClippingScene>},
    {"filled", "filled cubes listed in the wrong order and set right by a depth buffer",
     Projected<cgfs::demo::DrawFilledCubesScene>},
    {"shading", "solids and three lamps over three tiles, shaded as --shading says",
     Lit<cgfs::demo::DrawShadingScene>, true},
    {"textures", "a wooden crate on a floor paved with the same texture, out to the horizon",
     Textured<cgfs::demo::DrawTexturesScene>, false, true},
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

/// One shading model: its name on the command line and its line in the usage text.
struct ShadingChoice {
    std::string_view name;
    cgfs::ShadingModel shading_model;
    std::string_view description;
};

/// In the order of the book's chapter, each right where the one before it is not.
constexpr ShadingChoice kShadingChoices[] = {
    {"flat", cgfs::ShadingModel::kFlat, "one light value per triangle"},
    {"gouraud-faceted", cgfs::ShadingModel::kGouraudFaceted,
     "the light at the corners, interpolated; every corner has its triangle's normal"},
    {"gouraud", cgfs::ShadingModel::kGouraud,
     "the same with the model's normals at the corners: curved surfaces look curved"},
    {"phong", cgfs::ShadingModel::kPhong, "the normals interpolated, the light per pixel"},
};

const ShadingChoice& FindShadingChoice(cgfs::ShadingModel shading_model) {
    for (const ShadingChoice& choice : kShadingChoices) {
        if (choice.shading_model == shading_model) {
            return choice;
        }
    }
    return kShadingChoices[0];  // not reached: the table has every model
}

/// One way to carry a texture across a triangle: its name on the command line and its
/// line in the usage text.
struct TextureMappingChoice {
    std::string_view name;
    cgfs::TextureMapping texture_mapping;
    std::string_view description;
};

/// In the order of the book's chapter: what is tried first, and what is right.
constexpr TextureMappingChoice kTextureMappingChoices[] = {
    {"linear", cgfs::TextureMapping::kLinear,
     "u and v interpolated over the picture: planks bend on a face that leans away"},
    {"perspective-correct", cgfs::TextureMapping::kPerspectiveCorrect,
     "u/z and v/z interpolated and divided by 1/z at every pixel"},
};

const TextureMappingChoice& FindTextureMappingChoice(cgfs::TextureMapping texture_mapping) {
    for (const TextureMappingChoice& choice : kTextureMappingChoices) {
        if (choice.texture_mapping == texture_mapping) {
            return choice;
        }
    }
    return kTextureMappingChoices[0];  // not reached: the table has every mapping
}

/// One way to make the color of a point of a texture from its texels: its name on the
/// command line and its line in the usage text.
struct TextureFilterChoice {
    std::string_view name;
    cgfs::TextureFilter texture_filter;
    std::string_view description;
};

/// In the order of the book's chapter: the simplest, and what mends its blocks.
constexpr TextureFilterChoice kTextureFilterChoices[] = {
    {"nearest-neighbor", cgfs::TextureFilter::kNearestNeighbor,
     "the one texel a point falls in: blocks where a texture is drawn large"},
    {"bilinear", cgfs::TextureFilter::kBilinear,
     "the four texels around a point, blended by how near each is"},
};

const TextureFilterChoice& FindTextureFilterChoice(cgfs::TextureFilter texture_filter) {
    for (const TextureFilterChoice& choice : kTextureFilterChoices) {
        if (choice.texture_filter == texture_filter) {
            return choice;
        }
    }
    return kTextureFilterChoices[0];  // not reached: the table has every filter
}

// --- The command line -----------------------------------------------------------------

struct Options {
    std::string scene = std::string(kScenes[0].name);
    int width = 1920;
    int height = 1080;
    Settings settings;
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
           "                   [--width N] [--height N] [--distance D] [--shading ";
    for (std::size_t i = 0; i < std::size(kShadingChoices); ++i) {
        out << (i > 0 ? "|" : "") << kShadingChoices[i].name;
    }
    out << "]\n"
           "                   [--texture-mapping ";
    for (std::size_t i = 0; i < std::size(kTextureMappingChoices); ++i) {
        out << (i > 0 ? "|" : "") << kTextureMappingChoices[i].name;
    }
    out << "]\n"
           "                   [--texture-filter ";
    for (std::size_t i = 0; i < std::size(kTextureFilterChoices); ++i) {
        out << (i > 0 ? "|" : "") << kTextureFilterChoices[i].name;
    }
    out << "] [--output PATH]\n"
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
        << "  --distance  viewport distance; larger zooms in (default: "
        << defaults.settings.distance << ")\n";
    label = "  --shading   ";
    for (const ShadingChoice& choice : kShadingChoices) {
        out << label << std::left << std::setw(17) << (std::string(choice.name) + ":")
            << choice.description << "\n";
        label = "              ";
    }
    out << "              how the pictures under lights are shaded (default: "
        << FindShadingChoice(defaults.settings.shading_model).name << ")\n";
    label = "  --texture-mapping\n              ";
    for (const TextureMappingChoice& choice : kTextureMappingChoices) {
        out << label << std::left << std::setw(21) << (std::string(choice.name) + ":")
            << choice.description << "\n";
        label = "              ";
    }
    out << "              how a texture is carried across a triangle (default: "
        << FindTextureMappingChoice(defaults.settings.texture_mapping).name << ")\n";
    label = "  --texture-filter\n              ";
    for (const TextureFilterChoice& choice : kTextureFilterChoices) {
        out << label << std::left << std::setw(21) << (std::string(choice.name) + ":")
            << choice.description << "\n";
        label = "              ";
    }
    out << "              how the color of a point of a texture is made from its texels (default: "
        << FindTextureFilterChoice(defaults.settings.texture_filter).name << ")\n"
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

cgfs::ShadingModel ParseShadingModel(std::string_view name, const std::string& text) {
    std::string names;
    for (const ShadingChoice& choice : kShadingChoices) {
        if (choice.name == text) {
            return choice.shading_model;
        }
        names += (names.empty() ? "" : ", ") + std::string(choice.name);
    }
    throw std::runtime_error(std::string(name) + " expects one of " + names + ", got '" + text +
                             "'");
}

cgfs::TextureMapping ParseTextureMapping(std::string_view name, const std::string& text) {
    std::string names;
    for (const TextureMappingChoice& choice : kTextureMappingChoices) {
        if (choice.name == text) {
            return choice.texture_mapping;
        }
        names += (names.empty() ? "" : ", ") + std::string(choice.name);
    }
    throw std::runtime_error(std::string(name) + " expects one of " + names + ", got '" + text +
                             "'");
}

cgfs::TextureFilter ParseTextureFilter(std::string_view name, const std::string& text) {
    std::string names;
    for (const TextureFilterChoice& choice : kTextureFilterChoices) {
        if (choice.name == text) {
            return choice.texture_filter;
        }
        names += (names.empty() ? "" : ", ") + std::string(choice.name);
    }
    throw std::runtime_error(std::string(name) + " expects one of " + names + ", got '" + text +
                             "'");
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
            options.settings.distance = ParsePositiveDouble(arg, value);
        } else if (arg == "--shading") {
            options.settings.shading_model = ParseShadingModel(arg, value);
        } else if (arg == "--texture-mapping") {
            options.settings.texture_mapping = ParseTextureMapping(arg, value);
        } else if (arg == "--texture-filter") {
            options.settings.texture_filter = ParseTextureFilter(arg, value);
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

        // A textured picture has its texture read here, before the clock starts: reading
        // a file is not drawing.
        Settings settings = options->settings;
        std::optional<cgfs::Texture> texture;
        if (scene->textured) {
            texture.emplace(cgfs::demo::CrateTexture());
            settings.texture = &*texture;
        }

        // The render time is that of the drawing alone: the canvas exists before the
        // clock starts and the file is written after it stops. In hundredths of a
        // millisecond, since most of the rasterized pictures take only a few, and on a
        // line of its own, where it is easy to find and to compare between runs.
        cgfs::Canvas canvas(options->width, options->height);
        const auto start = std::chrono::steady_clock::now();
        scene->draw(canvas, settings);
        const std::chrono::duration<double, std::milli> elapsed =
            std::chrono::steady_clock::now() - start;
        canvas.Save(options->output);

        std::cout << "rendered '" << scene->name << "'";
        if (scene->lit) {
            std::cout << " (" << FindShadingChoice(settings.shading_model).name << " shading)";
        }
        if (scene->textured) {
            std::cout << " (" << FindTextureMappingChoice(settings.texture_mapping).name
                      << " texture mapping, "
                      << FindTextureFilterChoice(settings.texture_filter).name << " filtering)";
        }
        std::cout << " at " << canvas.width() << "x" << canvas.height() << " -> " << options->output
                  << "\n"
                  << "render time: " << std::fixed << std::setprecision(2) << elapsed.count()
                  << " ms\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "render_demo: " << error.what() << "\n" << Usage();
        return EXIT_FAILURE;
    }
}
