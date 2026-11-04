#include <cgfs/demo_scenes.h>

#include <cgfs/rasterizer.h>

#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

// These scenes are shared by render_demo and the golden-image tests. Changing them
// changes the reference images in tests/golden/.

namespace cgfs::demo {

namespace {

std::vector<Sphere> ThreeSpheres() {
    return {
        {.center = {0.0, -1.0, 3.0}, .radius = 1.0, .color = colors::kRed},
        {.center = {2.0, 0.0, 4.0}, .radius = 1.0, .color = colors::kBlue},
        {.center = {-2.0, 0.0, 4.0}, .radius = 1.0, .color = colors::kGreen},
    };
}

/// A fully saturated color `turn` of the way around the color wheel: red at 0, yellow at
/// 1/6, green at 1/3, cyan at 1/2, blue at 2/3, magenta at 5/6 and back to red at 1.
/// Precondition: 0 <= turn < 1. Plain arithmetic, no trigonometry.
Color Wheel(double turn) {
    const double position = 6.0 * turn;  // which of the six sectors, and how far into it
    const int sector = static_cast<int>(position);
    const double rising = 255.0 * (position - sector);
    const double falling = 255.0 - rising;
    switch (sector) {
        case 0:
            return {255.0, rising, 0.0};
        case 1:
            return {falling, 255.0, 0.0};
        case 2:
            return {0.0, 255.0, rising};
        case 3:
            return {0.0, falling, 255.0};
        case 4:
            return {rising, 0.0, 255.0};
        default:
            return {255.0, 0.0, falling};
    }
}

/// The color `t` of the way from `from` to `to`, for t in [0, 1].
Color Mix(Color from, Color to, double t) { return from * (1.0 - t) + to * t; }

/// `per_side` points along each edge of the rectangle |x| <= half_x, |y| <= half_y,
/// clockwise from the top-left corner: 4 * per_side points, each corner included once.
std::vector<Point> AroundRectangle(int half_x, int half_y, int per_side) {
    std::vector<Point> points;
    points.reserve(static_cast<std::size_t>(4 * per_side));
    for (int k = 0; k < per_side; ++k) {
        points.push_back({-half_x + 2 * half_x * k / per_side, half_y});  // top, left to right
    }
    for (int k = 0; k < per_side; ++k) {
        points.push_back({half_x, half_y - 2 * half_y * k / per_side});  // right, top to bottom
    }
    for (int k = 0; k < per_side; ++k) {
        points.push_back({half_x - 2 * half_x * k / per_side, -half_y});  // bottom, right to left
    }
    for (int k = 0; k < per_side; ++k) {
        points.push_back({-half_x, -half_y + 2 * half_y * k / per_side});  // left, bottom to top
    }
    return points;
}

}  // namespace

Scene FlatScene() {
    return {
        .spheres = ThreeSpheres(),
        .lights = {AmbientLight{.intensity = 1.0}},
    };
}

Scene DiffuseScene() {
    Scene scene{
        .spheres = ThreeSpheres(),
        .lights =
            {
                AmbientLight{.intensity = 0.2},
                PointLight{.intensity = 0.6, .position = {2.0, 1.0, 0.0}},
                DirectionalLight{.intensity = 0.2, .direction = {1.0, 4.0, 4.0}},
            },
    };
    // At our scale the surface of this sphere is practically flat: a floor.
    scene.spheres.push_back(
        {.center = {0.0, -5001.0, 0.0}, .radius = 5000.0, .color = colors::kYellow});
    return scene;
}

Scene SpecularScene() {
    Scene scene = DiffuseScene();
    // The spheres in the order DiffuseScene builds them.
    scene.spheres[0].specular = 500.0;   // red: shiny
    scene.spheres[1].specular = 500.0;   // blue: shiny
    scene.spheres[2].specular = 10.0;    // green: somewhat shiny
    scene.spheres[3].specular = 1000.0;  // yellow floor: very shiny
    return scene;
}

Scene ReflectiveScene() {
    Scene scene = SpecularScene();
    scene.spheres[0].reflective = 0.2;  // red: a bit reflective
    scene.spheres[1].reflective = 0.3;  // blue: a bit more reflective
    scene.spheres[2].reflective = 0.4;  // green: even more reflective
    scene.spheres[3].reflective = 0.5;  // yellow floor: half reflective
    return scene;
}

Scene MirrorBallScene() {
    Scene scene{
        .lights =
            {
                AmbientLight{.intensity = 0.2},
                PointLight{.intensity = 0.7, .position = {-4.0, 5.0, 2.0}},   // key, front left
                PointLight{.intensity = 0.5, .position = {5.0, 4.0, 11.0}},   // rim, back right
                DirectionalLight{.intensity = 0.15, .direction = {0.0, 1.0, 0.0}},  // top fill
            },
        .background_color = {50.0, 55.0, 90.0},  // dusk sky
    };

    // A dark polished floor, the same huge sphere as in the book's scene.
    scene.spheres.push_back({.center = {0.0, -5001.0, 0.0},
                             .radius = 5000.0,
                             .color = {90.0, 90.0, 100.0},
                             .specular = 1000.0,
                             .reflective = 0.5});

    // The chrome ball rests on the floor in the middle of the picture.
    const Vec3 ball_center{0.0, 0.5, 7.0};
    scene.spheres.push_back({.center = ball_center,
                             .radius = 1.5,
                             .color = {230.0, 230.0, 240.0},
                             .specular = 1000.0,
                             .reflective = 0.6});

    // Six spheres evenly spaced around the ball, each with its own finish: matte-ish,
    // glossy or mirror-like.
    struct Finish {
        Color color;
        double specular;
        double reflective;
    };
    const Finish ring[] = {
        {{220.0, 30.0, 60.0}, 500.0, 0.3},    // ruby
        {{250.0, 160.0, 30.0}, 100.0, 0.1},   // amber
        {{120.0, 220.0, 60.0}, 1000.0, 0.5},  // lime
        {{30.0, 190.0, 200.0}, 500.0, 0.3},   // teal
        {{40.0, 80.0, 230.0}, 100.0, 0.1},    // sapphire
        {{220.0, 60.0, 200.0}, 1000.0, 0.5},  // magenta
    };
    constexpr double kRingRadius = 3.0;
    constexpr double kSmallRadius = 0.6;
    for (int i = 0; i < 6; ++i) {
        const double angle = i * std::numbers::pi / 3;  // 0, 60, 120, ... degrees
        scene.spheres.push_back({.center = {ball_center.x + kRingRadius * std::cos(angle),
                                            -1.0 + kSmallRadius,  // resting on the floor
                                            ball_center.z + kRingRadius * std::sin(angle)},
                                 .radius = kSmallRadius,
                                 .color = ring[i].color,
                                 .specular = ring[i].specular,
                                 .reflective = ring[i].reflective});
    }

    // Three marbles in the foreground: pearl, gold and black glass.
    constexpr double kMarbleRadius = 0.25;
    constexpr double kMarbleY = -1.0 + kMarbleRadius;
    scene.spheres.push_back({.center = {-1.4, kMarbleY, 3.4},
                             .radius = kMarbleRadius,
                             .color = {240.0, 235.0, 225.0},
                             .specular = 1000.0,
                             .reflective = 0.4});
    scene.spheres.push_back({.center = {0.4, kMarbleY, 3.0},
                             .radius = kMarbleRadius,
                             .color = {250.0, 200.0, 80.0},
                             .specular = 500.0,
                             .reflective = 0.3});
    scene.spheres.push_back({.center = {1.8, kMarbleY, 3.8},
                             .radius = kMarbleRadius,
                             .color = {20.0, 20.0, 20.0},
                             .specular = 1000.0,
                             .reflective = 0.7});

    // Three spheres floating above the scene: they fill the sky, show up in the chrome
    // ball and drop shadows onto the floor.
    scene.spheres.push_back({.center = {-2.5, 2.5, 8.0},
                             .radius = 0.5,
                             .color = {240.0, 235.0, 225.0},
                             .specular = 500.0,
                             .reflective = 0.2});
    scene.spheres.push_back({.center = {2.5, 3.0, 9.0},
                             .radius = 0.7,
                             .color = {250.0, 120.0, 60.0},
                             .specular = 100.0,
                             .reflective = 0.1});
    scene.spheres.push_back({.center = {0.5, 3.8, 12.0},
                             .radius = 0.9,
                             .color = {160.0, 200.0, 240.0},
                             .specular = 1000.0,
                             .reflective = 0.5});
    return scene;
}

void DrawLinesScene(Canvas& canvas) {
    constexpr Color kBackground{18.0, 20.0, 28.0};
    constexpr Color kAxis{58.0, 62.0, 78.0};
    constexpr Color kEnvelopeStart{40.0, 90.0, 150.0};
    constexpr Color kEnvelopeEnd{150.0, 215.0, 255.0};

    canvas.Clear(kBackground);

    // The outermost pixels of the canvas (see Canvas for the coordinate range).
    const int left = -canvas.width() / 2;
    const int right = canvas.width() / 2 - 1;
    const int bottom = -canvas.height() / 2;
    const int top = canvas.height() / 2 - 1;

    // Axes through the origin: a horizontal and a vertical line, the two cases in which
    // Interpolate returns a constant.
    DrawLine(canvas, {left, 0}, {right, 0}, kAxis);
    DrawLine(canvas, {0, bottom}, {0, top}, kAxis);

    // String art in every corner. Line k joins the point k steps along the horizontal
    // edge to the point n - k steps along the vertical edge; none of the lines is curved,
    // yet together they outline a parabola. The lines reach 2/5 of the way along each
    // edge, so the envelopes of neighbouring corners do not meet.
    constexpr int kEnvelopeLines = 16;
    const int reach_x = canvas.width() * 2 / 5;
    const int reach_y = canvas.height() * 2 / 5;
    const Point corners[] = {{left, top}, {right, top}, {right, bottom}, {left, bottom}};
    for (const Point corner : corners) {
        const int toward_middle_x = corner.x < 0 ? 1 : -1;
        const int toward_middle_y = corner.y < 0 ? 1 : -1;
        for (int k = 1; k < kEnvelopeLines; ++k) {
            const Point on_horizontal_edge{
                corner.x + toward_middle_x * (reach_x * k / kEnvelopeLines), corner.y};
            const Point on_vertical_edge{
                corner.x,
                corner.y + toward_middle_y * (reach_y * (kEnvelopeLines - k) / kEnvelopeLines)};
            const double t = static_cast<double>(k) / kEnvelopeLines;
            DrawLine(canvas, on_horizontal_edge, on_vertical_edge,
                     Mix(kEnvelopeStart, kEnvelopeEnd, t));
        }
    }

    // A sunburst: lines from the origin to points evenly spaced around a rectangle, going
    // clockwise from the top-left corner and once around the color wheel. Each quadrant
    // of the rectangle produces a different mix of the horizontal-ish and vertical-ish
    // branches of DrawLine, with and without swapped endpoints.
    constexpr int kBurstPerSide = 12;
    constexpr int kBurstLines = 4 * kBurstPerSide;
    const int half_x = canvas.width() * 3 / 10;
    const int half_y = canvas.height() * 3 / 10;
    for (int k = 0; k < kBurstPerSide; ++k) {
        const int along_x = 2 * half_x * k / kBurstPerSide;  // 0 .. just short of 2 half_x
        const int along_y = 2 * half_y * k / kBurstPerSide;
        const Point around[] = {
            {-half_x + along_x, half_y},   // top edge, left to right
            {half_x, half_y - along_y},    // right edge, top to bottom
            {half_x - along_x, -half_y},   // bottom edge, right to left
            {-half_x, -half_y + along_y},  // left edge, bottom to top
        };
        for (int side = 0; side < 4; ++side) {
            const int index = side * kBurstPerSide + k;
            DrawLine(canvas, {0, 0}, around[side],
                     Wheel(static_cast<double>(index) / kBurstLines));
        }
    }
}

void DrawTrianglesScene(Canvas& canvas) {
    constexpr Color kBackground{18.0, 20.0, 28.0};

    canvas.Clear(kBackground);

    const int left = -canvas.width() / 2;
    const int right = canvas.width() / 2 - 1;
    const int bottom = -canvas.height() / 2;
    const int top = canvas.height() / 2 - 1;
    const int reach_x = canvas.width() * 2 / 5;
    const int reach_y = canvas.height() * 2 / 5;

    // A big triangle in every corner, drawn the way the book's demo draws its one
    // triangle: filled, then outlined in a brighter shade of the same color. Between
    // them they have a flat top, a vertical edge, a flat bottom and a vertex beyond the
    // canvas edge, which the canvas clips.
    struct OutlinedTriangle {
        Point a;
        Point b;
        Point c;
        Color fill;
        Color outline;
    };
    const OutlinedTriangle corners[] = {
        {{left, top},
         {left + reach_x, top},
         {left + reach_x / 3, top - reach_y},
         {70.0, 60.0, 110.0},
         {170.0, 150.0, 230.0}},  // top-left: flat top
        {{right, top},
         {right, top - reach_y},
         {right - reach_x, top - reach_y / 4},
         {50.0, 90.0, 90.0},
         {120.0, 210.0, 200.0}},  // top-right: vertical edge
        {{right, bottom},
         {right - reach_x, bottom},
         {right - reach_x / 4, bottom + reach_y},
         {90.0, 90.0, 50.0},
         {220.0, 220.0, 130.0}},  // bottom-right: flat bottom
        {{left - reach_x / 5, bottom + reach_y},
         {left + reach_x, bottom + reach_y / 3},
         {left, bottom},
         {110.0, 70.0, 60.0},
         {240.0, 160.0, 120.0}},  // bottom-left: one vertex off the canvas
    };
    for (const OutlinedTriangle& t : corners) {
        DrawFilledTriangle(canvas, t.a, t.b, t.c, t.fill);
        DrawWireframeTriangle(canvas, t.a, t.b, t.c, t.outline);
    }

    // A color wheel: wedges from the origin to consecutive points around a rectangle,
    // once around the hue wheel. Neighbouring wedges share an edge, which both of them
    // interpolate from the same two endpoints, so they tile without gaps; the wedge drawn
    // later owns the shared pixels.
    constexpr int kWedgesPerSide = 6;
    const std::vector<Point> rim =
        AroundRectangle(canvas.width() * 3 / 10, canvas.height() * 3 / 10, kWedgesPerSide);
    const int wedges = static_cast<int>(rim.size());
    for (int k = 0; k < wedges; ++k) {
        const Point a = rim[static_cast<std::size_t>(k)];
        const Point b = rim[static_cast<std::size_t>((k + 1) % wedges)];
        DrawFilledTriangle(canvas, {0, 0}, a, b, Wheel(static_cast<double>(k) / wedges));
    }
}

}  // namespace cgfs::demo
