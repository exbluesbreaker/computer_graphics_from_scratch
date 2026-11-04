#include <cgfs/image/canvas.h>

#include <cgfs/image/image_io.h>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <fstream>
#include <stdexcept>
#include <vector>

#include "support/test_helpers.h"

using cgfs::Canvas;
using cgfs::Color;
using cgfs::Rgb8;
using namespace cgfs::colors;

namespace {

// The stored pixel at buffer row `row` and column `column` (top-left is 0, 0).
Rgb8 PixelAt(const Canvas& canvas, int row, int column) {
    const auto data = canvas.Data();
    const std::size_t offset =
        (static_cast<std::size_t>(row) * static_cast<std::size_t>(canvas.width()) +
         static_cast<std::size_t>(column)) *
        3;
    return {data[offset], data[offset + 1], data[offset + 2]};
}

bool AllPixelsAre(const Canvas& canvas, Rgb8 expected) {
    const auto data = canvas.Data();
    for (std::size_t i = 0; i < data.size(); i += 3) {
        if (Rgb8{data[i], data[i + 1], data[i + 2]} != expected) {
            return false;
        }
    }
    return true;
}

}  // namespace

TEST_CASE("Canvas defaults to 1920x1080, white", "[canvas]") {
    const Canvas canvas;
    CHECK(canvas.width() == 1920);
    CHECK(canvas.height() == 1080);
    CHECK(canvas.background() == kWhite);
    CHECK(canvas.Data().size() == std::size_t{1920} * 1080 * 3);
    CHECK(AllPixelsAre(canvas, Rgb8{255, 255, 255}));
}

TEST_CASE("Canvas rejects non-positive dimensions", "[canvas]") {
    CHECK_THROWS_AS(Canvas(0, 10), std::invalid_argument);
    CHECK_THROWS_AS(Canvas(10, -1), std::invalid_argument);
}

TEST_CASE("PutPixel maps centered coordinates onto buffer rows and columns", "[canvas]") {
    Canvas canvas(4, 4, kBlack);

    canvas.PutPixel(0, 0, kRed);  // origin: column 2, row 1
    CHECK(PixelAt(canvas, 1, 2) == Rgb8{255, 0, 0});

    canvas.PutPixel(-2, 1, kGreen);  // top-left corner
    CHECK(PixelAt(canvas, 0, 0) == Rgb8{0, 255, 0});

    canvas.PutPixel(1, 1, kBlue);  // top-right corner
    CHECK(PixelAt(canvas, 0, 3) == Rgb8{0, 0, 255});

    canvas.PutPixel(-2, -2, kWhite);  // bottom-left corner
    CHECK(PixelAt(canvas, 3, 0) == Rgb8{255, 255, 255});

    canvas.PutPixel(1, -2, kYellow);  // bottom-right corner
    CHECK(PixelAt(canvas, 3, 3) == Rgb8{255, 255, 0});
}

TEST_CASE("GetPixel round-trips PutPixel", "[canvas]") {
    Canvas canvas(8, 6);
    canvas.PutPixel(3, -2, kBlue);
    CHECK(canvas.GetPixel(3, -2) == Rgb8{0, 0, 255});
    CHECK(canvas.GetPixel(0, 0) == Rgb8{255, 255, 255});
    CHECK_FALSE(canvas.GetPixel(4, 0).has_value());
    CHECK_FALSE(canvas.GetPixel(0, 3).has_value());
}

TEST_CASE("PutPixel ignores out-of-range coordinates", "[canvas]") {
    Canvas canvas(4, 4);
    canvas.PutPixel(2, 0, kRed);   // x must be < width/2
    canvas.PutPixel(-3, 0, kRed);  // x must be >= -width/2
    canvas.PutPixel(0, 2, kRed);   // y must be < height/2
    canvas.PutPixel(0, -3, kRed);  // y must be >= -height/2
    CHECK(AllPixelsAre(canvas, Rgb8{255, 255, 255}));
}

TEST_CASE("Odd dimensions follow the integer-division mapping", "[canvas]") {
    // column = 5/2 + x = 2 + x, row = 3/2 - 1 - y = -y.
    Canvas canvas(5, 3, kBlack);

    canvas.PutPixel(2, 0, kRed);  // column 4, row 0: valid
    CHECK(PixelAt(canvas, 0, 4) == Rgb8{255, 0, 0});

    canvas.PutPixel(0, -2, kBlue);  // column 2, row 2: valid
    CHECK(PixelAt(canvas, 2, 2) == Rgb8{0, 0, 255});

    canvas.PutPixel(0, 1, kGreen);  // row -1: ignored
    canvas.PutPixel(-3, 0, kGreen);  // column -1: ignored
    CHECK_FALSE(canvas.GetPixel(0, 1).has_value());
    CHECK_FALSE(canvas.GetPixel(-3, 0).has_value());
}

TEST_CASE("Clear fills with the background and can change it", "[canvas]") {
    Canvas canvas(3, 3, kRed);
    CHECK(AllPixelsAre(canvas, Rgb8{255, 0, 0}));

    canvas.PutPixel(0, 0, kBlue);
    canvas.Clear();
    CHECK(canvas.background() == kRed);
    CHECK(AllPixelsAre(canvas, Rgb8{255, 0, 0}));

    canvas.Clear(Color{20.0, 20.0, 40.0});
    CHECK(canvas.background() == Color{20.0, 20.0, 40.0});
    CHECK(AllPixelsAre(canvas, Rgb8{20, 20, 40}));
}

TEST_CASE("PutPixel clamps and truncates colors", "[canvas]") {
    Canvas canvas(2, 2);
    canvas.PutPixel(0, 0, Color{300.0, -1.0, 127.9});
    CHECK(canvas.GetPixel(0, 0) == Rgb8{255, 0, 127});
}

TEST_CASE("Save writes PPM and PNG by extension", "[canvas]") {
    Canvas canvas(3, 2, kBlue);
    canvas.PutPixel(0, 0, kRed);

    SECTION("ppm round-trips through ReadPpm") {
        const cgfs_test::TempFile file(".ppm");
        canvas.Save(file.path());
        const cgfs::Image image = cgfs::ReadPpm(file.path());
        CHECK(image.width == 3);
        CHECK(image.height == 2);
        CHECK(std::vector<std::uint8_t>(canvas.Data().begin(), canvas.Data().end()) == image.rgb);
    }

    SECTION("png starts with the PNG signature") {
        const cgfs_test::TempFile file(".PNG");  // extension matching is case-insensitive
        canvas.Save(file.path());
        std::ifstream in(file.path(), std::ios::binary);
        REQUIRE(in.good());
        std::vector<unsigned char> header(8);
        in.read(reinterpret_cast<char*>(header.data()), 8);
        CHECK(header == std::vector<unsigned char>{0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A});
    }

    SECTION("unknown extension throws") {
        const cgfs_test::TempFile file(".bmp");
        CHECK_THROWS_AS(canvas.Save(file.path()), std::runtime_error);
    }
}
