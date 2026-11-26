#include <cgfs/demo/textures.h>
#include <cgfs/image/image_io.h>

#include <cstdint>
#include <filesystem>
#include <utility>

namespace cgfs::demo {

Texture CrateTexture(Mipmapping mipmapping) {
    // CGFS_TEXTURE_DIR is assets/textures/ of the source tree, set by CMakeLists.txt.
    return Texture(ReadPpm(std::filesystem::path(CGFS_TEXTURE_DIR) / "wooden_crate.ppm"),
                   mipmapping);
}

Texture CheckerboardTexture(Mipmapping mipmapping) {
    constexpr int kSize = 256;  // texels along a side
    constexpr int kSquare = 4;  // texels along the side of a square
    constexpr std::uint8_t kWhite = 255;
    constexpr std::uint8_t kBlack = 0;

    Image image{.width = kSize, .height = kSize, .rgb = {}};
    image.rgb.reserve(static_cast<std::size_t>(kSize) * kSize * 3);
    for (int ty = 0; ty < kSize; ++ty) {
        for (int tx = 0; tx < kSize; ++tx) {
            // White where the column and the row of the square are both even or both odd.
            const bool white = (tx / kSquare + ty / kSquare) % 2 == 0;
            const std::uint8_t level = white ? kWhite : kBlack;
            image.rgb.insert(image.rgb.end(), {level, level, level});
        }
    }
    return Texture(std::move(image), mipmapping);
}

}  // namespace cgfs::demo
