#include <cgfs/camera/clipping_planes.h>
#include <cgfs/camera/viewport.h>
#include <cgfs/image/color.h>
#include <cgfs/math/bounding_sphere.h>
#include <cgfs/math/mat4.h>
#include <cgfs/math/plane.h>
#include <cgfs/math/vec3.h>
#include <cgfs/rasterizer/clipping.h>
#include <cgfs/rasterizer/model.h>
#include <cgfs/rasterizer/object.h>
#include <cgfs/rasterizer/point.h>
#include <cgfs/rasterizer/projection.h>

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <vector>

#include "support/test_helpers.h"

using cgfs::BoundingSphere;
using cgfs::ClipInstance;
using cgfs::ClipInstanceAgainstPlane;
using cgfs::ClipScene;
using cgfs::ClipTriangle;
using cgfs::ClipTrianglesAgainstPlane;
using cgfs::Cross;
using cgfs::Dot;
using cgfs::Length;
using cgfs::MakeBoundingSphere;
using cgfs::MakeClippingPlanes;
using cgfs::Model;
using cgfs::Plane;
using cgfs::SignedDistance;
using cgfs::TransformModel;
using cgfs::Triangle;
using cgfs::Vec3;
using cgfs::Viewport;
using namespace cgfs::colors;

namespace {

constexpr double kEpsilon = 1e-12;

// The near plane of a viewport at distance 1: z = 1, with +Z in front. Its normal is an
// axis, so with coordinates that are small binary fractions every distance, every
// crossing fraction and every crossing point below is exact.
constexpr Plane kNear{.normal = {0.0, 0.0, 1.0}, .d = -1.0};

// The plane x = 0 with -X in front: what is left of it is kept.
constexpr Plane kLeftHalf{.normal = {-1.0, 0.0, 0.0}, .d = 0.0};

using Corners = std::array<std::size_t, 3>;
using Intensities = std::array<double, 3>;

// One triangle clipped against one plane: the vertex list afterwards and the triangles
// that are left.
struct Clipped {
    std::vector<Vec3> vertices;
    std::vector<Triangle> triangles;
};
Clipped Clip(std::vector<Vec3> vertices, const Triangle& triangle, const Plane& plane) {
    std::vector<Triangle> triangles;
    ClipTriangle(triangle, plane, vertices, triangles);
    return {vertices, triangles};
}

// The vector perpendicular to a triangle whose length is twice its area; its direction
// tells the triangle's winding.
Vec3 DoubledAreaNormal(const std::vector<Vec3>& vertices, const Triangle& triangle) {
    const Vec3 a = vertices[triangle.v[0]];
    return Cross(vertices[triangle.v[1]] - a, vertices[triangle.v[2]] - a);
}

double Area(const std::vector<Vec3>& vertices, const std::vector<Triangle>& triangles) {
    double area = 0.0;
    for (const Triangle& triangle : triangles) {
        area += Length(DoubledAreaNormal(vertices, triangle)) / 2.0;
    }
    return area;
}

// Every vertex is in front of the plane or on it, and every vertex is named by a triangle.
bool AllInFront(const Model& model, const Plane& plane) {
    for (const Vec3 vertex : model.vertices) {
        if (SignedDistance(plane, vertex) < -kEpsilon) {
            return false;
        }
    }
    return true;
}
bool EveryVertexIsUsed(const Model& model) {
    std::vector<bool> used(model.vertices.size(), false);
    for (const Triangle& triangle : model.triangles) {
        for (const std::size_t index : triangle.v) {
            used[index] = true;
        }
    }
    for (const bool is_used : used) {
        if (!is_used) {
            return false;
        }
    }
    return true;
}

// A 2 x 2 square in the plane y = 0 whose near edge is at z = `near_z`: two triangles
// that share the diagonal from the near left corner to the far right one, with the
// bounding sphere around them (the center of the square, radius sqrt(2)).
Model Square(double near_z, cgfs::Color color = kWhite) {
    Model square;
    square.vertices = {{-1.0, 0.0, near_z},
                       {1.0, 0.0, near_z},
                       {1.0, 0.0, near_z + 2.0},
                       {-1.0, 0.0, near_z + 2.0}};
    square.triangles = {{{0, 1, 2}, color}, {{0, 2, 3}, color}};
    square.bounding_sphere = MakeBoundingSphere(square.vertices);
    return square;
}

// The book's cube, corners at x, y, z = ±1, with its bounding sphere.
Model Cube() {
    Model cube;
    cube.vertices = {{1.0, 1.0, 1.0},  {-1.0, 1.0, 1.0},  {-1.0, -1.0, 1.0},  {1.0, -1.0, 1.0},
                     {1.0, 1.0, -1.0}, {-1.0, 1.0, -1.0}, {-1.0, -1.0, -1.0}, {1.0, -1.0, -1.0}};
    cube.triangles = {
        {{0, 1, 2}, kRed},   {{0, 2, 3}, kRed},   {{4, 0, 3}, kGreen}, {{4, 3, 7}, kGreen},
        {{5, 4, 7}, kBlue},  {{5, 7, 6}, kBlue},  {{1, 5, 6}, kWhite}, {{1, 6, 2}, kWhite},
        {{4, 5, 1}, kBlack}, {{4, 1, 0}, kBlack}, {{2, 6, 7}, kRed},   {{2, 7, 3}, kRed},
    };
    cube.bounding_sphere = MakeBoundingSphere(cube.vertices);
    return cube;
}

}  // namespace

// --- ClipTriangle ---------------------------------------------------------------------

TEST_CASE("ClipTriangle: a triangle in front of the plane is kept as it is", "[clipping]") {
    const std::vector<Vec3> vertices = {{0.0, 0.0, 2.0}, {1.0, 0.0, 3.0}, {0.0, 1.0, 5.0}};
    const Triangle triangle{{0, 1, 2}, kRed, {0.25, 0.5, 0.75}};
    const Clipped clipped = Clip(vertices, triangle, kNear);
    CHECK(clipped.triangles == std::vector<Triangle>{triangle});
    CHECK(clipped.vertices == vertices);  // no new vertex
}

TEST_CASE("ClipTriangle: a triangle behind the plane is dropped", "[clipping]") {
    const std::vector<Vec3> vertices = {{0.0, 0.0, 0.5}, {1.0, 0.0, -3.0}, {0.0, 1.0, 0.0}};
    const Clipped clipped = Clip(vertices, {{0, 1, 2}, kRed}, kNear);
    CHECK(clipped.triangles.empty());
    CHECK(clipped.vertices == vertices);
}

TEST_CASE("ClipTriangle: one corner in front leaves the tip of the triangle", "[clipping]") {
    // A is 2 in front of the plane, B 6 behind it and C 2 behind it: the side AB crosses
    // a quarter of the way along, AC half way.
    const std::vector<Vec3> vertices = {
        {0.0, 0.0, 3.0},   // A
        {6.0, 0.0, -5.0},  // B
        {0.0, 4.0, -1.0},  // C
    };
    const Clipped clipped = Clip(vertices, {{0, 1, 2}, kRed, {1.0, 0.0, 0.25}}, kNear);

    // The two crossing points are appended to the vertex list, B' then C'.
    REQUIRE(clipped.vertices.size() == 5);
    CHECK(clipped.vertices[0] == vertices[0]);
    CHECK(clipped.vertices[1] == vertices[1]);
    CHECK(clipped.vertices[2] == vertices[2]);
    CHECK(clipped.vertices[3] == Vec3{1.5, 0.0, 1.0});  // B' = A + 1/4 (B - A)
    CHECK(clipped.vertices[4] == Vec3{0.0, 2.0, 1.0});  // C' = A + 1/2 (C - A)

    // The triangle A B' C', with the intensities interpolated by the same fractions:
    // 1 + 1/4 (0 - 1) and 1 + 1/2 (0.25 - 1).
    REQUIRE(clipped.triangles.size() == 1);
    CHECK(clipped.triangles[0] == Triangle{{0, 3, 4}, kRed, {1.0, 0.75, 0.625}});
}

TEST_CASE("ClipTriangle: the corner in front may be any of the three", "[clipping]") {
    const std::vector<Vec3> vertices = {{0.0, 0.0, 3.0}, {6.0, 0.0, -5.0}, {0.0, 4.0, -1.0}};
    const Intensities h_by_vertex = {1.0, 0.0, 0.25};
    // The same triangle with its corners rotated, so that A comes first, second or last.
    // The piece that is left always starts at A and goes around the same way.
    for (const Corners corners : {Corners{0, 1, 2}, Corners{1, 2, 0}, Corners{2, 0, 1}}) {
        const Triangle triangle{
            corners,
            kRed,
            {h_by_vertex[corners[0]], h_by_vertex[corners[1]], h_by_vertex[corners[2]]}};
        const Clipped clipped = Clip(vertices, triangle, kNear);
        REQUIRE(clipped.triangles.size() == 1);
        CHECK(clipped.triangles[0] == Triangle{{0, 3, 4}, kRed, {1.0, 0.75, 0.625}});
        CHECK(clipped.vertices[3] == Vec3{1.5, 0.0, 1.0});
        CHECK(clipped.vertices[4] == Vec3{0.0, 2.0, 1.0});
    }

    // Listed the other way around, A C B, it stays the other way around: A C' B'.
    const Clipped reversed = Clip(vertices, {{0, 2, 1}, kRed, {1.0, 0.25, 0.0}}, kNear);
    REQUIRE(reversed.triangles.size() == 1);
    CHECK(reversed.triangles[0] == Triangle{{0, 3, 4}, kRed, {1.0, 0.625, 0.75}});
    CHECK(reversed.vertices[3] == Vec3{0.0, 2.0, 1.0});  // C' is computed first now
    CHECK(reversed.vertices[4] == Vec3{1.5, 0.0, 1.0});
}

TEST_CASE("ClipTriangle: two corners in front leave a quadrilateral, as two triangles",
          "[clipping]") {
    // A and B are 2 in front of the plane and C 2 behind it: both sides to C cross half
    // way along.
    const std::vector<Vec3> vertices = {
        {0.0, 0.0, 3.0},   // A
        {4.0, 0.0, 3.0},   // B
        {0.0, 4.0, -1.0},  // C
    };
    const Clipped clipped = Clip(vertices, {{0, 1, 2}, kBlue, {1.0, 0.5, 0.0}}, kNear);

    REQUIRE(clipped.vertices.size() == 5);
    CHECK(clipped.vertices[3] == Vec3{0.0, 2.0, 1.0});  // A' on AC
    CHECK(clipped.vertices[4] == Vec3{2.0, 2.0, 1.0});  // B' on BC

    // A B A' and A' B B'; the intensities at A' and B' are half way to C's 0.
    REQUIRE(clipped.triangles.size() == 2);
    CHECK(clipped.triangles[0] == Triangle{{0, 1, 3}, kBlue, {1.0, 0.5, 0.5}});
    CHECK(clipped.triangles[1] == Triangle{{3, 1, 4}, kBlue, {0.5, 0.5, 0.25}});
}

TEST_CASE("ClipTriangle: the corner behind may be any of the three", "[clipping]") {
    const std::vector<Vec3> vertices = {{0.0, 0.0, 3.0}, {4.0, 0.0, 3.0}, {0.0, 4.0, -1.0}};
    const Intensities h_by_vertex = {1.0, 0.5, 0.0};
    for (const Corners corners : {Corners{0, 1, 2}, Corners{1, 2, 0}, Corners{2, 0, 1}}) {
        const Triangle triangle{
            corners,
            kBlue,
            {h_by_vertex[corners[0]], h_by_vertex[corners[1]], h_by_vertex[corners[2]]}};
        const Clipped clipped = Clip(vertices, triangle, kNear);
        REQUIRE(clipped.triangles.size() == 2);
        CHECK(clipped.triangles[0] == Triangle{{0, 1, 3}, kBlue, {1.0, 0.5, 0.5}});
        CHECK(clipped.triangles[1] == Triangle{{3, 1, 4}, kBlue, {0.5, 0.5, 0.25}});
    }
}

TEST_CASE(
    "ClipTriangle: the pieces keep the winding of the triangle and add up to the part in "
    "front",
    "[clipping]") {
    struct Case {
        std::vector<Vec3> vertices;
        double fraction_in_front;  // of the triangle's area
    };
    const std::vector<Case> cases = {
        // One corner in front, its sides cut at 1/4 and 1/2: the tip is 1/8 of the area.
        {{{0.0, 0.0, 3.0}, {6.0, 0.0, -5.0}, {0.0, 4.0, -1.0}}, 0.125},
        // Two corners in front, the sides to the third cut half way: the tip that is cut
        // off is 1/4 of the area.
        {{{0.0, 0.0, 3.0}, {4.0, 0.0, 3.0}, {0.0, 4.0, -1.0}}, 0.75},
    };
    for (const Case& c : cases) {
        for (const Corners corners : {Corners{0, 1, 2}, Corners{1, 2, 0}, Corners{2, 0, 1},
                                      Corners{0, 2, 1}, Corners{2, 1, 0}, Corners{1, 0, 2}}) {
            const Triangle triangle{corners, kWhite};
            const Vec3 normal = DoubledAreaNormal(c.vertices, triangle);
            const Clipped clipped = Clip(c.vertices, triangle, kNear);
            REQUIRE_FALSE(clipped.triangles.empty());
            for (const Triangle& piece : clipped.triangles) {
                const Vec3 piece_normal = DoubledAreaNormal(clipped.vertices, piece);
                CHECK(Dot(piece_normal, normal) > 0.0);                 // the same way round
                CHECK(Length(Cross(piece_normal, normal)) < kEpsilon);  // in the same plane
            }
            CHECK(std::abs(Area(clipped.vertices, clipped.triangles) -
                           c.fraction_in_front * Length(normal) / 2.0) < kEpsilon);
        }
    }
}

TEST_CASE("ClipTriangle: a corner on the plane is neither in front nor behind", "[clipping]") {
    // Touching the plane from the front, with one corner or with a whole side: kept.
    const std::vector<Vec3> touching_in_front = {{0.0, 0.0, 1.0}, {1.0, 0.0, 2.0}, {0.0, 1.0, 3.0}};
    const std::vector<Vec3> standing_on_it = {{0.0, 0.0, 1.0}, {1.0, 0.0, 1.0}, {0.0, 1.0, 3.0}};
    for (const std::vector<Vec3>& vertices : {touching_in_front, standing_on_it}) {
        const Triangle triangle{{0, 1, 2}, kRed};
        const Clipped clipped = Clip(vertices, triangle, kNear);
        CHECK(clipped.triangles == std::vector<Triangle>{triangle});
        CHECK(clipped.vertices == vertices);
    }

    // Touching it from behind: dropped, rather than kept as a point or a line.
    const std::vector<Vec3> touching_behind = {{0.0, 0.0, 1.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, -1.0}};
    const std::vector<Vec3> hanging_from_it = {{0.0, 0.0, 1.0}, {1.0, 0.0, 1.0}, {0.0, 1.0, -1.0}};
    for (const std::vector<Vec3>& vertices : {touching_behind, hanging_from_it}) {
        const Clipped clipped = Clip(vertices, {{0, 1, 2}, kRed}, kNear);
        CHECK(clipped.triangles.empty());
        CHECK(clipped.vertices == vertices);
    }

    // Lying in the plane: kept.
    const std::vector<Vec3> in_the_plane = {{0.0, 0.0, 1.0}, {1.0, 0.0, 1.0}, {0.0, 1.0, 1.0}};
    CHECK(Clip(in_the_plane, {{0, 1, 2}, kRed}, kNear).triangles.size() == 1);
}

TEST_CASE("ClipTriangle: a triangle cut through one of its corners gives one triangle",
          "[clipping]") {
    // A in front, B on the plane, C behind: only AC really crosses. The piece is A B C',
    // one triangle and no sliver; B' is B itself, found at the end of the side AB.
    const std::vector<Vec3> vertices = {{0.0, 0.0, 3.0}, {4.0, 0.0, 1.0}, {0.0, 4.0, -1.0}};
    const Clipped clipped = Clip(vertices, {{0, 1, 2}, kRed, {1.0, 0.5, 0.0}}, kNear);
    REQUIRE(clipped.triangles.size() == 1);
    REQUIRE(clipped.vertices.size() == 5);
    CHECK(clipped.vertices[3] == vertices[1]);          // B' = B
    CHECK(clipped.vertices[4] == Vec3{0.0, 2.0, 1.0});  // C'
    CHECK(clipped.triangles[0] == Triangle{{0, 3, 4}, kRed, {1.0, 0.5, 0.5}});
}

TEST_CASE("ClipTriangle: two triangles that share a cut side get the same crossing point",
          "[clipping]") {
    // P is in front and Q behind; the side PQ belongs to both triangles, which list it
    // in opposite directions, as neighbours on a surface do. Nothing here is a round
    // number, so the crossing point is rounded, and it must be rounded the same way
    // twice for the two pieces to still meet.
    std::vector<Vec3> vertices = {
        {0.1, 0.2, 2.3},    // 0: P, in front
        {0.7, -0.4, -0.9},  // 1: Q, behind
        {1.3, 0.9, 2.1},    // 2: R1, in front
        {-0.5, 0.3, -1.7},  // 3: R2, behind
    };
    std::vector<Triangle> clipped;
    ClipTriangle({{0, 1, 2}, kRed}, kNear, vertices, clipped);  // P Q R1: two in front
    ClipTriangle({{1, 0, 3}, kRed}, kNear, vertices, clipped);  // Q P R2: one in front
    REQUIRE(clipped.size() == 3);
    REQUIRE(vertices.size() == 8);
    // The first triangle appended the crossings of R1 Q and P Q, the second those of
    // P R2 and P Q.
    CHECK(vertices[5] == vertices[7]);  // bit for bit
    CHECK(std::abs(vertices[5].z - 1.0) < kEpsilon);
}

// --- ClipTrianglesAgainstPlane --------------------------------------------------------

TEST_CASE("ClipTrianglesAgainstPlane: every triangle in order, the new vertices after the old",
          "[clipping]") {
    std::vector<Vec3> vertices = {
        {0.0, 0.0, 2.0}, {1.0, 0.0, 3.0},  {0.0, 1.0, 5.0},   // 0 1 2: in front
        {0.0, 0.0, 0.0}, {1.0, 0.0, -3.0}, {0.0, 1.0, 0.5},   // 3 4 5: behind
        {0.0, 0.0, 3.0}, {4.0, 0.0, 3.0},  {0.0, 4.0, -1.0},  // 6 7 8: two corners in front
    };
    const std::vector<Triangle> triangles = {
        {{0, 1, 2}, kRed}, {{3, 4, 5}, kGreen}, {{6, 7, 8}, kBlue}};
    const std::vector<Triangle> clipped = ClipTrianglesAgainstPlane(triangles, kNear, vertices);
    CHECK(clipped ==
          std::vector<Triangle>{{{0, 1, 2}, kRed}, {{6, 7, 9}, kBlue}, {{9, 7, 10}, kBlue}});
    REQUIRE(vertices.size() == 11);
    CHECK(vertices[9] == Vec3{0.0, 2.0, 1.0});
    CHECK(vertices[10] == Vec3{2.0, 2.0, 1.0});

    std::vector<Vec3> none;
    CHECK(ClipTrianglesAgainstPlane({}, kNear, none).empty());
}

// --- ClipInstanceAgainstPlane ---------------------------------------------------------

TEST_CASE("ClipInstanceAgainstPlane: an instance whose sphere is in front is returned as it is",
          "[clipping]") {
    const Model square = Square(3.0);  // z from 3 to 5: the sphere's center is 3 in front
    const std::optional<Model> clipped = ClipInstanceAgainstPlane(square, kNear);
    REQUIRE(clipped.has_value());
    CHECK(*clipped == square);
}

TEST_CASE("ClipInstanceAgainstPlane: an instance whose sphere is behind is discarded",
          "[clipping]") {
    CHECK_FALSE(ClipInstanceAgainstPlane(Square(-5.0), kNear).has_value());  // z from -5 to -3
}

TEST_CASE(
    "ClipInstanceAgainstPlane: an instance whose sphere straddles the plane has its "
    "triangles clipped",
    "[clipping]") {
    const Model square = Square(0.0);  // z from 0 to 2: the plane z = 1 cuts it in half
    const std::optional<Model> clipped = ClipInstanceAgainstPlane(square, kNear);
    REQUIRE(clipped.has_value());

    // The near triangle 0 1 2 keeps the tip at its far corner; the far triangle 0 2 3
    // keeps a quadrilateral, two triangles.
    CHECK(clipped->triangles.size() == 3);
    CHECK(std::abs(Area(clipped->vertices, clipped->triangles) - 2.0) < kEpsilon);  // half of 4
    CHECK(AllInFront(*clipped, kNear));

    // The two corners that were cut off are gone and three crossing points have come:
    // one on each side of the square, and the one on the diagonal twice, once for each
    // of the triangles that share it, at the same place.
    CHECK(clipped->vertices == std::vector<Vec3>{{1.0, 0.0, 2.0},     // the far right corner
                                                 {0.0, 0.0, 1.0},     // on the diagonal
                                                 {1.0, 0.0, 1.0},     // on the right side
                                                 {-1.0, 0.0, 2.0},    // the far left corner
                                                 {0.0, 0.0, 1.0},     // on the diagonal again
                                                 {-1.0, 0.0, 1.0}});  // on the left side
    CHECK(clipped->triangles ==
          std::vector<Triangle>{{{0, 1, 2}, kWhite}, {{0, 3, 4}, kWhite}, {{4, 3, 5}, kWhite}});
    CHECK(EveryVertexIsUsed(*clipped));

    // What is left is still inside the sphere, which is kept.
    CHECK(clipped->bounding_sphere == square.bounding_sphere);
}

TEST_CASE("ClipInstanceAgainstPlane: the sphere is trusted", "[clipping]") {
    // The triangles are behind the plane, but the sphere says in front: returned as it is.
    Model claims_front = Square(-5.0);
    claims_front.bounding_sphere = {.center = {0.0, 0.0, 10.0}, .radius = 1.0};
    const std::optional<Model> kept = ClipInstanceAgainstPlane(claims_front, kNear);
    REQUIRE(kept.has_value());
    CHECK(*kept == claims_front);

    // The triangles are in front, but the sphere says behind: discarded.
    Model claims_behind = Square(3.0);
    claims_behind.bounding_sphere = {.center = {0.0, 0.0, -10.0}, .radius = 1.0};
    CHECK_FALSE(ClipInstanceAgainstPlane(claims_behind, kNear).has_value());
}

TEST_CASE("ClipInstanceAgainstPlane: a sphere that only touches the plane decides nothing",
          "[clipping]") {
    // The sphere's center is exactly one radius in front (d == r) or behind (d == -r):
    // neither test is met and the triangles are examined, which here are on the other
    // side from where the sphere would put them.
    Model touching_in_front = Square(-5.0);
    touching_in_front.bounding_sphere = {.center = {0.0, 0.0, 3.0}, .radius = 2.0};
    const std::optional<Model> emptied = ClipInstanceAgainstPlane(touching_in_front, kNear);
    REQUIRE(emptied.has_value());
    CHECK(emptied->triangles.empty());

    Model touching_behind = Square(3.0);
    touching_behind.bounding_sphere = {.center = {0.0, 0.0, -1.0}, .radius = 2.0};
    const std::optional<Model> kept = ClipInstanceAgainstPlane(touching_behind, kNear);
    REQUIRE(kept.has_value());
    CHECK(kept->triangles == Square(3.0).triangles);
}

TEST_CASE("ClipInstanceAgainstPlane: without a real sphere every triangle is examined",
          "[clipping]") {
    // The default bounding sphere contains everything, so it is never entirely on one
    // side. In front: the same triangles and vertices.
    Model in_front = Square(3.0);
    in_front.bounding_sphere = BoundingSphere{};
    const std::optional<Model> kept = ClipInstanceAgainstPlane(in_front, kNear);
    REQUIRE(kept.has_value());
    CHECK(*kept == in_front);

    // Behind: an instance is still returned, with nothing in it.
    Model behind = Square(-5.0);
    behind.bounding_sphere = BoundingSphere{};
    const std::optional<Model> emptied = ClipInstanceAgainstPlane(behind, kNear);
    REQUIRE(emptied.has_value());
    CHECK(emptied->triangles.empty());
    CHECK(emptied->vertices.empty());

    // Straddling: clipped as with the real sphere.
    Model straddling = Square(0.0);
    straddling.bounding_sphere = BoundingSphere{};
    const std::optional<Model> clipped = ClipInstanceAgainstPlane(straddling, kNear);
    REQUIRE(clipped.has_value());
    CHECK(clipped->triangles == ClipInstanceAgainstPlane(Square(0.0), kNear)->triangles);
    CHECK(clipped->vertices == ClipInstanceAgainstPlane(Square(0.0), kNear)->vertices);
}

TEST_CASE("ClipInstanceAgainstPlane: vertices that no triangle names are removed", "[clipping]") {
    // A vertex behind the camera that no triangle uses: harmless in the model, fatal to
    // the projection. Clipping the instance drops it.
    Model square = Square(0.0);
    square.vertices.push_back({0.0, 0.0, -7.0});
    const std::optional<Model> clipped = ClipInstanceAgainstPlane(square, kNear);
    REQUIRE(clipped.has_value());
    CHECK(clipped->vertices.size() == 6);
    CHECK(AllInFront(*clipped, kNear));
    CHECK(EveryVertexIsUsed(*clipped));
}

// --- ClipInstance ---------------------------------------------------------------------

TEST_CASE("ClipInstance: with no planes the instance is returned as it is", "[clipping]") {
    const Model square = Square(-5.0);
    const std::optional<Model> clipped = ClipInstance(square, {});
    REQUIRE(clipped.has_value());
    CHECK(*clipped == square);
}

TEST_CASE("ClipInstance: each plane clips what the one before left", "[clipping]") {
    // The square reaches from x = -1 to 1 and from z = 0 to 2. In front of z = 1 is its
    // far half, and to the left of x = 0 the left half of that: a quarter, 1 x 1.
    const std::array<Plane, 2> planes = {kNear, kLeftHalf};
    const std::optional<Model> clipped = ClipInstance(Square(0.0), planes);
    REQUIRE(clipped.has_value());
    CHECK(std::abs(Area(clipped->vertices, clipped->triangles) - 1.0) < kEpsilon);
    CHECK(AllInFront(*clipped, kNear));
    CHECK(AllInFront(*clipped, kLeftHalf));
    CHECK(EveryVertexIsUsed(*clipped));
    for (const Vec3 vertex : clipped->vertices) {
        CHECK(vertex.y == 0.0);  // still in its own plane
    }

    // The planes in the other order leave the same quarter.
    const std::array<Plane, 2> other_order = {kLeftHalf, kNear};
    const std::optional<Model> same = ClipInstance(Square(0.0), other_order);
    REQUIRE(same.has_value());
    CHECK(std::abs(Area(same->vertices, same->triangles) - 1.0) < kEpsilon);
    CHECK(AllInFront(*same, kNear));
    CHECK(AllInFront(*same, kLeftHalf));
}

TEST_CASE("ClipInstance: nothing is left as soon as one plane leaves nothing", "[clipping]") {
    // In front of z = 1 but entirely to the right of x = 0... moved to x from 4 to 6.
    Model right = Square(3.0);
    for (Vec3& vertex : right.vertices) {
        vertex.x += 5.0;
    }
    right.bounding_sphere = MakeBoundingSphere(right.vertices);
    const std::array<Plane, 2> planes = {kNear, kLeftHalf};
    CHECK_FALSE(ClipInstance(right, planes).has_value());
    const std::array<Plane, 2> other_order = {kLeftHalf, kNear};
    CHECK_FALSE(ClipInstance(right, other_order).has_value());
    // Each plane alone: the near plane keeps it, the other does not.
    CHECK(ClipInstance(right, std::array<Plane, 1>{kNear}).has_value());
    CHECK_FALSE(ClipInstance(right, std::array<Plane, 1>{kLeftHalf}).has_value());
}

// --- ClipScene ------------------------------------------------------------------------

TEST_CASE("ClipScene: instances are kept, clipped or removed, in order", "[clipping]") {
    const std::vector<Model> scene = {
        Square(3.0, kRed),     // in front: kept as it is
        Square(-5.0, kGreen),  // behind: removed
        Square(0.0, kBlue),    // across the plane: clipped
        Square(7.0, kWhite),   // in front: kept as it is
    };
    const std::array<Plane, 1> planes = {kNear};
    const std::vector<Model> clipped = ClipScene(scene, planes);
    REQUIRE(clipped.size() == 3);
    CHECK(clipped[0] == scene[0]);
    CHECK(clipped[1] == *ClipInstanceAgainstPlane(scene[2], kNear));
    CHECK(clipped[1].triangles.size() == 3);
    CHECK(clipped[1].triangles[0].color == kBlue);
    CHECK(clipped[2] == scene[3]);
}

TEST_CASE("ClipScene: with no planes the scene is returned as it is, and no scene gives none",
          "[clipping]") {
    const std::vector<Model> scene = {Square(3.0), Square(-5.0), Square(0.0)};
    CHECK(ClipScene(scene, {}) == scene);
    const std::array<Plane, 1> planes = {kNear};
    CHECK(ClipScene({}, planes).empty());
    CHECK(ClipScene({Square(-5.0)}, planes).empty());
}

// --- Against the clipping volume of a viewport ----------------------------------------

TEST_CASE("ClipInstance: what the planes of a viewport leave can be projected onto the canvas",
          "[clipping]") {
    // The viewport of the demo pictures, and the book's cube where the volume cuts it:
    // across the near plane, around the camera, across each side, in a corner.
    const Viewport viewport(320, 180, 16.0 / 9.0, 1.0, 1.0);
    const std::vector<Plane> planes = MakeClippingPlanes(viewport);
    const std::vector<Vec3> positions = {
        {0.0, 0.0, 1.5},   // the near plane goes through it
        {0.0, 0.0, 0.0},   // the camera is inside it
        {-4.0, 0.0, 5.0},  // across the left plane
        {4.5, 0.3, 5.0},   // across the right plane
        {0.0, 2.6, 5.0},   // across the top plane
        {0.0, -2.2, 4.0},  // across the bottom plane
        {3.1, 1.9, 3.9},   // in the top right corner
        {0.9, -0.6, 1.2},  // across three planes at once
    };
    for (const Vec3 position : positions) {
        INFO("cube at " << Catch::StringMaker<Vec3>::convert(position));
        const Model cube = TransformModel(Cube(), cgfs::MakeTranslationMatrix(position));
        const std::optional<Model> clipped = ClipInstance(cube, planes);
        REQUIRE(clipped.has_value());
        CHECK_FALSE(clipped->triangles.empty());  // part of it is in view
        CHECK_FALSE(*clipped == cube);            // and part of it is not
        CHECK(EveryVertexIsUsed(*clipped));
        for (const Vec3 vertex : clipped->vertices) {
            // In the volume, so in front of the camera: ProjectVertex's precondition.
            for (const Plane& plane : planes) {
                CHECK(SignedDistance(plane, vertex) > -1e-9);
            }
            CHECK(vertex.z > 0.999);
            // And it lands on the canvas, x from -160 to 159 and y from -90 to 89, or on
            // the column or the row just past its right or its top edge.
            const cgfs::Point point = cgfs::ProjectVertex(viewport, vertex);
            CHECK(point.x >= -160);
            CHECK(point.x <= 160);
            CHECK(point.y >= -90);
            CHECK(point.y <= 90);
        }
    }
}

TEST_CASE("ClipInstance: the planes of a viewport keep what is in view and remove what is not",
          "[clipping]") {
    const Viewport viewport(320, 180, 16.0 / 9.0, 1.0, 1.0);
    const std::vector<Plane> planes = MakeClippingPlanes(viewport);
    const auto cube_at = [](Vec3 position) {
        return TransformModel(Cube(), cgfs::MakeTranslationMatrix(position));
    };

    // Well inside the volume: returned as it is.
    const Model in_view = cube_at({0.0, 0.0, 8.0});
    const std::optional<Model> kept = ClipInstance(in_view, planes);
    REQUIRE(kept.has_value());
    CHECK(*kept == in_view);

    // Behind the camera, and beyond each side: nothing is left.
    for (const Vec3 position : {Vec3{0.0, 0.0, -10.0}, Vec3{0.0, 0.0, -1.0}, Vec3{-12.0, 0.0, 8.0},
                                Vec3{12.0, 0.0, 8.0}, Vec3{0.0, 8.0, 8.0}, Vec3{0.0, -8.0, 8.0}}) {
        INFO("cube at " << Catch::StringMaker<Vec3>::convert(position));
        CHECK_FALSE(ClipInstance(cube_at(position), planes).has_value());
    }
}

// --- Normals -----------------------------------------------------------------------------

TEST_CASE("ClipTriangle: a new corner gets its normal by the fraction of its position",
          "[clipping]") {
    // The triangle of the one-corner test, with a normal at each corner: AB crosses the
    // plane a quarter of the way along and AC half way.
    const std::vector<Vec3> vertices = {
        {0.0, 0.0, 3.0},   // A
        {6.0, 0.0, -5.0},  // B
        {0.0, 4.0, -1.0},  // C
    };
    const Triangle triangle{
        .v = {0, 1, 2},
        .color = kRed,
        .normals = {Vec3{0.0, 0.0, -1.0}, Vec3{4.0, 0.0, 0.0}, Vec3{0.0, 2.0, 0.0}}};
    const Clipped clipped = Clip(vertices, triangle, kNear);

    // A B' C': NA + 1/4 (NB - NA) and NA + 1/2 (NC - NA). Not normalized: whoever lights
    // the corner does that.
    REQUIRE(clipped.triangles.size() == 1);
    CHECK(clipped.triangles[0] ==
          Triangle{.v = {0, 3, 4},
                   .color = kRed,
                   .normals = {Vec3{0.0, 0.0, -1.0}, Vec3{1.0, 0.0, -0.75}, Vec3{0.0, 1.0, -0.5}}});
}

TEST_CASE("ClipTriangle: both pieces of a quadrilateral get the interpolated normals",
          "[clipping]") {
    // The triangle of the two-corner test: both sides to C cross half way along.
    const std::vector<Vec3> vertices = {
        {0.0, 0.0, 3.0},   // A
        {4.0, 0.0, 3.0},   // B
        {0.0, 4.0, -1.0},  // C
    };
    const Vec3 na{0.0, 0.0, -1.0};
    const Vec3 nb{1.0, 0.0, 0.0};
    const Vec3 nc{0.0, 1.0, 0.0};
    const Clipped clipped =
        Clip(vertices, {.v = {0, 1, 2}, .color = kBlue, .normals = {na, nb, nc}}, kNear);

    const Vec3 na_prime{0.0, 0.5, -0.5};  // half way from NA to NC
    const Vec3 nb_prime{0.5, 0.5, 0.0};   // half way from NB to NC
    REQUIRE(clipped.triangles.size() == 2);
    CHECK(clipped.triangles[0].normals == std::array<Vec3, 3>{na, nb, na_prime});
    CHECK(clipped.triangles[1].normals == std::array<Vec3, 3>{na_prime, nb, nb_prime});
}

TEST_CASE("ClipTriangle: a triangle without normals is cut into pieces without normals",
          "[clipping]") {
    const std::vector<Vec3> vertices = {{0.0, 0.0, 3.0}, {6.0, 0.0, -5.0}, {0.0, 4.0, -1.0}};
    for (const Corners corners : {Corners{0, 1, 2}, Corners{1, 2, 0}, Corners{1, 0, 2}}) {
        const Clipped clipped = Clip(vertices, {corners, kRed}, kNear);
        REQUIRE_FALSE(clipped.triangles.empty());
        for (const Triangle& piece : clipped.triangles) {
            CHECK(piece.normals == std::array<Vec3, 3>{});
        }
    }
}

TEST_CASE("ClipTriangle: a triangle that is kept whole keeps its normals", "[clipping]") {
    const std::vector<Vec3> vertices = {{0.0, 0.0, 3.0}, {4.0, 0.0, 3.0}, {0.0, 4.0, 2.0}};
    const Triangle triangle{
        .v = {0, 1, 2},
        .color = kRed,
        .normals = {Vec3{0.0, 0.0, -1.0}, Vec3{1.0, 0.0, 0.0}, Vec3{0.0, 1.0, 0.0}}};
    const Clipped clipped = Clip(vertices, triangle, kNear);
    REQUIRE(clipped.triangles.size() == 1);
    CHECK(clipped.triangles[0] == triangle);
}
