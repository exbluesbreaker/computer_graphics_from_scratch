#include <cgfs/demo/rasterizer_scenes.h>

#include <cgfs/camera/camera.h>
#include <cgfs/camera/clipping_planes.h>
#include <cgfs/image/color.h>
#include <cgfs/image/depth_buffer.h>
#include <cgfs/lighting/light.h>
#include <cgfs/math/bounding_sphere.h>
#include <cgfs/math/mat4.h>
#include <cgfs/math/triangle_normal.h>
#include <cgfs/math/vec3.h>
#include <cgfs/rasterizer/culling.h>
#include <cgfs/rasterizer/instance.h>
#include <cgfs/rasterizer/line.h>
#include <cgfs/rasterizer/model.h>
#include <cgfs/rasterizer/object.h>
#include <cgfs/rasterizer/point.h>
#include <cgfs/rasterizer/projection.h>
#include <cgfs/rasterizer/scene.h>
#include <cgfs/rasterizer/shading.h>
#include <cgfs/rasterizer/triangle.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <initializer_list>
#include <map>
#include <string_view>
#include <utility>
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

// The models of the 3D pictures, each in its own coordinates with its center at the origin
// and 2 units tall, so that a picture places them by position alone.

constexpr Color kPurple{255.0, 0.0, 255.0};
constexpr Color kCyan{0.0, 255.0, 255.0};
constexpr Color kOrange{255.0, 128.0, 0.0};
constexpr Color kLime{128.0, 255.0, 0.0};
constexpr Color kSky{0.0, 128.0, 255.0};
constexpr Color kPink{255.0, 0.0, 128.0};

/// The book's cube: its eight corners at x, y, z = ±1, in the book's order, and its six
/// faces as two triangles each, one color per face.
Model Cube() {
    Model cube;
    cube.vertices = {
        {1.0, 1.0, 1.0},    {-1.0, 1.0, 1.0},   // 0, 1: the top edge of the back face
        {-1.0, -1.0, 1.0},  {1.0, -1.0, 1.0},   // 2, 3: the bottom edge of the back face
        {1.0, 1.0, -1.0},   {-1.0, 1.0, -1.0},  // 4, 5: the top edge of the front face
        {-1.0, -1.0, -1.0}, {1.0, -1.0, -1.0},  // 6, 7: the bottom edge of the front face
    };
    cube.triangles = {
        {{0, 1, 2}, colors::kRed},    {{0, 2, 3}, colors::kRed},     // back, z = 1
        {{4, 0, 3}, colors::kGreen},  {{4, 3, 7}, colors::kGreen},   // right, x = 1
        {{5, 4, 7}, colors::kBlue},   {{5, 7, 6}, colors::kBlue},    // front, z = -1
        {{1, 5, 6}, colors::kYellow}, {{1, 6, 2}, colors::kYellow},  // left, x = -1
        {{4, 5, 1}, kPurple},         {{4, 1, 0}, kPurple},          // top, y = 1
        {{2, 6, 7}, kCyan},           {{2, 7, 3}, kCyan},            // bottom, y = -1
    };
    return cube;
}

/// A square pyramid of the cube's height: the apex above the center of a base that is the
/// bottom face of the same cube. The base comes first so that every side owns its base
/// edge: an edge shared by two triangles shows the color of the one drawn later. Every
/// triangle lists its corners clockwise as seen from outside, like the cube's: a side
/// names the apex and then its two base corners from the right to the left of someone
/// who faces it.
Model Pyramid() {
    Model pyramid;
    pyramid.vertices = {
        {0.0, 1.0, 0.0},     // 0: the apex
        {-1.0, -1.0, -1.0},  // 1: front left
        {1.0, -1.0, -1.0},   // 2: front right
        {1.0, -1.0, 1.0},    // 3: back right
        {-1.0, -1.0, 1.0},   // 4: back left
    };
    pyramid.triangles = {
        {{1, 2, 3}, colors::kWhite}, {{1, 3, 4}, colors::kWhite},  // the base, y = -1
        {{0, 2, 1}, kOrange},                                      // front, z = -1
        {{0, 3, 2}, kLime},                                        // right, x = 1
        {{0, 4, 3}, kSky},                                         // back, z = 1
        {{0, 1, 4}, kPink},                                        // left, x = -1
    };
    return pyramid;
}

// The models of the clipping picture. Unlike the ones above they are made to be drawn
// with DrawShadedTriangle, so every triangle has an intensity at each corner, and they
// stand on their origin rather than around it: the base is in the plane y = 0, so an
// instance is put on the ground by its position and stays there when it is scaled.
//
// The clipping picture is drawn without a depth buffer: triangles are painted in list
// order and the later one shows. Each solid therefore lists its faces in an order that
// is right for a
// camera standing to its front left (at a smaller x and a smaller z than any of its
// corners): first the faces such a camera cannot see, the right, the back and the bottom,
// then the top, then the two that face it, the left and the front. A convex solid seen
// from outside comes out right whenever the faces turned away are painted before the
// faces turned toward the viewer, and the top may be either.

/// How bright each face of a solid is, as if lit from above and from the front left.
constexpr double kTopShade = 1.0;
constexpr double kLeftShade = 0.85;
constexpr double kFrontShade = 0.65;
constexpr double kRightShade = 0.5;
constexpr double kBackShade = 0.4;
constexpr double kBottomShade = 0.3;

/// Append the quadrilateral `corners` to the model as two triangles that share the
/// diagonal from the first corner to the third. The corners are listed clockwise as seen
/// from outside the solid, the book's winding, and `h` is the intensity at each of them.
void AddFace(Model& model, std::array<std::size_t, 4> corners, std::array<double, 4> h,
             Color color) {
    model.triangles.push_back({{corners[0], corners[1], corners[2]}, color, {h[0], h[1], h[2]}});
    model.triangles.push_back({{corners[0], corners[2], corners[3]}, color, {h[0], h[2], h[3]}});
}

/// A box 2 x `height` x 2 that stands on its origin, in one color. Each face has its own
/// shade, and the four walls darken toward the ground, to `kGroundShade` of that.
Model ShadedBox(double height, Color color) {
    constexpr double kGroundShade = 0.55;
    Model box;
    box.vertices = {
        {-1.0, 0.0, -1.0},    {1.0, 0.0, -1.0},     // 0, 1: the front edge of the base
        {1.0, 0.0, 1.0},      {-1.0, 0.0, 1.0},     // 2, 3: the back edge of the base
        {-1.0, height, -1.0}, {1.0, height, -1.0},  // 4, 5: the front edge of the top
        {1.0, height, 1.0},   {-1.0, height, 1.0},  // 6, 7: the back edge of the top
    };
    // A wall: two corners on the ground, then the two above them.
    const auto wall = [&](std::array<std::size_t, 4> corners, double shade) {
        AddFace(box, corners, {shade * kGroundShade, shade, shade, shade * kGroundShade}, color);
    };
    wall({1, 5, 6, 2}, kRightShade);  // right, x = 1
    wall({2, 6, 7, 3}, kBackShade);   // back, z = 1
    AddFace(box, {0, 1, 2, 3}, {kBottomShade, kBottomShade, kBottomShade, kBottomShade}, color);
    AddFace(box, {4, 7, 6, 5}, {kTopShade, kTopShade, kTopShade, kTopShade}, color);
    wall({3, 7, 4, 0}, kLeftShade);   // left, x = -1
    wall({0, 4, 5, 1}, kFrontShade);  // front, z = -1
    box.bounding_sphere = MakeBoundingSphere(box.vertices);
    return box;
}

/// A square pyramid on the base of the box above, its apex `height` above the middle of
/// it. Each side has the shade of the wall under it, darker toward its base.
Model ShadedPyramid(double height, Color color) {
    constexpr double kBaseShade = 0.7;
    Model pyramid;
    pyramid.vertices = {
        {-1.0, 0.0, -1.0},  {1.0, 0.0, -1.0},  // 0, 1: the front edge of the base
        {1.0, 0.0, 1.0},    {-1.0, 0.0, 1.0},  // 2, 3: the back edge of the base
        {0.0, height, 0.0},                    // 4: the apex
    };
    // A side: the apex, then its two corners on the base.
    const auto side = [&](std::size_t from, std::size_t to, double shade) {
        pyramid.triangles.push_back(
            {{4, from, to}, color, {shade, shade * kBaseShade, shade * kBaseShade}});
    };
    side(2, 1, kRightShade);  // right
    side(3, 2, kBackShade);   // back
    AddFace(pyramid, {0, 1, 2, 3}, {kBottomShade, kBottomShade, kBottomShade, kBottomShade}, color);
    side(0, 3, kLeftShade);   // left
    side(1, 0, kFrontShade);  // front
    pyramid.bounding_sphere = MakeBoundingSphere(pyramid.vertices);
    return pyramid;
}

/// A checkerboard floor in the plane y = 0: `tiles` x `tiles` squares of side 2 in two
/// colors, the corner of the first one at (`start`, 0, `start`). One model, far larger
/// than anything a camera standing on it can see. The intensity at a corner falls off
/// with x + z, its distance from the origin along the diagonal x = z, which is the way
/// the camera of the clipping picture looks from there, so the floor fades into the
/// distance.
Model Checkerboard(double start, int tiles, Color light, Color dark) {
    constexpr double kTile = 2.0;
    const auto shade = [](Vec3 corner) {
        return std::clamp(1.1 - 0.024 * (corner.x + corner.z), 0.2, 1.0);
    };
    Model floor;
    const int corners = tiles + 1;
    for (int row = 0; row < corners; ++row) {
        for (int column = 0; column < corners; ++column) {
            floor.vertices.push_back({start + kTile * column, 0.0, start + kTile * row});
        }
    }
    const auto corner = [&](int row, int column) {
        return static_cast<std::size_t>(row * corners + column);
    };
    for (int row = 0; row < tiles; ++row) {
        for (int column = 0; column < tiles; ++column) {
            // Clockwise as seen from above: near left, far left, far right, near right.
            const std::array<std::size_t, 4> tile = {corner(row, column), corner(row + 1, column),
                                                     corner(row + 1, column + 1),
                                                     corner(row, column + 1)};
            AddFace(floor, tile,
                    {shade(floor.vertices[tile[0]]), shade(floor.vertices[tile[1]]),
                     shade(floor.vertices[tile[2]]), shade(floor.vertices[tile[3]])},
                    (row + column) % 2 == 0 ? light : dark);
        }
    }
    floor.bounding_sphere = MakeBoundingSphere(floor.vertices);
    return floor;
}

// The models of the shading picture. They are lit, so what matters about them is which
// way their surfaces face: the normals.

/// `model` in one color: every triangle of it painted `color`.
Model Painted(Model model, Color color) {
    for (Triangle& triangle : model.triangles) {
        triangle.color = color;
    }
    return model;
}

/// A sphere of radius 1 around the origin as 8 * divisions^2 triangles, with the normals
/// of the sphere at their corners.
///
/// The corners are the points of an octahedron pushed out to the sphere. The octahedron
/// |x| + |y| + |z| = n has a vertex at every point (i, j, k) of whole numbers on it, n
/// steps along each of its edges, and those points cut each of its eight faces into n^2
/// small triangles; Normalized((i, j, k)) is the same point on the unit sphere. No cos
/// and no sin: a square root per vertex, which every platform rounds the same way.
///
/// The normal of a sphere at one of its points is the direction from the center to the
/// point, and the center is the origin: every corner's normal is the corner itself. The
/// triangles themselves are flat, and each has a normal of its own that none of its
/// corners has, which is the difference between a ball and a faceted ball under lights.
Model Sphere(int divisions, Color color) {
    const int n = divisions;
    Model sphere;

    // The vertex of the lattice point (i, j, k), made the first time it is asked for, so
    // that the triangles around a point share it.
    std::map<std::array<int, 3>, std::size_t> index_of;
    const auto vertex = [&](int i, int j, int k) {
        const auto [entry, is_new] = index_of.try_emplace({i, j, k}, sphere.vertices.size());
        if (is_new) {
            sphere.vertices.push_back(Normalized(
                Vec3{static_cast<double>(i), static_cast<double>(j), static_cast<double>(k)}));
        }
        return entry->second;
    };

    // The triangle of three vertices, listed clockwise as seen from outside: if its normal
    // points toward the center, two of its corners change places.
    const auto triangle = [&](std::size_t a, std::size_t b, std::size_t c) {
        const std::vector<Vec3>& v = sphere.vertices;
        if (Dot(TriangleNormal(v[a], v[b], v[c]), v[a] + v[b] + v[c]) < 0.0) {
            std::swap(b, c);
        }
        sphere.triangles.push_back({.v = {a, b, c}, .color = color, .normals = {v[a], v[b], v[c]}});
    };

    // One face of the octahedron per octant. The lattice points of the face x, y, z >= 0
    // are (a, b, c) with a + b + c = n; a small triangle points away from the corner
    // (0, 0, n) or, in the gaps between those, toward it.
    for (const int sx : {1, -1}) {
        for (const int sy : {1, -1}) {
            for (const int sz : {1, -1}) {
                const auto at = [&](int a, int b) {
                    return vertex(sx * a, sy * b, sz * (n - a - b));
                };
                for (int a = 0; a < n; ++a) {
                    for (int b = 0; a + b < n; ++b) {
                        triangle(at(a, b), at(a + 1, b), at(a, b + 1));
                        if (a + b + 1 < n) {
                            triangle(at(a + 1, b), at(a + 1, b + 1), at(a, b + 1));
                        }
                    }
                }
            }
        }
    }
    sphere.bounding_sphere = {.center = {0.0, 0.0, 0.0}, .radius = 1.0};
    return sphere;
}

/// A rectangular floor in the plane y = 0, `width` along X and `depth` along Z around
/// the origin, facing up: one face, two triangles, however large it is.
Model Floor(double width, double depth, Color color) {
    const double x = width / 2.0;
    const double z = depth / 2.0;
    Model floor;
    floor.vertices = {{-x, 0.0, -z}, {-x, 0.0, z}, {x, 0.0, z}, {x, 0.0, -z}};
    // Clockwise as seen from above: near left, far left, far right, near right.
    floor.triangles = {{{0, 1, 2}, color}, {{0, 2, 3}, color}};
    floor.bounding_sphere = MakeBoundingSphere(floor.vertices);
    return floor;
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

    canvas.Clear(kBackground);

    // The vertices of `model` moved by `offset`: at this point of the book an object is
    // moved by hand, since there are no instances yet.
    const auto moved = [](const Model& model, Vec3 offset) {
        std::vector<Vec3> result;
        result.reserve(model.vertices.size());
        for (const Vec3 p : model.vertices) {
            result.push_back(p + offset);
        }
        return result;
    };

    // The book's cube, moved left and away from the camera, and the pyramid moved the
    // other way. Every edge of a closed surface belongs to two triangles and shows the
    // color of the one drawn later, which is why the cube's faces are left with their
    // diagonals in their own color while every side of the pyramid keeps an edge of its
    // own.
    const Model cube = Cube();
    RenderObject(canvas, viewport, moved(cube, {-1.5, 0.0, 7.0}), cube.triangles);
    const Model pyramid = Pyramid();
    RenderObject(canvas, viewport, moved(pyramid, {1.5, 0.0, 7.0}), pyramid.triangles);
}

void DrawInstancesScene(Canvas& canvas, const Viewport& viewport) {
    constexpr Color kBackground{18.0, 20.0, 28.0};

    canvas.Clear(kBackground);

    // Two models and a list of instances that name them by index: the cube and the
    // pyramid of the objects picture, stored once each and placed sixteen times.
    const std::vector<Model> models = {Cube(), Pyramid()};
    constexpr std::size_t kCube = 0;
    constexpr std::size_t kPyramid = 1;
    std::vector<Instance> instances;

    // The camera stands 1.5 above the ground and looks down the middle of a street.
    constexpr double kGround = -1.5;

    // A building: `floors` cubes stacked on the ground and a pyramid for a roof. Both
    // models are 2 units tall and centered on their origin, so the first cube's center is
    // 1 above the ground, every further floor 2 higher, and the roof's base (y = -1 in
    // its own coordinates) meets the top floor's top (y = +1) when the roof stands 2
    // above it; the roof's white base edges then outline the eaves.
    const auto building = [&](double x, double z, int floors) {
        double y = kGround + 1.0;
        for (int floor = 0; floor < floors; ++floor, y += 2.0) {
            instances.push_back({.model = kCube, .position = {x, y, z}});
        }
        instances.push_back({.model = kPyramid, .position = {x, y, z}});
    };

    // A tower at the far end of the street, then a house on each side, far to near: in a
    // wireframe the instance drawn later owns the shared pixels, so the near edges show
    // where buildings overlap.
    building(0.0, 20.0, 3);
    for (const double z : {14.0, 10.0, 6.0}) {
        building(-3.5, z, 1);
        building(3.5, z, 1);
    }

    // A wireframe with nothing hidden, the picture of its chapter: the back faces are
    // outlined like the others.
    RenderScene(canvas, viewport, models, instances, Camera{}, DrawWireframeTriangle, nullptr,
                BackFaceCulling::kOff);
}

void DrawCameraScene(Canvas& canvas, const Viewport& viewport) {
    constexpr Color kBackground{18.0, 20.0, 28.0};

    canvas.Clear(kBackground);

    // The book's closing scene: one cube model, two instances with a model transform
    // each, and a camera that has left the origin.
    const std::vector<Model> models = {Cube()};
    constexpr std::size_t kCube = 0;
    const std::vector<Instance> instances = {
        {.model = kCube, .position = {-1.5, 0.0, 7.0}, .scale = 0.75},
        {.model = kCube, .position = {1.25, 2.5, 7.5}, .orientation = MakeOYRotationMatrix(195.0)},
    };

    // The camera stands to the left of the cubes, a little above the first, and turns 30
    // degrees to its right, toward them. Everything stays well in front of it, which
    // nothing enforces: this camera has no clipping planes. The clipping picture's has.
    const Camera camera{.position = {-3.0, 1.0, 2.0}, .orientation = MakeOYRotationMatrix(-30.0)};

    // A wireframe with nothing hidden, like the instances picture: no back-face culling.
    RenderScene(canvas, viewport, models, instances, camera, DrawWireframeTriangle, nullptr,
                BackFaceCulling::kOff);
}

void DrawClippingScene(Canvas& canvas, const Viewport& viewport) {
    constexpr Color kBackground{18.0, 20.0, 28.0};

    canvas.Clear(kBackground);

    // The models: the floor, and the solids that stand on it.
    constexpr std::size_t kFloor = 0;
    constexpr std::size_t kPlinth = 1;
    constexpr std::size_t kHouse = 2;
    constexpr std::size_t kRoof = 3;
    constexpr std::size_t kTower = 4;
    constexpr std::size_t kHighRise = 5;
    constexpr std::size_t kObelisk = 6;
    constexpr std::size_t kMast = 7;
    constexpr double kHouseHeight = 2.0;
    const std::vector<Model> models = {
        Checkerboard(-12.0, 24, {214.0, 208.0, 190.0}, {96.0, 108.0, 132.0}),
        ShadedBox(1.0, {226.0, 190.0, 110.0}),
        ShadedBox(kHouseHeight, {236.0, 226.0, 204.0}),
        ShadedPyramid(1.5, {214.0, 92.0, 62.0}),
        ShadedBox(5.0, {92.0, 150.0, 226.0}),
        ShadedBox(9.0, {72.0, 196.0, 176.0}),
        ShadedPyramid(6.0, {240.0, 200.0, 90.0}),
        ShadedBox(40.0, {226.0, 84.0, 104.0}),
    };

    // The city, as a map: every letter is a lot of 2 x 2 on a grid of 4, the camera's
    // corner at the bottom left, X to the right and Z up the page. The diagonal is left
    // open as an avenue.
    //
    //   .  nothing        p  a plinth      h  a house with a roof
    //   t  a tower        T  a high-rise   o  an obelisk
    constexpr std::string_view kCity[] = {
        "TthTho",  // z from 22 to 24, the farthest row
        "htTh.T",  // z from 18 to 20
        "Tht.tt",  // z from 14 to 16
        "th.hTh",  // z from 10 to 12
        "h.thtT",  // z from 6 to 8
        "pTthtT",  // z from 2 to 4, the nearest row
    };
    constexpr double kLotSpacing = 4.0;
    constexpr double kFirstLot = 3.0;  // the center of the nearest lot, along X and Z

    std::vector<Instance> instances;

    // The floor first: everything stands on it.
    instances.push_back({.model = kFloor});

    // A high-rise behind the camera, which no clipping plane has to cut: its bounding
    // sphere is behind the near plane and the whole instance is discarded at once.
    instances.push_back({.model = kHighRise, .position = {-6.0, 0.0, -6.0}});

    // The lots, from the farthest row to the nearest, and in each row from the right
    // end, the farthest from the camera, to the left: the order in which they must be
    // painted, as long as nothing sorts them by depth.
    const int rows = static_cast<int>(std::size(kCity));
    for (int row = 0; row < rows; ++row) {
        const std::string_view lots = kCity[row];
        const double z = kFirstLot + kLotSpacing * (rows - 1 - row);
        for (int column = static_cast<int>(lots.size()) - 1; column >= 0; --column) {
            const Vec3 lot{kFirstLot + kLotSpacing * column, 0.0, z};
            switch (lots[static_cast<std::size_t>(column)]) {
                case 'p':
                    instances.push_back({.model = kPlinth, .position = lot});
                    break;
                case 'h':
                    // The roof after the house under it: the camera is above the eaves.
                    instances.push_back({.model = kHouse, .position = lot});
                    instances.push_back(
                        {.model = kRoof, .position = lot + Vec3{0.0, kHouseHeight, 0.0}});
                    break;
                case 't':
                    instances.push_back({.model = kTower, .position = lot});
                    break;
                case 'T':
                    instances.push_back({.model = kHighRise, .position = lot});
                    break;
                case 'o':
                    instances.push_back({.model = kObelisk, .position = lot});
                    break;
                default:
                    break;
            }
        }
    }

    // A lantern an arm's length from the camera: a post, the mast scaled down to
    // 0.11 x 2.23 x 0.11, and on it a cap, the obelisk scaled down to 0.2 x 0.6 x 0.2.
    // The cap is at the height of the camera and its nearest corner is nearer than the
    // viewport, so the near plane cuts that corner off, and through the cut the inside
    // of the cap shows: its floor and the two sides beyond. The lantern comes after the
    // rows of lots, being nearer than all of them, and its cap after its post, like a
    // roof after its house.
    //
    // The post's height is not a round number on purpose. The cut corners of the cap's
    // floor are exactly on the near plane, at z = 1, so they project to (height - 2.5)
    // times the canvas height: with 2.2 that is row -54 of 180 exactly, the border
    // between two rows, and the last bit of the arithmetic would pick the row.
    constexpr Vec3 kLantern{1.16, 0.0, 0.41};
    constexpr double kPostHeight = 2.23;
    instances.push_back({.model = kMast, .position = kLantern, .scale = kPostHeight / 40.0});
    instances.push_back(
        {.model = kObelisk, .position = kLantern + Vec3{0.0, kPostHeight, 0.0}, .scale = 0.1});

    // The camera stands at the corner of the city, 2.5 above the ground, and looks down
    // the avenue: turned 42 degrees to its right. Every solid is ahead of it and to its
    // right in the world's X and Z, which is what the face order of the models and the
    // order of the instances assume. Its clipping volume is what the viewport shows.
    //
    // Not 45 degrees, along the avenue exactly: from the corner of a grid of whole
    // numbers that view projects one corner after another onto the border between two
    // columns of pixels (the corner (x, z) to 180 (x - z) / (x + z) on a canvas 180
    // high), where the last bit of cos and sin would decide the column.
    const Camera camera{.position = {0.0, 2.5, 0.0},
                        .orientation = MakeOYRotationMatrix(-42.0),
                        .clipping_planes = MakeClippingPlanes(viewport)};

    // Painted in list order with every face drawn, the way the picture was composed.
    // Without its back faces it would be the same city but for the lantern: through the
    // cut in its cap what stands behind it would show instead of the cap's own inside.
    RenderScene(canvas, viewport, models, instances, camera, DrawShadedTriangle, nullptr,
                BackFaceCulling::kOff);
}

void DrawFilledCubesScene(Canvas& canvas, const Viewport& viewport) {
    constexpr Color kBackground{18.0, 20.0, 28.0};

    canvas.Clear(kBackground);

    // One model, the cube of the objects picture: six faces in six colors, listed back,
    // right, front, left, top, bottom. Every instance is that cube as it stands, moved
    // and nothing else, and the camera is the default one, so every cube is drawn in
    // that one order whichever of its sides the camera sees. The scene is composed so
    // that the order is wrong almost everywhere. The two halves of hidden-surface
    // removal make it not matter: back-face culling takes away the faces a cube turns
    // away from the camera, and the depth buffer settles what is left.
    const std::vector<Model> models = {Cube()};
    constexpr std::size_t kCube = 0;
    std::vector<Instance> instances;

    // The cube from nine directions: a wall of 3 x 3 cubes facing the camera, the middle
    // one straight ahead. From outside a cube shows three faces at most: here the blue
    // front and, off the axis, the side and the top or bottom turned toward the camera.
    // Painted in list order the faces turned away would be drawn all the same, and those
    // listed after the front (the left, the top and the bottom) over it. They are back
    // faces, and RenderScene removes them before anything is drawn: 21 faces of the
    // wall's 54 are left.
    constexpr double kWallSpacing = 3.5;
    constexpr double kWallDistance = 12.0;
    for (const double y : {kWallSpacing, 0.0, -kWallSpacing}) {
        for (const double x : {-kWallSpacing, 0.0, kWallSpacing}) {
            instances.push_back({.model = kCube, .position = {x, y, kWallDistance}});
        }
    }

    // Beside the wall, on each side, a row of three cubes that leads away from the camera
    // and outward, a little below it, so that each cube stands partly behind the one
    // before. The rows are listed from the nearest cube to the farthest, the wrong way
    // around for painting in list order, which would put every cube over the cube in
    // front of it. Culling does not help here: the faces that overlap are front faces of
    // different cubes. Through the depth buffer the nearer cube keeps its pixels.
    constexpr Vec3 kRowStart{7.0, -2.0, 12.0};
    constexpr Vec3 kRowStep{5.5, 0.0, 6.0};
    for (const double side : {-1.0, 1.0}) {
        for (int k = 0; k < 3; ++k) {
            const Vec3 right = kRowStart + kRowStep * k;
            instances.push_back({.model = kCube, .position = {side * right.x, right.y, right.z}});
        }
    }

    // One depth buffer for the picture, shared by all the cubes. Back-face culling is
    // RenderScene's default: of the 180 triangles 78 are drawn.
    DepthBuffer depth_buffer(canvas);
    RenderScene(canvas, viewport, models, instances, Camera{}, DrawFilledTriangle, &depth_buffer);
}

void DrawShadingScene(Canvas& canvas, const Viewport& viewport, ShadingModel shading_model) {
    constexpr Color kBackground{18.0, 20.0, 28.0};

    canvas.Clear(kBackground);

    // Five models, each there to be right from one shading model on and wrong before it.
    //
    // The floor is one face of two triangles, 10 x 8, with a lamp less than a unit above
    // it: the large face with a light close to it that only lighting every pixel gets
    // right. The cube has flat faces, which one light value per triangle is nearly enough
    // for. The two balls have the normals of a sphere at the corners of their triangles,
    // which lighting the corners is enough for as long as the ball is matte; the second
    // ball is shiny, and a highlight is smaller than its triangles.
    //
    // The bulb marks where the lamp is, and is not lit like the rest: its color is ten
    // times too bright, so the ambient light alone takes its red and green to the top
    // of the scale, and it glows the same under every shading model.
    constexpr std::size_t kFloor = 0;
    constexpr std::size_t kCube = 1;
    constexpr std::size_t kMatteBall = 2;
    constexpr std::size_t kShinyBall = 3;
    constexpr std::size_t kBulb = 4;
    std::vector<Model> models = {
        Floor(10.0, 8.0, {206.0, 198.0, 180.0}), Painted(Cube(), {70.0, 140.0, 230.0}),
        Sphere(6, {90.0, 200.0, 110.0}),         Sphere(6, {214.0, 52.0, 48.0}),
        Sphere(4, {2550.0, 2040.0, 300.0}),
    };
    models[kCube].bounding_sphere = MakeBoundingSphere(models[kCube].vertices);
    models[kShinyBall].specular = 30.0;

    // The stage: the floor around the world's origin, the cube and the two balls standing
    // on it in a row from left to right, and the lamp in front of the shiny ball, 0.9
    // above the floor. The cube stands the farthest from the lamp, where the lamp's light
    // changes least across a face.
    constexpr Vec3 kLamp{1.5, 0.9, -1.9};
    constexpr double kBallRadius = 1.2;
    const std::vector<Instance> instances = {
        {.model = kFloor},
        {.model = kCube, .position = {-3.6, 1.0, 2.2}},
        {.model = kMatteBall, .position = {-0.5, kBallRadius, 1.5}, .scale = kBallRadius},
        {.model = kShinyBall, .position = {2.7, kBallRadius, 0.6}, .scale = kBallRadius},
        {.model = kBulb, .position = kLamp, .scale = 0.14},
    };

    // The lights, in the world's coordinates like the instances: a little light from
    // everywhere, a sun high up to the left and behind the camera, and the lamp.
    const std::vector<Light> lights = {
        AmbientLight{.intensity = 0.2},
        DirectionalLight{.intensity = 0.25, .direction = {-1.0, 2.0, -1.5}},
        PointLight{.intensity = 0.65, .position = kLamp},
    };

    // The camera stands in front of the stage and above it and looks down on it: turned
    // about the X axis by the angle whose cosine and sine are 4/5 and 3/5, the angle of
    // the 3-4-5 triangle, about 36.9 degrees. Its forward axis (0, 0, 1) becomes
    // (0, -3/5, 4/5) and its up axis (0, 1, 0) becomes (0, 4/5, 3/5), the third and the
    // second column of the matrix. Written out rather than built from an angle so that
    // no cos or sin is called: with the sphere's square roots, every vertex of this
    // picture is plain IEEE arithmetic, which every platform rounds the same way.
    constexpr double kCosine = 0.8;
    constexpr double kSine = 0.6;
    constexpr Mat4 kLookingDown{{{1.0, 0.0, 0.0, 0.0},
                                 {0.0, kCosine, -kSine, 0.0},
                                 {0.0, kSine, kCosine, 0.0},
                                 {0.0, 0.0, 0.0, 1.0}}};
    const Camera camera{.position = {0.0, 5.4, -7.4},
                        .orientation = kLookingDown,
                        .clipping_planes = MakeClippingPlanes(viewport)};

    // Solid faces in any order: one depth buffer for the picture, and back-face culling,
    // RenderScene's default. The floor is not a closed object, but it is only ever seen
    // from above, which is its front.
    DepthBuffer depth_buffer(canvas);
    RenderScene(canvas, viewport, models, instances, camera, lights, shading_model, &depth_buffer);
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
