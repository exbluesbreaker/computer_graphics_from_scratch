#include <cgfs/camera/viewport.h>
#include <cgfs/image/canvas.h>
#include <cgfs/image/color.h>
#include <cgfs/image/depth_buffer.h>
#include <cgfs/image/image_io.h>
#include <cgfs/image/texture.h>
#include <cgfs/lighting/illumination.h>
#include <cgfs/lighting/light.h>
#include <cgfs/math/vec3.h>
#include <cgfs/rasterizer/model.h>
#include <cgfs/rasterizer/object.h>
#include <cgfs/rasterizer/point.h>
#include <cgfs/rasterizer/projection.h>
#include <cgfs/rasterizer/shading.h>
#include <cgfs/rasterizer/triangle.h>

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdint>
#include <vector>

#include "rasterizer/pixel_helpers.h"
#include "support/address_texture.h"
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
using cgfs::DrawTexturedTriangle;
using cgfs::Image;
using cgfs::kMatte;
using cgfs::Light;
using cgfs::Model;
using cgfs::Normalized;
using cgfs::Point;
using cgfs::PointLight;
using cgfs::RenderObject;
using cgfs::Rgb8;
using cgfs::ShadingModel;
using cgfs::Texture;
using cgfs::TextureFilter;
using cgfs::TextureMapping;
using cgfs::ToRgb8;
using cgfs::Uv;
using cgfs::Vec3;
using cgfs::Viewport;
using cgfs_test::AddressTexture;
using cgfs_test::Bytes;
using cgfs_test::LitPixels;
using namespace cgfs::colors;

namespace {

// A 32 x 32 canvas seen through a 1 x 1 viewport at distance 1: a vertex (x, y, 1) on
// the viewport plane lands on the pixel (32 x, 32 y).
constexpr int kSize = 32;

Viewport TestViewport() { return Viewport(kSize, kSize); }

constexpr ShadingModel kEveryShadingModel[] = {ShadingModel::kFlat, ShadingModel::kGouraudFaceted,
                                               ShadingModel::kGouraud, ShadingModel::kPhong};

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
    "RenderObject under lights: faceted Gouraud shading lights every corner with the "
    "triangle's own normal",
    "[shading]") {
    const std::vector<Light> lights = {
        AmbientLight{.intensity = 0.1},
        PointLight{.intensity = 0.8, .position = {0.125, 0.0, 0.75}},
    };
    // The face with the normals of a curved surface at its corners, and without them.
    Model plain = Face(kGreen);
    plain.specular = 20.0;
    Model smooth = plain;
    smooth.triangles[0].normals = {Vec3{0.0, 0.6, -0.8}, Vec3{0.6, 0.0, -0.8},
                                   Vec3{-0.6, 0.0, -0.8}};

    // DrawShadedTriangle on the projected corners, each with the light of its vertex
    // under the triangle's own normal, whatever normal the triangle states there.
    Point top = kTop;
    Point right = kRight;
    Point left = kLeft;
    top.h = LightAt(kCorners[0], kTowardCamera, lights, 20.0);
    right.h = LightAt(kCorners[1], kTowardCamera, lights, 20.0);
    left.h = LightAt(kCorners[2], kTowardCamera, lights, 20.0);
    Canvas expected(kSize, kSize, kBlack);
    DrawShadedTriangle(expected, top, right, left, kGreen);
    const Canvas faceted = Lit(smooth, lights, ShadingModel::kGouraudFaceted);
    CHECK(Bytes(faceted) == Bytes(expected));

    // That is what Gouraud shading draws of the model without its normals, so for a
    // model that states none the two are one picture, and for one that states them two.
    CHECK(Bytes(faceted) == Bytes(Lit(plain, lights, ShadingModel::kGouraud)));
    CHECK(Bytes(Lit(plain, lights, ShadingModel::kGouraudFaceted)) == Bytes(faceted));
    CHECK(Bytes(faceted) != Bytes(Lit(smooth, lights, ShadingModel::kGouraud)));
    // Three corners, three values: it is not flat shading either.
    CHECK(Bytes(faceted) != Bytes(Lit(smooth, lights, ShadingModel::kFlat)));
}

TEST_CASE(
    "RenderObject under lights: faceted Gouraud shading closes the seam flat shading "
    "leaves across a face",
    "[shading]") {
    // A square face of two triangles that share the diagonal from the pixel (-8, 8) to
    // (8, -8), and a lamp a quarter of a unit in front of its upper right half.
    const std::vector<Light> lights = {
        PointLight{.intensity = 1.0, .position = {0.125, 0.125, 0.75}}};
    const Model square{
        .vertices = {{-0.25, 0.25, 1.0},
                     {0.25, 0.25, 1.0},
                     {0.25, -0.25, 1.0},
                     {-0.25, -0.25, 1.0}},
        .triangles = {{{0, 1, 2}, kWhite}, {{0, 2, 3}, kWhite}},
    };

    // Flat shading gives each triangle the light of its own center, 0.973 and 0.647 of
    // the lamp: a step across the diagonal.
    const Canvas flat = Lit(square, lights, ShadingModel::kFlat);
    CHECK(Red(flat, 1, 1) == 248);
    CHECK(Red(flat, -1, -1) == 164);

    // Lit at their corners, the two triangles have the same normal and so the same light
    // at the two corners they share, 0.53 at both ends of the diagonal: the light runs
    // on across it.
    const Canvas faceted = Lit(square, lights, ShadingModel::kGouraudFaceted);
    CHECK(Red(faceted, 0, 0) == 136);
    CHECK(Red(faceted, 1, 1) > 136);
    CHECK(Red(faceted, -1, -1) < 136);
    CHECK(Red(faceted, 1, 1) - Red(faceted, -1, -1) < 25);
    // The square states no normals, so this is Gouraud shading's picture of it too.
    CHECK(Bytes(faceted) == Bytes(Lit(square, lights, ShadingModel::kGouraud)));
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
    // Flat shading is the same with the normals as without, and so is faceted Gouraud
    // shading, which does not read them: every corner of a triangle gets the light of
    // the triangle's own normal, under a sun the same at all three, and the fold shows
    // as it does under flat shading.
    CHECK(Bytes(Lit(folded, lights, ShadingModel::kFlat)) == Bytes(flat));
    CHECK(Bytes(Lit(folded, lights, ShadingModel::kGouraudFaceted)) == Bytes(flat));
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

// --- Painted with a texture --------------------------------------------------------------

namespace {

// Which point of the texture each corner of the face shows: the top the middle of the
// texture's top edge, the two below its bottom-right and bottom-left corners.
constexpr std::array<Uv, 3> kFaceUvs = {Uv{0.5, 0.0}, Uv{1.0, 1.0}, Uv{0.0, 1.0}};

// The face painted with `texture`. Its color is not what is seen.
Model TexturedFace(const Texture& texture, Color color = kWhite) {
    Model face = Face(color);
    face.triangles[0].uvs = kFaceUvs;
    face.texture = &texture;
    return face;
}

// The canvas points of a face's corners, as RenderObject hands them to a primitive: each
// where its vertex is projected to, at its depth, showing its point of the texture, with
// the intensity `h[k]`.
std::array<Point, 3> TexturedCorners(const std::array<Vec3, 3>& vertices,
                                     std::array<double, 3> h = {1.0, 1.0, 1.0}) {
    std::array<Point, 3> points;
    for (std::size_t k = 0; k < 3; ++k) {
        points[k] = cgfs::ProjectVertex(TestViewport(), vertices[k]);
        points[k].uv = kFaceUvs[k];
        points[k].h = h[k];
    }
    return points;
}

// A fresh black canvas with a textured triangle drawn through those points.
Canvas Textured(const std::array<Point, 3>& p, const Texture& texture,
                TextureMapping texture_mapping = TextureMapping::kPerspectiveCorrect) {
    Canvas canvas(kSize, kSize, kBlack);
    DrawTexturedTriangle(canvas, p[0], p[1], p[2], texture, texture_mapping);
    return canvas;
}

// The face leaning away from the camera: the same three pixels, the top corner at z = 1,
// the right one at z = 2 and the left one at z = 4.
const std::array<Vec3, 3> kLeaningCorners = {Vec3{0.0, 0.375, 1.0}, Vec3{0.75, -0.75, 2.0},
                                             Vec3{-1.5, -1.5, 4.0}};

Model LeaningTexturedFace(const Texture& texture) {
    Model face = TexturedFace(texture);
    face.vertices = {kLeaningCorners[0], kLeaningCorners[1], kLeaningCorners[2]};
    return face;
}

}  // namespace

TEST_CASE("RenderObject under lights: an object with a texture is painted with it, lit",
          "[shading]") {
    // Ambient light is the same everywhere, so every shading model draws the texture at
    // half its brightness: the textured triangle through the projected corners, each
    // with the intensity 0.5 and its texture coordinates.
    const Texture texture = AddressTexture(16, 16);
    const std::vector<Light> lights = {AmbientLight{.intensity = 0.5}};
    const Canvas expected = Textured(TexturedCorners(kCorners, {0.5, 0.5, 0.5}), texture);
    for (const ShadingModel shading_model : kEveryShadingModel) {
        INFO("shading model " << static_cast<int>(shading_model));
        CHECK(Bytes(Lit(TexturedFace(texture), lights, shading_model)) == Bytes(expected));
    }
    CHECK(LitPixels(expected).size() > 200);
    // Two pixels right of the face's center, two thirds of the way down from its top:
    // u = 7/12 and v = 2/3, the texel in column 9 of row 10, halved.
    CHECK(expected.GetPixel(2, -4) == Rgb8{4, 5, 100});
}

TEST_CASE(
    "RenderObject under lights: the light multiplies the texels, computed as each "
    "shading model computes it",
    "[shading]") {
    const Texture texture = AddressTexture(16, 16);
    const std::vector<Light> lights = LampInFrontOfPixel4_0(0.25);
    const Model face = TexturedFace(texture);

    // Flat shading: one light value, the center's, at all three corners.
    const double at_center = LightAt(kCenter, kTowardCamera, lights);
    CHECK(Bytes(Lit(face, lights, ShadingModel::kFlat)) ==
          Bytes(Textured(TexturedCorners(kCorners, {at_center, at_center, at_center}), texture)));

    // Gouraud shading, in both its forms for a face that states no normals: the light at
    // each corner, interpolated over the texels.
    const std::array<double, 3> at_corners = {LightAt(kCorners[0], kTowardCamera, lights),
                                              LightAt(kCorners[1], kTowardCamera, lights),
                                              LightAt(kCorners[2], kTowardCamera, lights)};
    const Canvas gouraud = Textured(TexturedCorners(kCorners, at_corners), texture);
    CHECK(Bytes(Lit(face, lights, ShadingModel::kGouraud)) == Bytes(gouraud));
    CHECK(Bytes(Lit(face, lights, ShadingModel::kGouraudFaceted)) == Bytes(gouraud));

    // Phong shading: the Phong triangle with the texture, which lights every texel it
    // draws by itself.
    std::array<Point, 3> p = TexturedCorners(kCorners);
    for (Point& point : p) {
        point.normal = kTowardCamera;
    }
    Canvas phong(kSize, kSize, kBlack);
    DrawPhongTriangle(phong, TestViewport(), p[0], p[1], p[2], kWhite, kMatte, lights, nullptr,
                      &texture);
    CHECK(Bytes(Lit(face, lights, ShadingModel::kPhong)) == Bytes(phong));

    // Three different pictures of the same texels.
    CHECK(Bytes(Lit(face, lights, ShadingModel::kFlat)) != Bytes(gouraud));
    CHECK(Bytes(phong) != Bytes(gouraud));
}

TEST_CASE("RenderObject under lights: a texture of one color is an object of that color",
          "[shading]") {
    // Whatever a shading model does to a color it does to a texel.
    constexpr Color kOrange{255.0, 128.0, 0.0};
    const Texture plain(Image{.width = 1, .height = 1, .rgb = {255, 128, 0}});
    Model painted = TexturedFace(plain);
    painted.specular = 20.0;
    Model colored = Face(kOrange);
    colored.specular = 20.0;
    const std::vector<Light> lights = {
        AmbientLight{.intensity = 0.1},
        PointLight{.intensity = 0.8, .position = {0.125, 0.0, 0.75}}};
    for (const ShadingModel shading_model : kEveryShadingModel) {
        INFO("shading model " << static_cast<int>(shading_model));
        CHECK(Bytes(Lit(painted, lights, shading_model)) ==
              Bytes(Lit(colored, lights, shading_model)));
    }
}

TEST_CASE("RenderObject under lights: the color of a textured triangle is not read", "[shading]") {
    const Texture texture = AddressTexture(16, 16);
    const std::vector<Light> lights = LampInFrontOfPixel4_0(0.25);
    for (const ShadingModel shading_model : kEveryShadingModel) {
        CHECK(Bytes(Lit(TexturedFace(texture, kRed), lights, shading_model)) ==
              Bytes(Lit(TexturedFace(texture, kBlue), lights, shading_model)));
    }
}

TEST_CASE(
    "RenderObject under lights: without a texture the texture coordinates and the "
    "mapping change nothing",
    "[shading]") {
    const std::vector<Light> lights = LampInFrontOfPixel4_0(0.25);
    Model pinned = Face(kYellow);
    pinned.triangles[0].uvs = kFaceUvs;
    for (const ShadingModel shading_model : kEveryShadingModel) {
        const Canvas expected = Lit(Face(kYellow), lights, shading_model);
        CHECK(Bytes(Lit(pinned, lights, shading_model)) == Bytes(expected));
        Canvas linear(kSize, kSize, kBlack);
        RenderObject(linear, TestViewport(), pinned, lights, shading_model, nullptr,
                     TextureMapping::kLinear);
        CHECK(Bytes(linear) == Bytes(expected));
    }
}

TEST_CASE(
    "RenderObject under lights: the texture mapping is the caller's choice, "
    "perspective-correct by default",
    "[shading]") {
    // A face that leans away from the camera, where the two mappings differ. Under full
    // ambient light the picture is the textured triangle through the projected corners,
    // which bring their depths along.
    const Texture texture = AddressTexture(16, 16);
    const std::vector<Light> lights = {AmbientLight{.intensity = 1.0}};
    const Model face = LeaningTexturedFace(texture);
    const std::array<Point, 3> corners = TexturedCorners(kLeaningCorners);
    const Canvas correct = Textured(corners, texture, TextureMapping::kPerspectiveCorrect);
    const Canvas linear = Textured(corners, texture, TextureMapping::kLinear);
    REQUIRE(Bytes(correct) != Bytes(linear));
    REQUIRE(LitPixels(correct) == LitPixels(linear));

    for (const ShadingModel shading_model : kEveryShadingModel) {
        INFO("shading model " << static_cast<int>(shading_model));
        CHECK(Bytes(Lit(face, lights, shading_model)) == Bytes(correct));
        Canvas asked_correct(kSize, kSize, kBlack);
        RenderObject(asked_correct, TestViewport(), face, lights, shading_model, nullptr,
                     TextureMapping::kPerspectiveCorrect);
        CHECK(Bytes(asked_correct) == Bytes(correct));
        Canvas asked_linear(kSize, kSize, kBlack);
        RenderObject(asked_linear, TestViewport(), face, lights, shading_model, nullptr,
                     TextureMapping::kLinear);
        CHECK(Bytes(asked_linear) == Bytes(linear));
    }
}

TEST_CASE(
    "RenderObject under lights: the texture filter is the caller's choice, nearest neighbor "
    "by default",
    "[shading]") {
    // The leaning face under full ambient light, where every shading model draws the
    // textured triangle through the projected corners. The texels are sixteen levels
    // apart, so that a blend of them shows.
    const Texture texture = AddressTexture(16, 16, 16);
    const std::vector<Light> lights = {AmbientLight{.intensity = 1.0}};
    const Model face = LeaningTexturedFace(texture);
    const std::array<Point, 3> corners = TexturedCorners(kLeaningCorners);
    const auto textured = [&](TextureMapping texture_mapping, TextureFilter texture_filter) {
        Canvas canvas(kSize, kSize, kBlack);
        DrawTexturedTriangle(canvas, corners[0], corners[1], corners[2], texture, texture_mapping,
                             nullptr, texture_filter);
        return canvas;
    };
    const auto rendered = [&](ShadingModel shading_model, TextureMapping texture_mapping,
                              TextureFilter texture_filter) {
        Canvas canvas(kSize, kSize, kBlack);
        RenderObject(canvas, TestViewport(), face, lights, shading_model, nullptr, texture_mapping,
                     texture_filter);
        return canvas;
    };
    const Canvas nearest =
        textured(TextureMapping::kPerspectiveCorrect, TextureFilter::kNearestNeighbor);
    const Canvas bilinear = textured(TextureMapping::kPerspectiveCorrect, TextureFilter::kBilinear);
    REQUIRE(Bytes(bilinear) != Bytes(nearest));
    REQUIRE(LitPixels(bilinear) == LitPixels(nearest));  // the same pixels, other colors

    for (const ShadingModel shading_model : kEveryShadingModel) {
        INFO("shading model " << static_cast<int>(shading_model));
        CHECK(Bytes(Lit(face, lights, shading_model)) == Bytes(nearest));
        CHECK(Bytes(rendered(shading_model, TextureMapping::kPerspectiveCorrect,
                             TextureFilter::kNearestNeighbor)) == Bytes(nearest));
        CHECK(Bytes(rendered(shading_model, TextureMapping::kPerspectiveCorrect,
                             TextureFilter::kBilinear)) == Bytes(bilinear));
        // The filter and the mapping are two choices, and both reach the triangle.
        CHECK(Bytes(rendered(shading_model, TextureMapping::kLinear, TextureFilter::kBilinear)) ==
              Bytes(textured(TextureMapping::kLinear, TextureFilter::kBilinear)));
    }

    // Without a texture there is nothing to filter.
    Model pinned = Face(kYellow);
    pinned.triangles[0].uvs = kFaceUvs;
    for (const ShadingModel shading_model : kEveryShadingModel) {
        Canvas filtered(kSize, kSize, kBlack);
        RenderObject(filtered, TestViewport(), pinned, lights, shading_model, nullptr,
                     TextureMapping::kPerspectiveCorrect, TextureFilter::kBilinear);
        CHECK(Bytes(filtered) == Bytes(Lit(Face(kYellow), lights, shading_model)));
    }
}

TEST_CASE("RenderObject under lights: through a depth buffer the nearer textured triangle shows",
          "[shading]") {
    // Two triangles on the same pixels in one object, the second twice as far and twice
    // as large, painted with one texture: wherever both are, the near one's texels show.
    const Texture texture = AddressTexture(16, 16);
    const std::vector<Light> lights = {AmbientLight{.intensity = 1.0}};
    Model both = TexturedFace(texture);
    for (const Vec3 corner : kCorners) {
        both.vertices.push_back(corner * 2.0);
    }
    // The far one shows the texture upside down, so that the two can be told apart.
    both.triangles.push_back(
        {.v = {3, 4, 5}, .color = kWhite, .uvs = {Uv{0.5, 1.0}, Uv{1.0, 0.0}, Uv{0.0, 0.0}}});
    const Canvas near_alone = Lit(TexturedFace(texture), lights, ShadingModel::kFlat);
    for (const ShadingModel shading_model : kEveryShadingModel) {
        Canvas canvas(kSize, kSize, kBlack);
        DepthBuffer depth_buffer(canvas);
        RenderObject(canvas, TestViewport(), both, lights, shading_model, &depth_buffer);
        CHECK(Bytes(canvas) == Bytes(near_alone));
        // Painted in list order the later one shows.
        CHECK(Bytes(Lit(both, lights, shading_model)) != Bytes(near_alone));
    }
}
