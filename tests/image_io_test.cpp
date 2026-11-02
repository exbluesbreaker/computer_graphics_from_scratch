#include <cgfs/image_io.h>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <vector>

#include "test_helpers.h"

using cgfs::Image;
using cgfs::ReadPpm;
using cgfs::WritePng;
using cgfs::WritePpm;

namespace {

const std::vector<std::uint8_t> kPixels2x2 = {
    255, 0,   0,   0,   255, 0,    // top row: red, green
    0,   0,   255, 255, 255, 255,  // bottom row: blue, white
};

}  // namespace

TEST_CASE("PPM write then read round-trips the bytes", "[image_io]") {
    const cgfs_test::TempFile file(".ppm");
    WritePpm(file.path(), 2, 2, kPixels2x2);

    const Image image = ReadPpm(file.path());
    CHECK(image.width == 2);
    CHECK(image.height == 2);
    CHECK(image.rgb == kPixels2x2);
}

TEST_CASE("PPM header is the plain P6 form", "[image_io]") {
    const cgfs_test::TempFile file(".ppm");
    WritePpm(file.path(), 2, 2, kPixels2x2);

    std::ifstream in(file.path(), std::ios::binary);
    std::string header(11, '\0');
    in.read(header.data(), 11);
    CHECK(header == "P6\n2 2\n255\n");
}

TEST_CASE("Writers reject a buffer that does not match the dimensions", "[image_io]") {
    const cgfs_test::TempFile file(".ppm");
    CHECK_THROWS_AS(WritePpm(file.path(), 3, 2, kPixels2x2), std::invalid_argument);
    CHECK_THROWS_AS(WritePng(file.path(), 0, 2, kPixels2x2), std::invalid_argument);
}

TEST_CASE("PNG writer produces a file with the PNG signature", "[image_io]") {
    const cgfs_test::TempFile file(".png");
    WritePng(file.path(), 2, 2, kPixels2x2);

    std::ifstream in(file.path(), std::ios::binary);
    REQUIRE(in.good());
    std::vector<unsigned char> signature(8);
    in.read(reinterpret_cast<char*>(signature.data()), 8);
    CHECK(signature == std::vector<unsigned char>{0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A});
}

TEST_CASE("ReadPpm rejects files it cannot understand", "[image_io]") {
    SECTION("missing file") {
        const cgfs_test::TempFile file(".ppm");
        CHECK_THROWS_AS(ReadPpm(file.path()), std::runtime_error);
    }
    SECTION("not P6") {
        const cgfs_test::TempFile file(".ppm");
        std::ofstream(file.path(), std::ios::binary) << "P3\n1 1\n255\n255 0 0\n";
        CHECK_THROWS_AS(ReadPpm(file.path()), std::runtime_error);
    }
    SECTION("truncated pixel data") {
        const cgfs_test::TempFile file(".ppm");
        std::ofstream(file.path(), std::ios::binary) << "P6\n2 2\n255\n" << "abc";
        CHECK_THROWS_AS(ReadPpm(file.path()), std::runtime_error);
    }
}
