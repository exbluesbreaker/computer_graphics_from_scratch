#pragma once

#include <cgfs/image/texture.h>

namespace cgfs::demo {

// The textures of the demo pictures. A texture is read from a file, and a picture
// function reads no files: whoever draws a textured picture reads its texture once,
// beforehand, and hands it to the picture. So a picture cannot fail for a missing file
// halfway through, and the time render_demo prints stays that of the drawing alone: it
// reads the texture before it starts its clock.

/// The wooden crate: the side of a crate, 512 x 512 texels, a frame of four boards
/// around upright planks with one plank nailed across them from the top-left corner to
/// the bottom-right one. The texture of the book's chapter in kind, though not the same
/// picture.
///
/// It is read from `assets/textures/wooden_crate.ppm` in the source tree, whose place
/// the build compiles in. Throws std::runtime_error, with the path in the message, when
/// the file cannot be read.
[[nodiscard]] Texture CrateTexture();

}  // namespace cgfs::demo
