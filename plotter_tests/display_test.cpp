#include <display.h>
#include <catch2.hpp>
#include <bitset>
#include <algorithm>

using namespace ev3plotter;

bool get_pixel(const unsigned char* buffer, std::uint32_t x, std::uint32_t y, std::uint32_t displayWidth, std::uint32_t bytesPerPixel = 4) {
    const auto baseIndex = (y*displayWidth + x) * bytesPerPixel;
    for (auto p = buffer + baseIndex; p != buffer + baseIndex + bytesPerPixel; ++p) {
        if (*p != 0xff) {
            return true;
        }
    }

    return false;
}

std::string get_picture(const unsigned char* buffer, std::uint32_t width, std::uint32_t height, std::uint32_t bytesPerPixel = 4) {
    std::string picture;
    picture.resize(height * (width + 1) + 1, '\n');

    for (std::uint32_t x = 0; x != width; ++x) {
        for (std::uint32_t y = 0; y != height; ++y) {
            const auto color = get_pixel(buffer, x, y, width, bytesPerPixel);
            picture[1 + y*(width+1) + x] = color ? '#' : '.';
        }
    }

    return picture;
}

TEST_CASE("Test get_pixel by itself") {

    SECTION("All kinds of non-empty pixels")
    {
        constexpr unsigned char c[] { 0, 1, 3, 0xfe,   0xff, 0xff, 0xff, 0xfe,   0, 0, 0, 0};

        REQUIRE(get_pixel(c, 0, 0, 8, 3) == true);
        REQUIRE(get_pixel(c, 1, 0, 8, 3) == true);
        REQUIRE(get_pixel(c, 2, 0, 8, 3) == true);
        REQUIRE(get_pixel(c, 3, 0, 8, 3) == true);
    }

    SECTION("Some empty pixels (1 byte per pixel)")
    {
        constexpr unsigned char c[]{0,0xff,42};

        REQUIRE(get_pixel(c, 0, 0, 8, 1) == true);
        REQUIRE(get_pixel(c, 1, 0, 8, 1) == false);
        REQUIRE(get_pixel(c, 2, 0, 8, 1) == true);

        REQUIRE(get_pixel(c, 2, 0, 8, 3) == true);
    }

    SECTION("display 16x2")
    {
        constexpr unsigned char c[] {
            0x01,0xff,0xff,0xff,0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0X01,
            0x01,0xff,0xff,0xff,0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff,0xff,0xff,0x01,0X01};

        REQUIRE(get_pixel(c, 0, 0, 16, 1) == true);
        REQUIRE(get_pixel(c, 1, 0, 16, 1) == false);
        REQUIRE(get_pixel(c, 7, 0, 16, 1) == false);
        REQUIRE(get_pixel(c, 8, 0, 16, 1) == false);
        REQUIRE(get_pixel(c, 14, 0, 16, 1) == false);
        REQUIRE(get_pixel(c, 15, 0, 16, 1) == true);

        REQUIRE(get_pixel(c, 0, 1, 16, 1) == true);
        REQUIRE(get_pixel(c, 7, 1, 16, 1) == false);
        REQUIRE(get_pixel(c, 8, 1, 16, 1) == false);
        REQUIRE(get_pixel(c, 14, 1, 16, 1) == true);
        REQUIRE(get_pixel(c, 15, 1, 16, 1) == true);

        REQUIRE(get_pixel(c, 0, 0, 4, 4) == true);
        REQUIRE(get_pixel(c, 1, 0, 4, 4) == false);
        REQUIRE(get_pixel(c, 2, 0, 4, 4) == false);
        REQUIRE(get_pixel(c, 3, 0, 4, 4) == true);

        REQUIRE(get_pixel(c, 0, 1, 4, 4) == true);
        REQUIRE(get_pixel(c, 1, 1, 4, 4) == false);
        REQUIRE(get_pixel(c, 2, 1, 4, 4) == false);
        REQUIRE(get_pixel(c, 3, 1, 4, 4) == true);
    }
}

TEST_CASE("Display properly puts pixels into buffer") {
    constexpr std::uint32_t displayWidth = 4;
    constexpr std::uint32_t displayHeight = 2;

    unsigned char buffer[displayWidth*displayHeight*4];
    std::fill(std::begin(buffer), std::end(buffer), 0xff);

    Display d{buffer, displayWidth, displayHeight};

    d.set(0, 0, true);
    d.set(1, 0, true);
    d.set(3, 1, true);

    REQUIRE(get_pixel(buffer, 0, 0, displayWidth) == true);
    REQUIRE(get_pixel(buffer, 1, 0, displayWidth) == true);
    REQUIRE(get_pixel(buffer, 2, 0, displayWidth) == false);
    REQUIRE(get_pixel(buffer, 3, 0, displayWidth) == false);

    REQUIRE(get_pixel(buffer, 0, 1, displayWidth) == false);
    REQUIRE(get_pixel(buffer, 1, 1, displayWidth) == false);
    REQUIRE(get_pixel(buffer, 2, 1, displayWidth) == false);
    REQUIRE(get_pixel(buffer, 3, 1, displayWidth) == true);

    REQUIRE(get_picture(buffer, displayWidth, displayHeight) == R"(
##..
...#
)");
}


struct TenBySixDisplay{
    static constexpr std::uint32_t displayWidth = 10;
    static constexpr std::uint32_t displayHeight = 6;

    TenBySixDisplay() {
        std::fill(std::begin(buffer), std::end(buffer), 0xff);
    }

    unsigned char buffer[displayWidth*displayHeight*4];
    Display d{buffer, displayWidth, displayHeight};
};


TEST_CASE("Rectangles") {
    TenBySixDisplay d{};

    SECTION("Full fill") {
        rectangle(d.d, 0, 0, d.displayWidth, d.displayHeight, true);
        REQUIRE(get_picture(d.buffer, d.displayWidth, d.displayHeight) == R"(
##########
##########
##########
##########
##########
##########
)");
    }

    SECTION("-1 offset") {
        rectangle(d.d, 1, 1, d.displayWidth-2, d.displayHeight-2, true);
        REQUIRE(get_picture(d.buffer, d.displayWidth, d.displayHeight) == R"(
..........
.########.
.########.
.########.
.########.
..........
)");
    }

    SECTION("-1 offset + white rectangle") {
        rectangle(d.d, 1, 1, d.displayWidth-2, d.displayHeight-2, true);
        rectangle(d.d, 4, 3, 4, 2, false);

        REQUIRE(get_picture(d.buffer, d.displayWidth, d.displayHeight) == R"(
..........
.########.
.########.
.###....#.
.###....#.
..........
)");
    }
}

TEST_CASE("Lines") {
    TenBySixDisplay d{};

    SECTION("Horisontal line") {
        hline(d.d, 0, 0, 10, true);
        REQUIRE(get_picture(d.buffer, d.displayWidth, d.displayHeight) == R"(
##########
..........
..........
..........
..........
..........
)");
    }

    SECTION("Horisontal line out of bounds - just does not print further") {
        hline(d.d, 0, 0, 1000, true);
        REQUIRE(get_picture(d.buffer, d.displayWidth, d.displayHeight) == R"(
##########
..........
..........
..........
..........
..........
)");
    }

    SECTION("Vertical line") {
        vline(d.d, 1, 1, 4, true);

        REQUIRE(get_picture(d.buffer, d.displayWidth, d.displayHeight) == R"(
..........
.#........
.#........
.#........
.#........
..........
)");
    }


    SECTION("Vertical line out of bounds - just does not print further") {
        vline(d.d, 1, 1, 400, true);

        REQUIRE(get_picture(d.buffer, d.displayWidth, d.displayHeight) == R"(
..........
.#........
.#........
.#........
.#........
.#........
)");
    }
}


TEST_CASE_METHOD(TenBySixDisplay, "Text, with some overflow") {
    SECTION("A") {
        print_text(d, 2, 0, "AA", true);

        REQUIRE(get_picture(buffer, displayWidth, displayHeight) == R"(
...##...##
..#..#.#..
..####.###
..#..#.#..
..#..#.#..
..#..#.#..
)");
    }
}