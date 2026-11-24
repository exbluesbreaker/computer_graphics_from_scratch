#pragma once

#include <cgfs/camera/camera.h>
#include <cgfs/camera/viewport.h>
#include <cgfs/image/canvas.h>
#include <cgfs/image/depth_buffer.h>
#include <cgfs/lighting/light.h>
#include <cgfs/rasterizer/culling.h>
#include <cgfs/rasterizer/instance.h>
#include <cgfs/rasterizer/model.h>
#include <cgfs/rasterizer/shading.h>
#include <cgfs/rasterizer/triangle.h>

#include <span>

namespace cgfs {

/// Draw a scene as `camera` sees it: every instance in `instances`, in order; the book's
/// RenderScene, in four steps:
///
///     M_camera = MakeCameraMatrix(camera.position, camera.orientation)
///     scene    = for each instance I:
///                    TransformModel(models[I.model], M_camera * MakeModelMatrix(I))
///     culled   = for each instance S of scene:
///                    CullBackFaces(S, the origin)
///     clipped  = ClipScene(culled, camera.clipping_planes)
///     for each instance C of clipped:
///                    RenderObject(C.vertices, C.triangles, draw_triangle, depth_buffer)
///
/// First the transforms: one matrix per instance takes its vertices from the model's
/// coordinates through the world into the camera's, and the instance becomes geometry
/// placed in the scene. Then the back faces go: in the camera's coordinates the camera
/// is at the origin, and every triangle turned away from it is removed (see
/// CullBackFaces), before the clipping, so that nothing is cut that would not be drawn.
/// Then the clipping, which is done in 3D and so must come after the transforms and
/// before the projection: what the camera's clipping planes leave of every instance (see
/// ClipScene). Last the perspective projection, vertex by vertex inside RenderObject and
/// kept out of the matrix on purpose (see Mat4), which is what leaves room for the two
/// steps in the middle.
///
/// Back-face culling is on unless `back_face_culling` is BackFaceCulling::kOff, with
/// which the second step is left out and every triangle is drawn. It takes the models to
/// be closed, with their triangles listed clockwise as seen from outside (see
/// culling.h). Then the picture of filled or shaded faces through a depth buffer is the
/// same with it and without, give or take a pixel on an outline, for about half the
/// work. Turn it off for a model that is not closed, for a view into an object that a
/// clipping plane has cut open, where the inside would otherwise be missing, and for a
/// wireframe that is to show the edges at the back as well.
///
/// The default camera is at the origin looking along +Z and has no clipping planes, so
/// a scene drawn without one is drawn as before. `draw_triangle` is how every triangle
/// is drawn, an outline unless the caller asks for DrawFilledTriangle or
/// DrawShadedTriangle (see RenderObject).
///
/// The rasterizer's scene is a list of models and a list of instances that name them by
/// index and say where they stand: the same shape as an object, a vertex list and a
/// triangle list that indexes it, one level up. It is the book's `scene.instances`, with
/// the models alongside because an Instance names its model rather than holding it, and
/// the camera beside them.
///
/// Instances are drawn in list order. Without a `depth_buffer`, where two overlap the
/// later one shows, and a scene of solid faces must be listed from the back to the
/// front. With one, a buffer of the canvas's size that the caller makes for the picture,
/// every triangle of every instance is drawn through it and a pixel shows whatever is
/// nearest there, in any order: hidden-surface removal (see DepthBuffer).
///
/// Preconditions: every `instance.model` is less than `models.size()`, asserted; after
/// the clipping every vertex has z > 0, asserted by ProjectVertex, the vertices of the
/// back faces that were removed included, since they stay in their instance's list. The
/// planes of MakeClippingPlanes guarantee the second for any scene; a camera without
/// clipping planes must have every instance entirely in front of it.
void RenderScene(Canvas& canvas, const Viewport& viewport, std::span<const Model> models,
                 std::span<const Instance> instances, const Camera& camera = Camera{},
                 DrawTriangleFunction draw_triangle = DrawWireframeTriangle,
                 DepthBuffer* depth_buffer = nullptr,
                 BackFaceCulling back_face_culling = BackFaceCulling::kOn);

/// Draw a scene under lights as `camera` sees it: the book's RenderScene of the Shading
/// chapter. The scene goes through the steps above, and so do its lights:
///
///     M_camera = MakeCameraMatrix(camera.position, camera.orientation)
///     scene    = for each instance I:
///                    TransformModel(models[I.model], M_camera * MakeModelMatrix(I))
///     lit_by   = for each light L of lights:
///                    TransformLight(L, M_camera)
///     culled, clipped as above
///     for each instance C of clipped:
///                    RenderObject(C, lit_by, shading_model, depth_buffer)
///
/// `lights` are given in the world's coordinates, where the instances are placed and the
/// camera stands. The light on a surface depends on where the surface, the lights and
/// the viewer are relative to one another, and on nothing else, so it can be computed in
/// any coordinates all of them are in. Here those are the camera's: the vertices are
/// there already, TransformModel turns the normals of the models with them, and
/// TransformLight takes the position of a point light and the direction of a
/// directional one along. The viewer is then the origin.
///
/// Every triangle is drawn in its color times the light it gets, computed per triangle,
/// per corner or per pixel as `shading_model` says: flat, Gouraud or Phong shading (see
/// shading.h, where RenderObject with lights does the work). A model's `specular` is how
/// shiny its instances are, and the normals at the corners of its triangles are what
/// lets Gouraud and Phong shading show a curved surface as one. The intensities stored
/// in the triangles are not read. Nothing casts a shadow.
///
/// Lit faces are solid, so a scene wants a `depth_buffer` here, a buffer of the canvas's
/// size, unless it is listed from the back to the front; without one the instances and
/// their triangles are painted in list order. Back-face culling is on unless it is
/// turned off, as above, and matters more here than there: a face turned away from the
/// camera is lit by its outward normal like any other and would be drawn over the front
/// of its object if nothing sorted the two. A triangle that the clipping cuts is lit as
/// its pieces: each piece has its own corners, with the normals of the whole triangle
/// interpolated to them, and under flat shading its own center.
///
/// Preconditions: as above.
void RenderScene(Canvas& canvas, const Viewport& viewport, std::span<const Model> models,
                 std::span<const Instance> instances, const Camera& camera,
                 std::span<const Light> lights, ShadingModel shading_model = ShadingModel::kPhong,
                 DepthBuffer* depth_buffer = nullptr,
                 BackFaceCulling back_face_culling = BackFaceCulling::kOn);

}  // namespace cgfs
