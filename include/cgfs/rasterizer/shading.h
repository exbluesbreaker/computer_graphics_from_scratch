#pragma once

#include <cgfs/camera/viewport.h>
#include <cgfs/image/canvas.h>
#include <cgfs/image/depth_buffer.h>
#include <cgfs/image/texture.h>
#include <cgfs/lighting/light.h>
#include <cgfs/rasterizer/model.h>
#include <cgfs/rasterizer/triangle.h>

#include <span>

namespace cgfs {

// Shading: from the light at a point to the light on a triangle.
//
// The illumination equation (see illumination.h) says how bright one point of a surface
// is, given the point, the normal of the surface there and the lights. A triangle covers
// many pixels, and shading, the book's word, is how light computed at points is spread
// over all of them. The book has three ways, and shows the second of them in two forms,
// before the model has normals and after: four pictures of one scene. Each is right in
// cases where the one before it is not.
//
// Flat shading computes the light once per triangle, at its center and with the
// triangle's own normal, and fills the triangle with its color times that. A flat face
// lit from far away is about equally bright all over, so a cube comes out right, give or
// take a seam where the two triangles of a face got values a little apart. A sphere
// comes out as what it is made of: every triangle has its own normal and with it its own
// shade, and the facets show.
//
// Gouraud shading computes the light at the three corners and has DrawShadedTriangle
// interpolate it over the triangle. The chapter comes to it in two steps.
//
// First with nothing but the triangles to go by: every corner is lit with the normal of
// its own triangle. This is "faceted" Gouraud shading here, for what it leaves of a
// sphere. It mends the cube: the two triangles of a face have the same normal and share
// two corners, so they compute the same light there, and the seam between them is gone.
// It does not mend the sphere. The triangles that meet at a vertex of a curved surface
// each light that vertex with a normal of their own, their shades do not meet there, and
// every triangle still shows, now with a slight gradient across it.
//
// Then with the normals of the model: when the corners of a triangle carry the normals
// of the curved surface the triangle stands for (see Triangle), all the triangles around
// a vertex compute the same light there, the shade runs on smoothly across their edges,
// and the sphere looks round. Nothing about the computing has changed between the two
// steps, only which normal goes into it: for a model that states no normals they are
// the same picture. What neither can show is anything the light does between the
// corners. A highlight that falls inside a triangle is missed, or caught at one corner
// and smeared over the triangle. A lamp close to a large face leaves the face dim, and
// dimmer the closer it comes: at the far-away corners its light arrives at a grazing
// angle, and the bright spot in front of the lamp is nowhere computed.
//
// Phong shading interpolates the normals instead of the light and computes the light at
// every pixel (see DrawPhongTriangle). The highlights are where they belong and have
// their shape, and a lamp close to a face lights the part of the face it is close to.
// The price is one evaluation of the equation per pixel instead of one or three per
// triangle.
//
// None of them changes the outline of an object: a sphere made of triangles keeps its
// corners along its silhouette, however smoothly its inside is shaded.

/// Which of the four a scene is shaded with, in the chapter's order: flat shading;
/// Gouraud shading with every triangle's own normal at its corners, whatever normals
/// the model states; Gouraud shading with the model's normals; Phong shading.
enum class ShadingModel { kFlat, kGouraudFaceted, kGouraud, kPhong };

/// Draw an object under lights: the book's RenderModel of the Shading chapter.
///
/// `object` is a model whose vertices and normals are in the camera's coordinates, as
/// TransformModel returns it for the camera matrix times an instance's model matrix, and
/// `lights` are the lights in the same coordinates (see TransformLight). Every vertex is
/// projected once with ProjectVertex through `viewport`, as RenderObject does without
/// lights, and every triangle is then drawn in its color times the light that falls on
/// it, by `shading_model`:
///
///     kFlat              i  = ComputeIllumination(C, N, -C)
///                        DrawFilledTriangle(P0, P1, P2, color * i)
///
///     kGouraudFaceted    hk = ComputeIllumination(Vk, N, -Vk)       for each corner k
///                        DrawShadedTriangle(P0, P1, P2 with the intensities hk, color)
///
///     kGouraud           hk = ComputeIllumination(Vk, Nk, -Vk)      for each corner k
///                        DrawShadedTriangle(P0, P1, P2 with the intensities hk, color)
///
///     kPhong             DrawPhongTriangle(P0, P1, P2 with the normals Nk, color)
///
/// Vk is the corner k of the triangle and Pk its projection; C = (V0 + V1 + V2) / 3 is
/// the triangle's center. N is the triangle's own normal (TriangleNormal), normalized.
/// Nk is the normal the triangle states at its corner k, or N where it states none (see
/// Triangle). The view vector of a point is the direction from it to the camera, and
/// the camera is the origin of its own coordinates: minus the point. The specular
/// exponent is `object.specular`.
///
/// The normals Nk are taken as they are. An object that TransformModel placed has them
/// at unit length. One that the clipping then cut has, at its new corners, the normals
/// interpolated between those, which are shorter than 1 and are exactly what Phong
/// shading's own interpolation over the whole triangle has at those points: so the
/// pieces of a cut triangle are lit as the triangle was. Gouraud shading normalizes a
/// corner's normal to compute the light there, and Phong shading normalizes at every
/// pixel, after the interpolation.
///
/// Flat shading uses the triangle's own normal whatever normals the model states: one
/// point, one normal and one value per triangle is what it is. Faceted Gouraud shading
/// does the same at each of the three corners, which is all that tells it from Gouraud
/// shading: it is the picture Gouraud shading draws of the same model without its
/// normals. The intensities stored in the triangles (`h`) are not read by any of the
/// four; under lights they are computed. A triangle without area has no normal and is
/// not drawn.
///
/// An object with a texture (`object.texture`, see Model) is painted with it: the color
/// of a pixel is then the texel its triangle shows there, by the texture coordinates at
/// the triangle's corners, and the triangle's own `color` is not read. The light is
/// computed as above and multiplies the texel instead: "fetch the texel, apply shading,
/// and paint the pixel", in the book's words.
///
///     kFlat              DrawTexturedTriangle(P0, P1, P2 with the intensity i at each)
///     kGouraudFaceted,
///     kGouraud           DrawTexturedTriangle(P0, P1, P2 with the intensities hk)
///     kPhong             DrawPhongTriangle(P0, P1, P2 with the normals Nk, texture)
///
/// `texture_mapping` is how the texture coordinates get from the corners to the pixels,
/// perspective-correct unless the caller asks for the linear mapping the book's chapter
/// starts with (see TextureMapping). `texture_filter` is how the color at those
/// coordinates is made from the texels there: the one texel the point falls in unless
/// the caller asks for bilinear filtering, which blends the four around it (see
/// TextureFilter). Neither makes a difference to an object without a texture. A texture
/// that was made with mipmap levels is painted, triangle by triangle, with the level
/// that suits the size the triangle is drawn at (see Mipmapping and MipmapLevel); that
/// is the texture's doing and the primitives', and nothing here asks for it.
///
/// The triangles are drawn in list order, through `depth_buffer` if there is one, so
/// that a pixel shows the nearest of them; without one the later triangle shows, as in
/// RenderObject. Nothing is clipped and nothing is culled here; RenderScene does both
/// before an object arrives.
///
/// Preconditions: every index is less than `object.vertices.size()`, asserted; every
/// vertex has z > 0, asserted by ProjectVertex; the depth buffer, if there is one, has
/// the canvas's size, asserted by the triangles; the normals an object states are unit
/// vectors or interpolated between unit vectors, as above, which TransformModel and the
/// clipping see to and nothing checks: under Phong shading normals of unequal lengths
/// would pull the interpolation toward the longer ones.
void RenderObject(Canvas& canvas, const Viewport& viewport, const Model& object,
                  std::span<const Light> lights, ShadingModel shading_model = ShadingModel::kPhong,
                  DepthBuffer* depth_buffer = nullptr,
                  TextureMapping texture_mapping = TextureMapping::kPerspectiveCorrect,
                  TextureFilter texture_filter = TextureFilter::kNearestNeighbor);

}  // namespace cgfs
