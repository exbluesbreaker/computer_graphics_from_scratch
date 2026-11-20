#include <cgfs/image/color.h>
#include <cgfs/math/bounding_sphere.h>
#include <cgfs/math/mat4.h>
#include <cgfs/math/vec3.h>
#include <cgfs/rasterizer/culling.h>
#include <cgfs/rasterizer/model.h>
#include <cgfs/rasterizer/object.h>

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <initializer_list>
#include <vector>

#include "support/test_helpers.h"

using cgfs::CullBackFaces;
using cgfs::IsBackFacing;
using cgfs::MakeBoundingSphere;
using cgfs::Model;
using cgfs::TransformModel;
using cgfs::Triangle;
using cgfs::Vec3;
using namespace cgfs::colors;

namespace {

// The camera of the camera's own coordinates: at the origin, looking along +Z, with x to
// its right and y up.
constexpr Vec3 kOrigin{0.0, 0.0, 0.0};

// A triangle in the plane z = 4, in front of that camera: its corners at the top, at the
// bottom right and at the bottom left, which is clockwise as the camera sees them.
constexpr Vec3 kTop{0.0, 1.0, 4.0};
constexpr Vec3 kRight{1.0, -1.0, 4.0};
constexpr Vec3 kLeft{-1.0, -1.0, 4.0};

// The book's cube, corners at x, y, z = ±1 and two triangles per face, each listing its
// corners clockwise as seen from outside: the faces at z = 1, x = 1, z = -1, x = -1,
// y = 1 and y = -1, in that order. Every corner has an intensity of its own, so that a
// test can tell whether a triangle came through untouched.
Model Cube() {
    Model cube;
    cube.vertices = {{1.0, 1.0, 1.0},  {-1.0, 1.0, 1.0},  {-1.0, -1.0, 1.0},  {1.0, -1.0, 1.0},
                     {1.0, 1.0, -1.0}, {-1.0, 1.0, -1.0}, {-1.0, -1.0, -1.0}, {1.0, -1.0, -1.0}};
    cube.triangles = {
        {{0, 1, 2}, kRed, {0.1, 0.2, 0.3}},    {{0, 2, 3}, kRed, {0.1, 0.3, 0.4}},     // z = 1
        {{4, 0, 3}, kGreen, {0.5, 0.1, 0.4}},  {{4, 3, 7}, kGreen, {0.5, 0.4, 0.8}},   // x = 1
        {{5, 4, 7}, kBlue, {0.6, 0.5, 0.8}},   {{5, 7, 6}, kBlue, {0.6, 0.8, 0.7}},    // z = -1
        {{1, 5, 6}, kYellow, {0.2, 0.6, 0.7}}, {{1, 6, 2}, kYellow, {0.2, 0.7, 0.3}},  // x = -1
        {{4, 5, 1}, kWhite, {0.5, 0.6, 0.2}},  {{4, 1, 0}, kWhite, {0.5, 0.2, 0.1}},   // y = 1
        {{2, 6, 7}, kBlack, {0.3, 0.7, 0.8}},  {{2, 7, 3}, kBlack, {0.3, 0.8, 0.4}},   // y = -1
    };
    cube.bounding_sphere = MakeBoundingSphere(cube.vertices);
    return cube;
}

// The triangles of `model` at the given places of its list.
std::vector<Triangle> Picked(const Model& model, std::initializer_list<std::size_t> places) {
    std::vector<Triangle> triangles;
    for (const std::size_t place : places) {
        triangles.push_back(model.triangles[place]);
    }
    return triangles;
}

}  // namespace

// --- IsBackFacing ----------------------------------------------------------------------

TEST_CASE("IsBackFacing: corners listed clockwise as the camera sees them are a front face",
          "[culling]") {
    CHECK_FALSE(IsBackFacing(kTop, kRight, kLeft, kOrigin));
    // The same three corners the other way round are the triangle seen from behind.
    CHECK(IsBackFacing(kTop, kLeft, kRight, kOrigin));
}

TEST_CASE("IsBackFacing: it is the order around the triangle that counts, not where it starts",
          "[culling]") {
    CHECK_FALSE(IsBackFacing(kRight, kLeft, kTop, kOrigin));
    CHECK_FALSE(IsBackFacing(kLeft, kTop, kRight, kOrigin));
    CHECK(IsBackFacing(kLeft, kRight, kTop, kOrigin));
    CHECK(IsBackFacing(kRight, kTop, kLeft, kOrigin));
}

TEST_CASE(
    "IsBackFacing: the same triangle is a front face from one side and a back face "
    "from the other",
    "[culling]") {
    // The camera steps through the triangle's plane, z = 4, and looks at it from behind.
    CHECK_FALSE(IsBackFacing(kTop, kRight, kLeft, {0.0, 0.0, 3.0}));
    CHECK(IsBackFacing(kTop, kRight, kLeft, {0.0, 0.0, 5.0}));
    CHECK(IsBackFacing(kTop, kRight, kLeft, {7.0, -3.0, 8.0}));
    // Anywhere on the near side of the plane it is a front face, however far off to the
    // side the camera stands.
    CHECK_FALSE(IsBackFacing(kTop, kRight, kLeft, {100.0, -50.0, 3.75}));
}

TEST_CASE("IsBackFacing: a triangle seen edge-on is a back face", "[culling]") {
    // The camera in the triangle's own plane: <N, V> = 0, and the book counts that as
    // turned away. Both ways round, since neither side is seen.
    const Vec3 in_the_plane{5.0, 2.0, 4.0};
    CHECK(IsBackFacing(kTop, kRight, kLeft, in_the_plane));
    CHECK(IsBackFacing(kTop, kLeft, kRight, in_the_plane));
    // A camera at one of its corners is in its plane too.
    CHECK(IsBackFacing(kTop, kRight, kLeft, kTop));
}

TEST_CASE("IsBackFacing: a triangle without area is a back face", "[culling]") {
    // Three corners on one line, and three in one place: the normal is the zero vector.
    CHECK(IsBackFacing({0.0, 0.0, 4.0}, {1.0, 0.0, 4.0}, {2.0, 0.0, 4.0}, kOrigin));
    CHECK(IsBackFacing(kTop, kTop, kTop, kOrigin));
}

TEST_CASE("IsBackFacing: the direction to the camera decides, not the direction it looks in",
          "[culling]") {
    // A wall to the camera's right, at x = 2 where it is nearest, that faces left and a
    // little away: its normal (-2, 0, 1) has a positive z, the same sign as the camera's
    // line of sight, and still the camera sees the wall's front, from the side.
    const Vec3 a{2.0, 0.0, 2.0};
    const Vec3 b{3.0, 0.0, 4.0};
    const Vec3 c{2.0, 1.0, 2.0};
    CHECK(cgfs::Cross(b - a, c - a) == Vec3{-2.0, 0.0, 1.0});
    CHECK_FALSE(IsBackFacing(a, b, c, kOrigin));

    // And the other way about: this wall's normal (2, 0, -1) points back toward the
    // plane the camera stands in, yet from the origin it is the wall's back that shows.
    CHECK(cgfs::Cross(c - a, b - a) == Vec3{2.0, 0.0, -1.0});
    CHECK(IsBackFacing(a, c, b, kOrigin));
}

TEST_CASE("IsBackFacing: any corner may come first when the camera is off to the side",
          "[culling]") {
    // V is taken from the first corner, and every corner gives the same answer, because
    // every point of the triangle's plane does.
    const Vec3 a{2.0, 0.0, 2.0};
    const Vec3 b{3.0, 0.0, 4.0};
    const Vec3 c{2.0, 1.0, 2.0};
    for (const Vec3 camera : {kOrigin, Vec3{0.0, 5.0, -3.0}, Vec3{4.0, 0.0, 1.0}}) {
        CAPTURE(camera);
        const bool back = IsBackFacing(a, b, c, camera);
        CHECK(IsBackFacing(b, c, a, camera) == back);
        CHECK(IsBackFacing(c, a, b, camera) == back);
        CHECK(IsBackFacing(a, c, b, camera) != back);
    }
}

TEST_CASE("IsBackFacing: the coordinates do not matter as long as all four points share them",
          "[culling]") {
    // The triangle and the camera moved together.
    const Vec3 offset{-3.0, 8.0, 2.5};
    CHECK_FALSE(IsBackFacing(kTop + offset, kRight + offset, kLeft + offset, kOrigin + offset));
    CHECK(IsBackFacing(kTop + offset, kLeft + offset, kRight + offset, kOrigin + offset));
}

// --- CullBackFaces ---------------------------------------------------------------------

TEST_CASE("CullBackFaces: of a cube straight ahead only the face toward the camera is left",
          "[culling]") {
    // The cube 5 ahead of a camera at the origin: the face at z = -1 of the cube's own
    // coordinates is the nearest, and the four sides are turned away from a camera on
    // the axis, which sees them from inside the slab between their planes.
    const Model cube = TransformModel(Cube(), cgfs::MakeTranslationMatrix({0.0, 0.0, 5.0}));
    const Model culled = CullBackFaces(cube, kOrigin);
    CHECK(culled.triangles == Picked(cube, {4, 5}));
}

TEST_CASE("CullBackFaces: a cube seen from a corner keeps three faces, in their order",
          "[culling]") {
    const Model cube = Cube();
    // From beyond the corner (1, 1, -1): the faces x = 1, z = -1 and y = 1.
    CHECK(CullBackFaces(cube, {5.0, 4.0, -3.0}).triangles == Picked(cube, {2, 3, 4, 5, 8, 9}));
    // From beyond the opposite corner: the other three.
    CHECK(CullBackFaces(cube, {-5.0, -4.0, 3.0}).triangles == Picked(cube, {0, 1, 6, 7, 10, 11}));
    // From beside an edge, level with the cube's middle: the two faces that meet there.
    CHECK(CullBackFaces(cube, {5.0, 0.0, -3.0}).triangles == Picked(cube, {2, 3, 4, 5}));
}

TEST_CASE("CullBackFaces: a camera in the plane of a face does not see that face", "[culling]") {
    // Level with the top of the cube, y = 1, and in front of it: the top is edge-on.
    const Model cube = Cube();
    CHECK(CullBackFaces(cube, {0.0, 1.0, -6.0}).triangles == Picked(cube, {4, 5}));
}

TEST_CASE("CullBackFaces: from inside an object nothing is left", "[culling]") {
    const Model cube = Cube();
    CHECK(CullBackFaces(cube, kOrigin).triangles.empty());
    CHECK(CullBackFaces(cube, {0.5, -0.25, 0.75}).triangles.empty());
}

TEST_CASE("CullBackFaces: only the triangle list changes", "[culling]") {
    const Model cube = TransformModel(Cube(), cgfs::MakeTranslationMatrix({0.0, 0.0, 5.0}));
    const Model culled = CullBackFaces(cube, kOrigin);
    // All eight vertices are still there, the four of the far face that no triangle
    // names any more among them, and the sphere is the one around all of them.
    CHECK(culled.vertices == cube.vertices);
    CHECK(culled.bounding_sphere == cube.bounding_sphere);
    // The triangles that are left are the cube's own: indices, color and intensities.
    REQUIRE(culled.triangles.size() == 2);
    CHECK(culled.triangles[0] == Triangle{{5, 4, 7}, kBlue, {0.6, 0.5, 0.8}});
    CHECK(culled.triangles[1] == Triangle{{5, 7, 6}, kBlue, {0.6, 0.8, 0.7}});
}

TEST_CASE("CullBackFaces: an object with nothing turned away is returned as it is", "[culling]") {
    // Two triangles that both face the origin.
    Model object;
    object.vertices = {kTop, kRight, kLeft, {0.0, -3.0, 4.0}};
    object.triangles = {{{0, 1, 2}, kRed}, {{2, 1, 3}, kGreen}};
    CHECK(CullBackFaces(object, kOrigin) == object);

    // And one without triangles, or without anything.
    Model points_only;
    points_only.vertices = {kTop, kRight, kLeft};
    CHECK(CullBackFaces(points_only, kOrigin) == points_only);
    CHECK(CullBackFaces(Model{}, kOrigin) == Model{});
}

TEST_CASE("CullBackFaces: a cube keeps the faces whose planes the camera is beyond", "[culling]") {
    // A face of the cube is in sight exactly when the camera is beyond that face's
    // plane. Cameras all around the cube, off its planes, and one inside it, which is
    // beyond none.
    const Model cube = Cube();
    for (const double x : {-4.0, 0.5, 3.0}) {
        for (const double y : {-2.5, 0.25, 6.0}) {
            for (const double z : {-7.0, -0.5, 1.5}) {
                const Vec3 camera{x, y, z};
                CAPTURE(camera);
                std::vector<Triangle> in_sight;
                const std::array<bool, 6> beyond = {z > 1.0,  x > 1.0, z < -1.0,
                                                    x < -1.0, y > 1.0, y < -1.0};
                for (std::size_t face = 0; face < beyond.size(); ++face) {
                    if (beyond[face]) {
                        in_sight.push_back(cube.triangles[2 * face]);
                        in_sight.push_back(cube.triangles[2 * face + 1]);
                    }
                }
                CHECK(CullBackFaces(cube, camera).triangles == in_sight);
            }
        }
    }
}

TEST_CASE("CullBackFaces: turning and scaling an object keeps its faces the right way round",
          "[culling]") {
    // The cube shrunk, turned by an angle that is not a quarter turn and moved in front
    // of the camera, to its right and below it, the way RenderScene places an instance.
    // The camera sees its front, its left side and its top.
    const Vec3 position{1.5, -1.5, 6.0};
    const Model placed = TransformModel(Cube(), cgfs::MakeTranslationMatrix(position) *
                                                    cgfs::MakeOYRotationMatrix(35.0) *
                                                    cgfs::MakeScalingMatrix(0.75));
    const Model culled = CullBackFaces(placed, kOrigin);
    CHECK(culled.triangles == Picked(placed, {4, 5, 6, 7, 8, 9}));

    // It is the same question asked in the cube's own coordinates: the transform undone
    // takes the camera to where it stands relative to the cube as it was made.
    const Vec3 camera_in_the_cube = cgfs::MakeScalingMatrix(1.0 / 0.75) *
                                    cgfs::MakeOYRotationMatrix(-35.0) *
                                    cgfs::MakeTranslationMatrix(-position) * kOrigin;
    CHECK(culled.triangles == CullBackFaces(Cube(), camera_in_the_cube).triangles);
}
