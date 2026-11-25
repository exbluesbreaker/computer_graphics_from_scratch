#include <cgfs/demo/textures.h>
#include <cgfs/image/image_io.h>

#include <filesystem>

namespace cgfs::demo {

Texture CrateTexture() {
    // CGFS_TEXTURE_DIR is assets/textures/ of the source tree, set by CMakeLists.txt.
    return Texture(ReadPpm(std::filesystem::path(CGFS_TEXTURE_DIR) / "wooden_crate.ppm"));
}

}  // namespace cgfs::demo
