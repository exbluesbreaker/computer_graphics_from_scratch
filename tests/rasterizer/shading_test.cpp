#include <cgfs/camera/viewport.h>
#include <cgfs/image/canvas.h>
#include <cgfs/image/color.h>
#include <cgfs/image/depth_buffer.h>
#include <cgfs/lighting/illumination.h>
#include <cgfs/lighting/light.h>
#include <cgfs/math/vec3.h>
#include <cgfs/rasterizer/model.h>
#include <cgfs/rasterizer/object.h>
#include <cgfs/rasterizer/point.h>
#include <cgfs/rasterizer/shading.h>
#include <cgfs/rasterizer/triangle.h>

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdint>
#include <vector>

#include "rasterizer/pixel_helpers.h"
#include "support/test_helpers.h"

using cgfs::AmbientLight;
using cgfs::Canvas;
using cgfs::Color;
using cgfs::ComputeIllumination;
using cgfs::DepthBuffer;
using cgfs::DirectionalLight;
using cgfs::DrawFilledTriangle;
using cgfs::DrawPhongTriangle;
using cgfs::DrawShadedTriangle;
using cgfs::kMatte;
using cgfs::Light;
using cgfs::Model;
using cgfs::Normalized;
using cgfs::Point;
using cgfs::PointLight;
using cgfs::RenderObject;
using cgfs::Rgb8;
using cgfs::ShadingModel;
using cgfs::ToRgb8;
using cgfs::Vec3;
using cgfs::Viewport;
using cgfs_test::Bytes;
using cgfs_test::LitPixels;
using namespace cgfs::colors;

namespace {

// A 32 x 32 canvas seen through a 1 x 1 viewport at distance 1: a vertex (x, y, 1) on
// the viewport plane lands on the pixel (32 x, 32 y).
constexpr int kSize = 32;

Viewport TestViewport() { return Viewport(kSize, kSize); }

constexpr ShadingModel kEveryShadingModel[] = {ShadingModel::kFlat, ShadingModel::kGouraud,
                                               ShadingModel::kPhong};

// The direction from a surface that faces the camera back to the camera.
constexpr Vec3 kTowardCamera{0.0, 0.0, -1.0};

// One triangle on the viewport plane that faces the camera: its corners are listed
// clockwise as the camera sees them, top, bottom right, bottom left, so its own normal
// is kTowardCamera. They land on the pixels kTop, kRight and kLeft, and its center,
// (0, -1/8, 1), on the pixel (0, -4).
const std::array<Vec3, 3> kCorners = {Vec3{0.0, 0.375, 1.0}, Vec3{0.375, -0.375, 1.0},
                                      Vec3{-0.375, -0.375, 1.0}};
constexpr Vec3 kCenter{0.0, -0.125, 1.0};
constexpr Point kTop{0, 12};
constexpr Point kRight{12, -12};
constexpr Point kLeft{-12, -12};

Model Face(Color color = kWhite) {
    return {.vertices = {kCorners[0], kCorners[1], kCorners[2]}, .triangles = {{{0, 1, 2}, color}}};
}

// A fresh black canvas with the object drawn on it under the lights.
Canvas Lit(const Model& object, const std::vector<Light>& lights, ShadingModel shading_model) {
    Canvas canvas(kSize, kSize, kBlack);
    RenderObject(canvas, TestViewport(), object, lights, shading_model);
    return canvas;
}

// A fresh black canvas with the face filled in one color.
Canvas Filled(Color color) {
    Canvas canvas(kSize, kSize, kBlack);
    DrawFilledTriangle(canvas, kTop, kRight, kLeft, color);
    return canvas;
}

// The light at `point` of a surface with the unit normal `normal`, seen from the camera.
double LightAt(Vec3 point, Vec3 normal, const std::vector<Light>& lights,
               double specular = kMatte) {
    return ComputeIllumination(point, normal, -point, specular, lights);
}

// The red channel of a pixel that is on the canvas.
int Red(const Canvas& canvas, int x, int y) {
    const auto pixel = canvas.GetPixel(x, y);
    REQUIRE(pixel.has_value());
    return pixel->r;
}

// A lamp in front of the face, `distance` away from the point the pixel (4, 0) shows.
std::vector<Light> LampInFrontOfPixel4_0(double distance) {
    return {PointLight{.intensity = 1.0, .position = {0.125, 0.0, 1.0 - distance}}};
}

}  // namespace

TEST_CASE("RenderObject under lights: Phong shading unless the caller says otherwise",
          "[shading]") {
    const std::vector<Light> lights = LampInFrontOfPixel4_0(0.25);
    Canvas canvas(kSize, kSize, kBlack);
    RenderObject(canvas, TestViewport(), Face(), lights);
    CHECK(Bytes(canvas) == Bytes(Lit(Face(), lights, ShadingModel::kPhong)));
}

TEST_CASE(
    "RenderObject under lights: ambient light alone gives the filled triangle, dimmed, "
    "under every shading model",
    "[shading]") {
    const std::vector<Light> lights = {AmbientLight{.intensity = 0.5}};
    for (const ShadingModel shading_model : kEveryShadingModel) {
        CHECK(Bytes(Lit(Face(kYellow), lights, shading_model)) == Bytes(Filled(kYellow * 0.5)));
    }
}

TEST_CASE(
    "RenderObject under lights: a flat face lit from far away has one shade under every "
    "shading model",
    "[shading]") {
    // A directional light arrives at every point of a flat face at the same angle: here
    // the angle of the 3-4-5 triangle, cos = 0.8. One value per triangle is all there is
    // to compute, and the three ways of computing more agree with it.
    const std::vector<Light> lights = {
        AmbientLight{.intensity = 0.125},
        DirectionalLight{.intensity = 0.5, .direction = {0.0, 3.0, -4.0}},
    };
    const double light = LightAt(kCenter, kTowardCamera, lights);
    CHECK(std::abs(light - 0.525) < 1e-12);
    for (const ShadingModel shading_model : kEveryShadingModel) {
        CHECK(Bytes(Lit(Face(), lights, shading_model)) == Bytes(Filled(kWhite * light)));
    }
}

TEST_CASE("RenderObject under lights: flat shading computes the light once, at the center",
          "[shading]") {
    const std::vector<Light> lights = {
        AmbientLight{.intensity = 0.1},
        PointLight{.intensity = 0.8, .position = {0.125, 0.0, 0.75}},
    };
    const double light = LightAt(kCenter, kTowardCamera, lights);
    const Canvas flat = Lit(Face(kRed), lights, ShadingModel::kFlat);
    CHECK(Bytes(flat) == Bytes(Filled(kRed * light)));
    // The lamp is off the center, so this is not the light of any other pixel's point.
    CHECK(LightAt({0.125, 0.0, 1.0}, kTowardCamera, lights) > light + 0.1);
}

TEST_CASE("RenderObject under lights: flat shading uses the triangle's own normal", "[shading]") {
    // The normals a model states are those of the surface its triangles stand for; one
    // value per triangle is computed with the one normal the triangle itself has.
    const std::vector<Light> lights = LampInFrontOfPixel4_0(0.25);
    Model smooth = Face();
    smooth.triangles[0].normals = {Vec3{0.0, 1.0, -1.0}, Vec3{1.0, 0.0, -1.0},
                                   Vec3{-1.0, 0.0, -1.0}};
    CHECK(Bytes(Lit(smooth, lights, ShadingModel::kFlat)) ==
          Bytes(Lit(Face(), lights, ShadingModel::kFlat)));
    // Gouraud and Phong shading do read them.
    CHECK(Bytes(Lit(smooth, lights, ShadingModel::kGouraud)) !=
          Bytes(Lit(Face(), lights, ShadingModel::kGouraud)));
    CHECK(Bytes(Lit(smooth, lights, ShadingModel::kPhong)) !=
          Bytes(Lit(Face(), lights, ShadingModel::kPhong)));
}

TEST_CASE(
    "RenderObject under lights: Gouraud shading computes the light at the corners and "
    "interpolates it",
    "[shading]") {
    const std::vector<Light> lights = {
        AmbientLight{.intensity = 0.1},
        PointLight{.intensity = 0.8, .position = {0.125, 0.0, 0.75}},
    };
    // DrawShadedTriangle on the projected corners, each with the light of its vertex.
    Point top = kTop;
    Point right = kRight;
    Point left = kLeft;
    top.h = LightAt(kCorners[0], kTowardCamera, lights, 20.0);
    right.h = LightAt(kCorners[1], kTowardCamera, lights, 20.0);
    left.h = LightAt(kCorners[2], kTowardCamera, lights, 20.0);
    Canvas expected(kSize, kSize, kBlack);
    DrawShadedTriangle(expected, top, right, left, kGreen);

    Model face = Face(kGreen);
    face.specular = 20.0;
    CHECK(Bytes(Lit(face, lights, ShadingModel::kGouraud)) == Bytes(expected));
    // Three corners, three values.
    CHECK(top.h != right.h);
    CHECK(right.h != left.h);
}

TEST_CASE(
    "RenderObject under lights: Gouraud shading lights a corner with the normal the "
    "triangle states there",
    "[shading]") {
    // A light from where the camera is, and normals that lean away from it by the angle
    // of the 3-4-5 triangle at the two lower corners: cos = 0.8 there and 1 at the top.
    const std::vector<Light> lights = {
        DirectionalLight{.intensity = 1.0, .direction = kTowardCamera}};
    Model face = Face();
    face.triangles[0].normals = {kTowardCamera, Vec3{0.6, 0.0, -0.8}, Vec3{-0.6, 0.0, -0.8}};
    const Canvas canvas = Lit(face, lights, ShadingModel::kGouraud);
    CHECK(canvas.GetPixel(kRight.x, kRight.y) == Rgb8{204, 204, 204});  // 255 * 0.8
    CHECK(canvas.GetPixel(kLeft.x, kLeft.y) == Rgb8{204, 204, 204});
    CHECK(Red(canvas, 0, 11) > 250);  // next to the top corner
    // Without them the face is flat and fully lit all over.
    CHECK(Bytes(Lit(Face(), lights, ShadingModel::kGouraud)) == Bytes(Filled(kWhite)));

    // The light at a corner is computed with its normal's direction: three normals half
    // as long, as the clipping leaves them at a corner it makes, are the same picture.
    Model shorter = face;
    for (Vec3& normal : shorter.triangles[0].normals) {
        normal *= 0.5;
    }
    CHECK(Bytes(Lit(shorter, lights, ShadingModel::kGouraud)) == Bytes(canvas));
    CHECK(Bytes(Lit(shorter, lights, ShadingModel::kPhong)) ==
          Bytes(Lit(face, lights, ShadingModel::kPhong)));
}

TEST_CASE("RenderObject under lights: a corner without a normal has the triangle's", "[shading]") {
    // One corner states a normal and the other two do not: they are lit as the flat
    // face they are corners of.
    const std::vector<Light> lights = {
        DirectionalLight{.intensity = 1.0, .direction = kTowardCamera}};
    Model face = Face();
    face.triangles[0].normals[0] = Vec3{0.0, 0.6, -0.8};
    for (const ShadingModel shading_model : {ShadingModel::kGouraud, ShadingModel::kPhong}) {
        const Canvas canvas = Lit(face, lights, shading_model);
        CHECK(canvas.GetPixel(kRight.x, kRight.y) == Rgb8{255, 255, 255});
        CHECK(canvas.GetPixel(kLeft.x, kLeft.y) == Rgb8{255, 255, 255});
        CHECK(Red(canvas, 0, 10) < 225);  // toward the top the normal leans away
    }
}

TEST_CASE(
    "RenderObject under lights: Phong shading draws the Phong triangle with the corners' "
    "normals",
    "[shading]") {
    const std::vector<Light> lights = {
        AmbientLight{.intensity = 0.1},
        PointLight{.intensity = 0.8, .position = {0.125, 0.0, 0.75}},
    };
    Model face = Face(kBlue);
    face.specular = 20.0;
    face.triangles[0].normals = {Vec3{0.0, 0.6, -0.8}, Vec3{0.6, 0.0, -0.8}, kTowardCamera};

    // The projected corners with their depth and their normals, as the object has them.
    const auto corner = [](Point p, Vec3 normal) {
        p.inv_z = 1.0;
        p.normal = normal;
        return p;
    };
    Canvas expected(kSize, kSize, kBlack);
    DrawPhongTriangle(expected, TestViewport(), corner(kTop, {0.0, 0.6, -0.8}),
                      corner(kRight, {0.6, 0.0, -0.8}), corner(kLeft, kTowardCamera), kBlue, 20.0,
                      lights);
    CHECK(Bytes(Lit(face, lights, ShadingModel::kPhong)) == Bytes(expected));
    CHECK(LitPixels(expected).size() > 200);
}

TEST_CASE("RenderObject under lights: a lamp close to a face is seen by Phong shading alone",
          "[shading]") {
    // The book's Figures 13-7 and 13-10: a lamp in front of the point the pixel (4, 0)
    // shows, a quarter of a unit away, then half of that, then half again.
    int flat_before = 256;
    int gouraud_before = 256;
    for (const double distance : {0.25, 0.125, 0.0625}) {
        INFO("the lamp is " << distance << " from the face");
        const std::vector<Light> lights = LampInFrontOfPixel4_0(distance);

        // Lit pixel by pixel, the point in front of the lamp gets its light along the
        // normal, in full, however close the lamp is.
        CHECK(Lit(Face(), lights, ShadingModel::kPhong).GetPixel(4, 0) == Rgb8{255, 255, 255});

        // The corners and the center are to the side of the lamp, where its light
        // arrives at a slant, and the more so the closer it comes: the face gets darker
        // as the lamp approaches it, and nowhere is it bright.
        const int flat = Red(Lit(Face(), lights, ShadingModel::kFlat), 4, 0);
        const int gouraud = Red(Lit(Face(), lights, ShadingModel::kGouraud), 4, 0);
        CHECK(flat < flat_before);
        CHECK(gouraud < gouraud_before);
        CHECK(gouraud < 135);
        flat_before = flat;
        gouraud_before = gouraud;
    }
    CHECK(flat_before < 100);  // cos = 1/3 at the center with the lamp at 1/16
    CHECK(gouraud_before < 50);
}

TEST_CASE(
    "RenderObject under lights: a highlight inside a triangle is seen by Phong shading "
    "alone",
    "[shading]") {
    // A lamp where the camera is. On the camera's axis its light is mirrored straight
    // back: a highlight in the middle of the face, far from every corner.
    const std::vector<Light> lights = {PointLight{.intensity = 0.4, .position = {0.0, 0.0, 0.0}}};
    const Model matte = Face();
    Model shiny = Face();
    shiny.specular = 50.0;

    CHECK(Lit(matte, lights, ShadingModel::kPhong).GetPixel(0, 0) == Rgb8{102, 102, 102});
    CHECK(Lit(shiny, lights, ShadingModel::kPhong).GetPixel(0, 0) == Rgb8{204, 204, 204});

    // At the corners the mirror direction misses the camera by far, the light computed
    // there has nothing of the highlight in it, and neither has anything in between.
    const Canvas gouraud_matte = Lit(matte, lights, ShadingModel::kGouraud);
    const Canvas gouraud_shiny = Lit(shiny, lights, ShadingModel::kGouraud);
    CHECK(Red(gouraud_shiny, 0, 0) - Red(gouraud_matte, 0, 0) <= 1);
    CHECK(Red(gouraud_shiny, 0, 0) < 102);

    // Flat shading catches a part of it at the center, 4 pixels below the axis, and
    // spreads that over the whole triangle.
    const Canvas flat_shiny = Lit(shiny, lights, ShadingModel::kFlat);
    CHECK(Red(flat_shiny, 0, 0) > Red(Lit(matte, lights, ShadingModel::kFlat), 0, 0) + 15);
    CHECK(flat_shiny.GetPixel(0, 0) == flat_shiny.GetPixel(kRight.x, kRight.y));
}

TEST_CASE(
    "RenderObject under lights: the normals of a curved surface hide the fold between "
    "two triangles",
    "[shading]") {
    // Two triangles that share the vertical side from (0, 8) to (0, -8) and are folded
    // along it: their third corners, to the left and to the right, are an eighth of a
    // unit farther away. A light from the left front: the left triangle faces it almost
    // squarely, cos = 0.95, and the right one at a slant, cos = 0.32.
    const std::vector<Light> lights = {
        DirectionalLight{.intensity = 1.0, .direction = {-1.0, 0.0, -1.0}}};
    // The two triangles' own normals, at unit length, as an object has its normals.
    const Vec3 left_normal = Normalized({-0.5, 0.0, -1.0});
    const Vec3 right_normal = Normalized({0.5, 0.0, -1.0});
    Model folded{
        .vertices = {{0.0, 0.25, 1.0}, {0.0, -0.25, 1.0}, {-0.25, 0.0, 1.125}, {0.25, 0.0, 1.125}},
        .triangles = {{{0, 1, 2}, kWhite}, {{0, 3, 1}, kWhite}},
    };

    // With nothing but the triangles to go by there are two faces and an edge.
    const Canvas flat = Lit(folded, lights, ShadingModel::kFlat);
    CHECK(Red(flat, -2, 0) == 241);  // 255 * 0.9487
    CHECK(Red(flat, 2, 0) == 80);    // 255 * 0.3162
    CHECK(Red(flat, -2, 0) - Red(flat, 2, 0) > 150);

    // The model says that the two stand for one curved surface: along the fold it faces
    // the camera, and at the outer corners the way the triangles do.
    folded.triangles[0].normals = {kTowardCamera, kTowardCamera, left_normal};
    folded.triangles[1].normals = {kTowardCamera, right_normal, kTowardCamera};
    for (const ShadingModel shading_model : {ShadingModel::kGouraud, ShadingModel::kPhong}) {
        const Canvas smooth = Lit(folded, lights, shading_model);
        // On the fold the light is that of its normal, cos = 0.7071, from either side,
        // and it changes little by little across it.
        CHECK(smooth.GetPixel(0, 0) == Rgb8{180, 180, 180});
        CHECK(Red(smooth, -2, 0) > 180);
        CHECK(Red(smooth, 2, 0) < 180);
        CHECK(Red(smooth, -2, 0) - Red(smooth, 2, 0) < 50);
    }
    // Flat shading is the same with the normals as without.
    CHECK(Bytes(Lit(folded, lights, ShadingModel::kFlat)) == Bytes(flat));
}

TEST_CASE("RenderObject under lights: the intensities stored in the triangles are not read",
          "[shading]") {
    const std::vector<Light> lights = LampInFrontOfPixel4_0(0.25);
    Model dark = Face();
    dark.triangles[0].h = {0.0, 0.0, 0.0};
    for (const ShadingModel shading_model : kEveryShadingModel) {
        CHECK(Bytes(Lit(dark, lights, shading_model)) == Bytes(Lit(Face(), lights, shading_model)));
    }
}

TEST_CASE(
    "RenderObject under lights: the order of a triangle's corners says which side the "
    "light is on",
    "[shading]") {
    // The same three corners the other way round: the triangle's normal points away from
    // the camera, and a light from the camera's side does not reach that side of it.
    // Only the ambient light is left. (RenderScene would not draw the triangle at all.)
    const std::vector<Light> lights = {
        AmbientLight{.intensity = 0.25},
        DirectionalLight{.intensity = 0.5, .direction = kTowardCamera},
    };
    Model reversed = Face();
    reversed.triangles[0].v = {0, 2, 1};
    for (const ShadingModel shading_model : kEveryShadingModel) {
        CHECK(Bytes(Lit(Face(), lights, shading_model)) == Bytes(Filled(kWhite * 0.75)));
        CHECK(Bytes(Lit(reversed, lights, shading_model)) == Bytes(Filled(kWhite * 0.25)));
    }
}

TEST_CASE("RenderObject under lights: a triangle without area is not drawn", "[shading]") {
    // Three corners on one line have no normal to light them by.
    const std::vector<Light> lights = {AmbientLight{.intensity = 1.0}};
    const Model line{
        .vertices = {{-0.25, -0.25, 1.0}, {0.0, 0.0, 1.0}, {0.25, 0.25, 1.0}},
        .triangles = {{{0, 1, 2}, kWhite}, {{0, 0, 1}, kWhite}},
    };
    for (const ShadingModel shading_model : kEveryShadingModel) {
        CHECK(LitPixels(Lit(line, lights, shading_model)).empty());
    }
    // An object without triangles, or without anything, draws nothing either.
    CHECK(LitPixels(Lit(Model{}, lights, ShadingModel::kPhong)).empty());
}

TEST_CASE("RenderObject under lights: every triangle is lit in its own color", "[shading]") {
    // A red triangle on the left and a blue one on the right, under the same light.
    const std::vector<Light> lights = {AmbientLight{.intensity = 0.5}};
    const Model two{
        .vertices = {{-0.25, 0.25, 1.0},
                     {0.0, -0.25, 1.0},
                     {-0.5, -0.25, 1.0},
                     {0.25, 0.25, 1.0},
                     {0.5, -0.25, 1.0}},
        .triangles = {{{0, 1, 2}, kRed}, {{3, 4, 1}, kBlue}},
    };
    for (const ShadingModel shading_model : kEveryShadingModel) {
        const Canvas canvas = Lit(two, lights, shading_model);
        CHECK(canvas.GetPixel(-8, 0) == Rgb8{127, 0, 0});
        CHECK(canvas.GetPixel(8, 0) == Rgb8{0, 0, 127});
    }
}

TEST_CASE("RenderObject under lights: through a depth buffer the nearer triangle shows",
          "[shading]") {
    // The face on the viewport plane in red and, listed after it, a blue one twice as
    // far and twice as large, which covers the same pixels.
    const std::vector<Light> lights = {AmbientLight{.intensity = 1.0}};
    const Model two{
        .vertices = {kCorners[0], kCorners[1], kCorners[2], 2.0 * kCorners[0], 2.0 * kCorners[1],
                     2.0 * kCorners[2]},
        .triangles = {{{0, 1, 2}, kRed}, {{3, 4, 5}, kBlue}},
    };
    for (const ShadingModel shading_model : kEveryShadingModel) {
        // Painted in list order the far one is on top.
        CHECK(Lit(two, lights, shading_model).GetPixel(0, 0) == Rgb8{0, 0, 255});

        Canvas canvas(kSize, kSize, kBlack);
        DepthBuffer depth_buffer(canvas);
        RenderObject(canvas, TestViewport(), two, lights, shading_model, &depth_buffer);
        CHECK(Bytes(canvas) == Bytes(Filled(kRed)));
        CHECK(depth_buffer.InvZ(0, 0) == 1.0);
    }
}
