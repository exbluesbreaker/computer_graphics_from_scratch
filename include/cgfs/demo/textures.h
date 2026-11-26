#pragma once

#include <cgfs/image/texture.h>

namespace cgfs::demo {

// The textures of the demo pictures. A texture is read from a file or made by a loop,
// and a picture function does neither: whoever draws a textured picture makes its
// texture once, beforehand, and hands it to the picture. So a picture cannot fail for a
// missing file halfway through, and the time render_demo prints stays that of the
// drawing alone: it makes the texture before it starts its clock, its mipmap levels
// included when `mipmapping` asks for them (see Mipmapping), which is where the book
// puts that work: "when loading a texture".

/// The wooden crate: the side of a crate, 512 x 512 texels, a frame of four boards
/// around upright planks with one plank nailed across them from the top-left corner to
/// the bottom-right one. The texture of the book's chapter in kind, though not the same
/// picture.
///
/// It is read from `assets/textures/wooden_crate.ppm` in the source tree, whose place
/// the build compiles in. Throws std::runtime_error, with the path in the message, when
/// the file cannot be read.
[[nodiscard]] Texture CrateTexture(Mipmapping mipmapping = Mipmapping::kOff);

/// The checkerboard: the texture of the book's example of what mipmapping is for, "half
/// the pixels are black and half the pixels are white, laid out in a checkerboard
/// pattern". 256 x 256 texels in squares of 4 x 4, 64 squares each way, the top-left
/// one white.
///
/// No picture is harder on a texture lookup. Drawn at a quarter of its size or smaller a
/// pixel covers a whole number of black and of white squares, the same number of each,
/// and ought to be gray; a lookup that takes one texel can only say black or white, and
/// says them in patterns that have nothing to do with the squares. With mipmapping its
/// levels are squares of 2 x 2 texels at half the size, of single texels at a quarter,
/// and from an eighth on every texel is the mean of as many black texels as white ones:
/// gray, 127.5 on the scale to 255, exactly.
///
/// It is made here, by a loop, and no file is read.
[[nodiscard]] Texture CheckerboardTexture(Mipmapping mipmapping = Mipmapping::kOff);

}  // namespace cgfs::demo
