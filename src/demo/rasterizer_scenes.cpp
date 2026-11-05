#include <cgfs/demo/rasterizer_scenes.h>

#include <cgfs/image/color.h>
#include <cgfs/math/vec3.h>
#include <cgfs/rasterizer/line.h>
#include <cgfs/rasterizer/object.h>
#include <cgfs/rasterizer/point.h>
#include <cgfs/rasterizer/projection.h>
#include <cgfs/rasterizer/triangle.h>

#include <cstddef>
#include <vector>

// These pictures are shared by render_demo and the golden-image tests. Changing them
// changes the reference images in tests/demo/golden/.

namespace cgfs::demo {

namespace {

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

/// A filled square of side 2 * radius + 1 around (cx, cy).
void DrawDot(Canvas& canvas, int cx, int cy, Color color, int radius = 6) {
    for (int dx = -radius; dx <= radius; ++dx) {
        for (int dy = -radius; dy <= radius; ++dy) {
            canvas.PutPixel(cx + dx, cy + dy, color);
        }
    }
}

}  // namespace

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

void DrawShadedTrianglesScene(Canvas& canvas) {
    constexpr Color kBackground{18.0, 20.0, 28.0};

    canvas.Clear(kBackground);

    // The book's shaded triangle: its demos draw on a 600 x 600 canvas a green triangle
    // with the intensities 0.3, 0.1 and 1.0 at its vertices. The book's canvas height
    // becomes this canvas's height, and the triangle moves to the left part.
    constexpr int kBookCanvas = 600;
    const int unit = canvas.height();
    const int triangle_center_x = -canvas.width() * 9 / 32;
    const auto from_book = [&](int x, int y, double h) {
        return Point{triangle_center_x + x * unit / kBookCanvas, y * unit / kBookCanvas, h};
    };
    DrawShadedTriangle(canvas, from_book(-200, -250, 0.3), from_book(200, 50, 0.1),
                       from_book(20, 250, 1.0), colors::kGreen);

    // A glowing color wheel: wedges from a bright center to the dim rim of a rectangle,
    // once around the hue wheel. Neighbouring wedges share an edge and interpolate its x
    // and h from the same two vertices, so the shading is continuous across it; only the
    // hue changes from wedge to wedge.
    constexpr double kRimIntensity = 0.2;
    constexpr int kWedgesPerSide = 6;
    const Point center{canvas.width() * 3 / 16, 0, 1.0};
    const std::vector<Point> around =
        AroundRectangle(canvas.width() / 4, canvas.height() * 2 / 5, kWedgesPerSide);
    const auto rim = [&](int k) {
        const Point p = around[static_cast<std::size_t>(k)];
        return Point{center.x + p.x, center.y + p.y, kRimIntensity};
    };
    const int wedges = static_cast<int>(around.size());
    for (int k = 0; k < wedges; ++k) {
        DrawShadedTriangle(canvas, center, rim(k), rim((k + 1) % wedges),
                           Wheel(static_cast<double>(k) / wedges));
    }
}

void DrawCubeScene(Canvas& canvas, const Viewport& viewport) {
    constexpr Color kBackground{18.0, 20.0, 28.0};

    canvas.Clear(kBackground);

    // The four "front" vertices and the four "back" vertices of the book's cube.
    const Vec3 front_a{-2.0, -0.5, 5.0};
    const Vec3 front_b{-2.0, 0.5, 5.0};
    const Vec3 front_c{-1.0, 0.5, 5.0};
    const Vec3 front_d{-1.0, -0.5, 5.0};
    const Vec3 back_a{-2.0, -0.5, 6.0};
    const Vec3 back_b{-2.0, 0.5, 6.0};
    const Vec3 back_c{-1.0, 0.5, 6.0};
    const Vec3 back_d{-1.0, -0.5, 6.0};

    // An edge of the cube: both ends projected, then a line between the pixels.
    const auto edge = [&](Vec3 from, Vec3 to, Color color) {
        DrawLine(canvas, ProjectVertex(viewport, from), ProjectVertex(viewport, to), color);
    };

    // The front face.
    edge(front_a, front_b, colors::kBlue);
    edge(front_b, front_c, colors::kBlue);
    edge(front_c, front_d, colors::kBlue);
    edge(front_d, front_a, colors::kBlue);

    // The back face.
    edge(back_a, back_b, colors::kRed);
    edge(back_b, back_c, colors::kRed);
    edge(back_c, back_d, colors::kRed);
    edge(back_d, back_a, colors::kRed);

    // The front-to-back edges.
    edge(front_a, back_a, colors::kGreen);
    edge(front_b, back_b, colors::kGreen);
    edge(front_c, back_c, colors::kGreen);
    edge(front_d, back_d, colors::kGreen);
}

void DrawObjectsScene(Canvas& canvas, const Viewport& viewport) {
    constexpr Color kBackground{18.0, 20.0, 28.0};
    constexpr Color kPurple{255.0, 0.0, 255.0};
    constexpr Color kCyan{0.0, 255.0, 255.0};
    constexpr Color kOrange{255.0, 128.0, 0.0};
    constexpr Color kLime{128.0, 255.0, 0.0};
    constexpr Color kSky{0.0, 128.0, 255.0};
    constexpr Color kPink{255.0, 0.0, 128.0};

    canvas.Clear(kBackground);

    // The book's cube: its eight corners at x, y, z = ±1, in the book's order, and its
    // six faces as two triangles each. The cube is then moved left and away from the
    // camera, as the book does by hand, since there are no transforms yet.
    const std::vector<Vec3> cube_corners = {
        {1.0, 1.0, 1.0},    {-1.0, 1.0, 1.0},   // 0, 1: the top edge of the back face
        {-1.0, -1.0, 1.0},  {1.0, -1.0, 1.0},   // 2, 3: the bottom edge of the back face
        {1.0, 1.0, -1.0},   {-1.0, 1.0, -1.0},  // 4, 5: the top edge of the front face
        {-1.0, -1.0, -1.0}, {1.0, -1.0, -1.0},  // 6, 7: the bottom edge of the front face
    };
    const std::vector<Triangle> cube_faces = {
        {{0, 1, 2}, colors::kRed},    {{0, 2, 3}, colors::kRed},     // back, z = 1
        {{4, 0, 3}, colors::kGreen},  {{4, 3, 7}, colors::kGreen},   // right, x = 1
        {{5, 4, 7}, colors::kBlue},   {{5, 7, 6}, colors::kBlue},    // front, z = -1
        {{1, 5, 6}, colors::kYellow}, {{1, 6, 2}, colors::kYellow},  // left, x = -1
        {{4, 5, 1}, kPurple},         {{4, 1, 0}, kPurple},          // top, y = 1
        {{2, 6, 7}, kCyan},           {{2, 7, 3}, kCyan},            // bottom, y = -1
    };
    const auto moved = [](const std::vector<Vec3>& points, Vec3 offset) {
        std::vector<Vec3> result;
        result.reserve(points.size());
        for (const Vec3 p : points) {
            result.push_back(p + offset);
        }
        return result;
    };
    RenderObject(canvas, viewport, moved(cube_corners, {-1.5, 0.0, 7.0}), cube_faces);

    // A square pyramid of the same height: the apex above the center of a base that is
    // the bottom face of the same cube, the base first so that every side owns its base
    // edge; moved to the right the same distance. Every edge of a closed surface belongs
    // to two triangles and shows the color of the one drawn later, which is why the base
    // and the cube's faces are left with their diagonals in their own color.
    const std::vector<Vec3> pyramid_corners = {
        {0.0, 1.0, 0.0},     // 0: the apex
        {-1.0, -1.0, -1.0},  // 1: front left
        {1.0, -1.0, -1.0},   // 2: front right
        {1.0, -1.0, 1.0},    // 3: back right
        {-1.0, -1.0, 1.0},   // 4: back left
    };
    const std::vector<Triangle> pyramid_faces = {
        {{1, 2, 3}, colors::kWhite}, {{1, 3, 4}, colors::kWhite},  // the base, y = -1
        {{0, 1, 2}, kOrange},                                      // front, z = -1
        {{0, 2, 3}, kLime},                                        // right, x = 1
        {{0, 3, 4}, kSky},                                         // back, z = 1
        {{0, 4, 1}, kPink},                                        // left, x = -1
    };
    RenderObject(canvas, viewport, moved(pyramid_corners, {1.5, 0.0, 7.0}), pyramid_faces);
}

void DrawAxesScene(Canvas& canvas) {
    constexpr Color kGray{160.0, 160.0, 160.0};
    constexpr Color kDarkGreen{0.0, 160.0, 0.0};

    const int half_w = canvas.width() / 2;
    const int half_h = canvas.height() / 2;
    for (int x = -half_w; x < half_w; ++x) {
        canvas.PutPixel(x, 0, kGray);
    }
    for (int y = -half_h; y < half_h; ++y) {
        canvas.PutPixel(0, y, kGray);
    }
    DrawDot(canvas, 0, 0, colors::kBlack);
    DrawDot(canvas, -half_w + 8, half_h - 9, colors::kRed);    // top-left
    DrawDot(canvas, half_w - 9, half_h - 9, kDarkGreen);       // top-right
    DrawDot(canvas, -half_w + 8, -half_h + 8, colors::kBlue);  // bottom-left
    DrawDot(canvas, half_w - 9, -half_h + 8, colors::kBlack);  // bottom-right
}

}  // namespace cgfs::demo
