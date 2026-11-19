#include <cgfs/image/canvas.h>
#include <cgfs/image/color.h>
#include <cgfs/image/pixel_index.h>

#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <optional>

using cgfs::Canvas;
using cgfs::PixelIndex;
using namespace cgfs::colors;

TEST_CASE("PixelIndex: rows are stored top first and the origin is in the middle",
          "[pixel_index]") {
    // 4 x 4: column = 2 + x, row = 1 - y.
    CHECK(PixelIndex(4, 4, 0, 0) == std::size_t{6});     // row 1, column 2
    CHECK(PixelIndex(4, 4, -2, 1) == std::size_t{0});    // the top-left corner
    CHECK(PixelIndex(4, 4, 1, 1) == std::size_t{3});     // the top-right corner
    CHECK(PixelIndex(4, 4, -2, -2) == std::size_t{12});  // the bottom-left corner
    CHECK(PixelIndex(4, 4, 1, -2) == std::size_t{15});   // the bottom-right corner
}

TEST_CASE("PixelIndex: x grows to the right and y upward", "[pixel_index]") {
    const std::size_t origin = *PixelIndex(8, 6, 0, 0);
    CHECK(PixelIndex(8, 6, 1, 0) == origin + 1);  // the next column
    CHECK(PixelIndex(8, 6, 0, 1) == origin - 8);  // the row above, one width earlier
    CHECK(PixelIndex(8, 6, 0, -1) == origin + 8);
}

TEST_CASE("PixelIndex: a pixel outside the grid has no index", "[pixel_index]") {
    CHECK_FALSE(PixelIndex(4, 4, 2, 0).has_value());   // x must be < width/2
    CHECK_FALSE(PixelIndex(4, 4, -3, 0).has_value());  // x must be >= -width/2
    CHECK_FALSE(PixelIndex(4, 4, 0, 2).has_value());   // y must be < height/2
    CHECK_FALSE(PixelIndex(4, 4, 0, -3).has_value());  // y must be >= -height/2
}

TEST_CASE("PixelIndex: odd dimensions follow the integer-division mapping", "[pixel_index]") {
    // column = 5/2 + x = 2 + x, row = 3/2 - 1 - y = -y.
    CHECK(PixelIndex(5, 3, 2, 0) == std::size_t{4});    // column 4, row 0
    CHECK(PixelIndex(5, 3, 0, -2) == std::size_t{12});  // column 2, row 2
    CHECK_FALSE(PixelIndex(5, 3, 0, 1).has_value());    // row -1
    CHECK_FALSE(PixelIndex(5, 3, -3, 0).has_value());   // column -1
}

TEST_CASE("PixelIndex: usable at compile time", "[pixel_index]") {
    static_assert(PixelIndex(4, 4, 0, 0) == std::size_t{6});
    static_assert(!PixelIndex(4, 4, 2, 0).has_value());
    SUCCEED();
}

TEST_CASE("PixelIndex: the canvas keeps its pixels where PixelIndex says", "[pixel_index]") {
    // Every pixel of an odd-sized canvas, and a border of coordinates around it.
    const int width = 5;
    const int height = 3;
    for (int x = -4; x <= 4; ++x) {
        for (int y = -4; y <= 4; ++y) {
            INFO("pixel (" << x << ", " << y << ")");
            Canvas canvas(width, height, kBlack);
            canvas.PutPixel(x, y, kWhite);
            const std::optional<std::size_t> index = PixelIndex(width, height, x, y);
            CHECK(canvas.GetPixel(x, y).has_value() == index.has_value());
            const auto data = canvas.Data();
            for (std::size_t i = 0; i < data.size(); ++i) {
                const bool lit = index.has_value() && i / 3 == *index;
                CHECK(data[i] == (lit ? 255 : 0));
            }
        }
    }
}
