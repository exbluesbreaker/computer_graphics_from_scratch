#pragma once

#include <cgfs/image/texture.h>
#include <cgfs/rasterizer/point.h>

namespace cgfs {

/// Which of a texture's mipmap levels the triangle p0 p1 p2 is painted with: in the
/// book's words, "the texture whose scale best matches its size".
///
/// The points are the corners of the triangle on the canvas, each with the point of the
/// texture it shows (see Point and Uv). Between them the texture is taken to lie evenly
/// on the canvas, so that a step of one pixel is the same step on the texture wherever
/// in the triangle it is taken: with
///
///     A = P1 - P0,   B = P2 - P0     two sides of the triangle on the canvas, in pixels
///     S = T1 - T0,   T = T2 - T0     the same two sides on the texture, in texels
///
/// where Tk = (uk * width, vk * height) in the texels of the texture itself, there is
/// one 2 x 2 matrix M that takes every step on the canvas to its step on the texture,
/// the one with M A = S and M B = T:
///
///     M = [S T] [A B]^-1
///
/// A step of one pixel in the direction of a unit vector n is a step of |M n| texels.
/// That is how small the texture is drawn in that direction: where it is 1, a texel and
/// a pixel are the same size; where it is 4, a pixel steps over four texels and three
/// of every four are never seen. It is not the same in every direction. A floor that
/// runs toward the horizon is drawn far smaller in depth than across, and the pixels
/// skip texels in depth long before they do across. So the size that counts is the
/// direction in which the texture is drawn smallest, the largest |M n| there is:
///
///     q  = m11^2 + m12^2 + m21^2 + m22^2
///     d  = m11 m22 - m12 m21
///     s^2 = (q + sqrt(q^2 - 4 d^2)) / 2
///
/// s texels to a pixel. (The unit steps in all directions are a circle, M makes an
/// ellipse of it, and s is the longer half-axis of the ellipse.) Level k has texels
/// 2^k texels of the texture wide, so it suits s = 2^k, and the level whose scale is
/// nearest to s, as a ratio, is the result:
///
///     level k   for   2^(k - 1/2) <= s < 2^(k + 1/2)
///
/// The book's square that is 10 times smaller than its texture gets level 3, the copy
/// that is 8 times smaller: 10 is between 5.66 and 11.3. A triangle whose texture is
/// drawn at its own size or larger, s below 1.41, gets level 0, the texture itself. A
/// triangle drawn smaller than the last level suits gets the last, one texel. The
/// borders are compared as s^2 against 2, 8, 32 and so on, without a logarithm.
///
/// For a triangle parallel to the viewport the texture does lie evenly on the canvas,
/// and this is exact. For one that leans, the step on the texture grows from the near
/// end of the triangle to the far end, and M, which goes by the corners alone, is
/// between the two: one level per triangle is what the book describes, and a triangle
/// that reaches from the camera's feet to the horizon is painted with one compromise.
/// Choosing by the smallest direction keeps the pixels from skipping texels; the price
/// is that the other direction is then drawn from a copy smaller than it needed, and
/// is blurred.
///
/// Two cases have no matrix. Three corners that show one point of the texture have no
/// texels to skip: level 0. Three corners on one line of the canvas, with a texture
/// spread over them, are a triangle seen edge on, infinitely small across: the last
/// level.
///
/// A texture without mipmapping has one level, and the result is 0 whatever the
/// triangle: the triangle is painted with the texture itself, as it was before there
/// were levels. The vertex order does not matter.
[[nodiscard]] int MipmapLevel(Point p0, Point p1, Point p2, const Texture& texture);

}  // namespace cgfs
