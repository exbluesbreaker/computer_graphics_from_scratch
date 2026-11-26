#include <cgfs/rasterizer/mipmap_level.h>

#include <algorithm>
#include <cmath>

namespace cgfs {

int MipmapLevel(Point p0, Point p1, Point p2, const Texture& texture) {
    const int last = texture.level_count() - 1;
    if (last == 0) {
        return 0;  // no mipmapping: the texture is all there is
    }

    // Two sides of the triangle on the canvas, in pixels, and on the texture, in texels.
    const double ax = p1.x - p0.x;
    const double ay = p1.y - p0.y;
    const double bx = p2.x - p0.x;
    const double by = p2.y - p0.y;
    const double sx = (p1.uv.u - p0.uv.u) * texture.width();
    const double sy = (p1.uv.v - p0.uv.v) * texture.height();
    const double tx = (p2.uv.u - p0.uv.u) * texture.width();
    const double ty = (p2.uv.v - p0.uv.v) * texture.height();

    if (sx == 0.0 && sy == 0.0 && tx == 0.0 && ty == 0.0) {
        return 0;  // one point of the texture: nothing is skipped
    }
    const double determinant = ax * by - ay * bx;  // twice the triangle's area on the canvas
    if (determinant == 0.0) {
        return last;  // seen edge on: every texel of the texture on no pixels at all
    }

    // M = [S T] [A B]^-1: a step on the canvas, in pixels, to its step on the texture,
    // in texels.
    const double m11 = (sx * by - tx * ay) / determinant;
    const double m12 = (tx * ax - sx * bx) / determinant;
    const double m21 = (sy * by - ty * ay) / determinant;
    const double m22 = (ty * ax - sy * bx) / determinant;

    // The square of the longest step on the texture that a step of one pixel can be.
    // Under the root is the square of a difference, which rounding could take just
    // below 0.
    const double q = m11 * m11 + m12 * m12 + m21 * m21 + m22 * m22;
    const double d = m11 * m22 - m12 * m21;
    const double s_squared = (q + std::sqrt(std::max(q * q - 4.0 * d * d, 0.0))) / 2.0;

    // Level k suits s = 2^k, and the border between it and the next is halfway between
    // the two as a ratio, s = 2^(k + 1/2): s^2 = 2, 8, 32, ... A triangle on a border
    // gets the smaller copy. Triangles exactly on a border are not rare, with corners
    // on whole pixels and a texture whose side is a power of two: a tile of a far floor
    // that is one pixel high, with the diagonal of its texture running up it, is one.
    // Their s^2 comes out on the border or a rounding error beside it, so a border
    // counts as reached from a billionth below it, a million times what the rounding
    // can be.
    constexpr double kOnTheBorder = 1.0 - 1e-9;
    int level = 0;
    for (double border = 2.0; level < last && s_squared >= border * kOnTheBorder; border *= 4.0) {
        ++level;
    }
    return level;
}

}  // namespace cgfs
