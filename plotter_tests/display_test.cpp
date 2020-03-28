#include <display.h>
#include <catch2.hpp>
#include <bitset>
#include <algorithm>

using namespace ev3plotter;

namespace {
    bool get_pixel(const unsigned char* buffer, std::uint32_t x, std::uint32_t y, std::uint32_t displayWidth, std::uint32_t bytesPerPixel = 4) {
        const auto baseIndex = (y*displayWidth + x) * bytesPerPixel;
        for (auto p = buffer + baseIndex; p != buffer + baseIndex + bytesPerPixel; ++p) {
            if (*p != 0xff) {
                return true;
            }
        }

        return false;
    }

    template <int W, int H>
    struct MockDispay{
        static constexpr int displayWidth = W;
        static constexpr int displayHeight = H;

        MockDispay() {
            std::fill(std::begin(buffer), std::end(buffer), 0xff);
        }

        unsigned char buffer[displayWidth*displayHeight*4];
        display d{buffer, displayWidth, displayHeight};
    };

    using TenBySixDisplay = MockDispay<10, 6>;

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

    template <int W, int H>
    auto get_picture(const MockDispay<W, H>& display) {
        return get_picture(display.buffer, W, H);
    }

    std::string mirror_transpose(std::size_t pictureWidth, std::string picture) {
        REQUIRE((picture.size() - 1) % (pictureWidth + 1) == 0);
        REQUIRE(picture.front() == '\n');
        REQUIRE(picture.back() == '\n');

        const auto height = (picture.size() - 1) / (pictureWidth + 1);
        auto get{[](auto& pic, auto w, auto x, auto y) -> char & {
            return pic[1 + y * (w + 1) + x];
        }};

        std::string transposed(1 + (height + 1)* pictureWidth, '\n');
        for (std::size_t x = 0; x != pictureWidth; ++x) {
            for (std::size_t y = 0; y != height; ++y) {
                get(transposed, height, y, x) =  get(picture, pictureWidth, pictureWidth - x - 1, y);
            }
        }

        return transposed;
    }
}

TEST_CASE("Test transpose itself") {
    REQUIRE(mirror_transpose(5, R"(
12345
abcde
)") == R"(
5e
4d
3c
2b
1a
)");

//     REQUIRE(mirror_transpose(5, R"(
// 12345
// abcde
// )") == R"(
// 1a
// 2b
// 3c
// 4d
// 5e
// )");
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

        REQUIRE(get_pixel(c, 0, 0, 8, 3) == true);
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

TEST_CASE("display properly puts pixels into buffer") {
    constexpr std::uint32_t displayWidth = 4;
    constexpr std::uint32_t displayHeight = 2;

    unsigned char buffer[displayWidth*displayHeight*4];
    std::fill(std::begin(buffer), std::end(buffer), 0xff);

    display d{buffer, displayWidth, displayHeight};

    d.set({0, 0}, true);
    d.set({1, 0}, true);
    d.set({3, 1}, true);

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


TEST_CASE_METHOD(TenBySixDisplay, "set() method tests") {
    constexpr rect rectOutsideTheScreen{{-1, -1},{displayWidth + 1, displayHeight + 1}};
    constexpr char emptyScreen[] = R"(
..........
..........
..........
..........
..........
..........
)";

    SECTION("set outside the screen with rect larger than screen")
    {
        SECTION("x < 0") {
            d.set({-1, 0}, rectOutsideTheScreen, true);
            REQUIRE(get_picture(*this) == emptyScreen);
        }

        SECTION("y < 0") {
            d.set({0, -1}, rectOutsideTheScreen, true);
            REQUIRE(get_picture(*this) == emptyScreen);
        }

        SECTION("x > width") {
            d.set({displayWidth, 0}, rectOutsideTheScreen, true);
            REQUIRE(get_picture(*this) == emptyScreen);
        }

        SECTION("y > height") {
            d.set({0, displayHeight}, rectOutsideTheScreen, true);
            REQUIRE(get_picture(*this) == emptyScreen);
        }
    }

    SECTION("set on the corners") {
        d.set({0, 0}, true);
        d.set({displayWidth-1, 0}, true);
        d.set({0, displayHeight-1}, true);
        d.set({displayWidth-1, displayHeight-1}, true);

        REQUIRE(get_picture(*this) == R"(
#........#
..........
..........
..........
..........
#........#
)");
    }
}

TEST_CASE("Rectangle tests") {
    TenBySixDisplay d{};

    SECTION("Around the border") {
        rectangle(d.d, {{0, 0}, {d.displayWidth - 1, d.displayHeight - 1}}, true);
        REQUIRE(get_picture(d) == R"(
##########
#........#
#........#
#........#
#........#
##########
)");
    }
}

TEST_CASE("Fill") {
    TenBySixDisplay d{};

    SECTION("Full fill") {
        fill(d.d, {{0, 0}, {d.displayWidth-1, d.displayHeight-1}}, true);
        REQUIRE(get_picture(d) == R"(
##########
##########
##########
##########
##########
##########
)");
    }

    SECTION("Border -1 offset") {
        fill(d.d, {{1, 1}, {d.displayWidth-2, d.displayHeight-2}}, true);
        REQUIRE(get_picture(d) == R"(
..........
.########.
.########.
.########.
.########.
..........
)");
    }

    SECTION("Border -1 offset + rectangle") {
        fill(d.d, {{1, 1}, {d.displayWidth-2, d.displayHeight-2}}, true);
        fill(d.d, {{4, 3}, {7, 5}}, false);

        REQUIRE(get_picture(d) == R"(
..........
.########.
.########.
.###....#.
.###....#.
..........
)");
    }

    SECTION("Outside of the screen x axis") {
        fill(d.d, {{-1, 1}, {d.displayWidth+1, d.displayHeight-2}}, true);

        REQUIRE(get_picture(d) == R"(
..........
##########
##########
##########
##########
..........
)");
    }

    SECTION("Outside of the screen y axis") {
        fill(d.d, {{1, -1}, {d.displayWidth-2, d.displayHeight+1}}, true);

        REQUIRE(get_picture(d) == R"(
.########.
.########.
.########.
.########.
.########.
.########.
)");
    }
}

TEST_CASE("Lines") {
    TenBySixDisplay d{};

    SECTION("Horisontal line") {
        hline(d.d, {0, 0}, 10, true);
        REQUIRE(get_picture(d) == R"(
##########
..........
..........
..........
..........
..........
)");
    }

    SECTION("Horisontal line out of bounds - just does not print further") {
        hline(d.d, {-2, 2}, 1000, true);
        REQUIRE(get_picture(d) == R"(
..........
..........
##########
..........
..........
..........
)");
    }

    SECTION("Vertical line") {
        vline(d.d, {1, 1}, 4, true);

        REQUIRE(get_picture(d) == R"(
..........
.#........
.#........
.#........
.#........
..........
)");
    }


    SECTION("Vertical line out of bounds - just does not print further") {
        vline(d.d, {1, -1}, 400, true);

        REQUIRE(get_picture(d) == R"(
.#........
.#........
.#........
.#........
.#........
.#........
)");
    }

    SECTION("Vertical line outside the screen - not visible") {
        vline(d.d, {-1, -1}, 400, true);

        REQUIRE(get_picture(d) == R"(
..........
..........
..........
..........
..........
..........
)");
    }
}


TEST_CASE_METHOD((MockDispay<270, 14>), "Text (all capitals)") {
    print_text(d, {2, 12}, "ABCDEFGHIJKLMNOPQRSTUVWXYZ", true);
    d.set({0, 12}, true);

    REQUIRE(get_picture(buffer, displayWidth, displayHeight) == R"(
..............................................................................................................................................................................................................................................................................
....###...####.........##....#####....#######..#######....#####...#.....#..###........#.#.....#..#........#.......#..#.....#......##......#####........##......#####......#####...#######..#.....#..#.......#..#.......#..#.....#..#.....#..#######...........................
...#...#..#...#......##..##..#....#...#........#.........#.....#..#.....#...#.........#.#.....#..#........##.....##..##....#....##..##....#....#.....##..##....#....#....#.....#.....#.....#.....#..#.......#..#.......#...#...#...#.....#........#...........................
...#...#..#....#....#......#.#.....#..#........#.........#......#.#.....#...#.........#.#....#...#........#.#...#.#..##....#...#......#...#.....#...#......#...#.....#..#............#.....#.....#..#.......#..#.......#...#...#...#.....#.......#............................
...#...#..#....#....#........#.....#..#........#........#.........#.....#...#.........#.#....#...#........#.#...#.#..#.#...#...#......#...#.....#...#......#...#.....#..#............#.....#.....#...#.....#...#...#...#....#.#.....#...#........#............................
...#...#..#...#....#.........#.....#..#........#........#.........#.....#...#.........#.#...#....#........#.#...#.#..#.#...#..#........#..#.....#..#........#..#.....#...#...........#.....#.....#...#.....#...#...#...#....#.#.....#...#.......#.............................
..#.....#.#####....#.........#.....#..#........#........#.........#.....#...#.........#.#...#....#........#..#.#..#..#..#..#..#........#..#....#...#........#..#....#.....####.......#.....#.....#...#.....#....#..#..#......#.......#.#.......#..............................
..#.....#.#....#...#.........#.....#..####.....####.....#.........#######...#.........#.####.....#........#...#...#..#..#..#..#........#..#####....#........#..#####..........#......#.....#.....#....#...#.....#..#..#......#.......#.#.......#..............................
..#.....#.#.....#..#.........#.....#..#........#........#.....###.#.....#...#.........#.#...#....#........#.......#..#...#.#..#........#..#........#........#..#..#............#.....#.....#.....#....#...#.....#.#.#.#.....#.#.......#.......#...............................
..#######.#.....#...#........#.....#..#........#........#.......#.#.....#...#.........#.#...#....#........#.......#..#...#.#...#......#...#.........#....#.#...#...#...........#.....#.....#.....#....#...#.....#.#.#.#.....#.#.......#......#................................
..#.....#.#.....#...#......#.#.....#..#........#.........#......#.#.....#...#...#....#..#....#...#........#.......#..#....##...#......#...#.........#.....##...#....#..........#.....#.....#.....#.....#.#......#.#.#.#....#...#.....#.......#................................
..#.....#.#....#.....##..##..#....#...#........#.........#.....#..#.....#...#...#....#..#....#...#........#.......#..#....##....##..##....#..........##..###...#.....#..#.....#......#......#...#......#.#......#.#.#.#....#...#.....#......#.................................
#.#.....#.#####........##....#####....#######..#..........#####...#.....#..###...####...#.....#..#######..#.......#..#.....#......##......#............##...#..#.....#...#####.......#.......###........#........#...#....#.....#..#........#######...........................
..............................................................................................................................................................................................................................................................................
)");
}

TEST_CASE_METHOD((MockDispay<4, 14>), "Just l") {
    print_text(d, {2, 12}, "l", true);
    d.set({0, 12}, true);

    REQUIRE(get_picture(buffer, displayWidth, displayHeight) == R"(
....
..#.
..#.
..#.
..#.
..#.
..#.
..#.
..#.
..#.
..#.
..#.
#.#.
....
)");
}

TEST_CASE_METHOD((MockDispay<200, 20>), "Text (all small ones)") {
    print_text(d, {2, 12}, "abcdefghijklmnopqrstuvwxyz", true);
    d.set({0, 12}, true);

    REQUIRE(mirror_transpose(displayWidth, get_picture(buffer, displayWidth, displayHeight)) == mirror_transpose(displayWidth, R"(
.........................................................................#..............................................................................................................................
..........#....................#............###..........#...............#..#.......#...............................................................#...................................................
..........#....................#...........#...#.........#...............#..#.......#...............................................................#...................................................
..........#....................#...........#...#.........#.......#.......#..#.......#...............................................................#...................................................
..........#....................#...........#.............#...............#..#.......#...............................................................#...................................................
...#####..#####.....###....#####....###...###.....####...#####...#.......#..#....#..#...#.#.##...#####.....###....#.###....###.#..#.###....#####..#####...#....#..#.....#..#.....#..#...#..#.....#.#####
..#....#..#....#...#...#..#....#...#...#...#.....#....#..#....#..#.......#..#...#...#...##.#..#..#....#...#...#...##...#..#...##..##...#..#.........#.....#....#..#.....#..#.....#..#...#..#.....#......
..#....#..#....#..#.......#....#..#....#...#.....#....#..#....#..#.......#..#..#....#...#..#..#..#....#..#.....#..#....#..#....#..#.......#.........#.....#....#...#...#...#.....#...#.#....#....#.....#
..#....#..#....#..#.......#....#..#....#...#.....#....#..#....#..#.......#..###.....#...#..#..#..#....#..#.....#..#....#..#....#..#........####.....#.....#....#...#...#....#.#.#.....#.....#....#....#.
..#....#..#....#..#.......#....#..#####....#.....#....#..#....#..#.......#..#..#....#...#..#..#..#....#..#.....#..#....#..#....#..#............#....#.....#....#....#.#.....#.#.#....#.#.....#..#....#..
..#....#..#....#..#.......#....#..#........#......####...#....#..#.......#..#...#...#...#..#..#..#....#..#.....#..#....#..#....#..#............#....#.....#....#....#.#.....#.#.#....#.#.....#..#...#...
..#....#..#....#...#...#..#....#...#...#...#.....#.......#....#..#...#..#...#....#..#...#..#..#..#....#...#...#...##...#..#...##..#............#....#.....#....#.....#.......#.#....#...#.....#.#..#....
#..####.#.#####.....###....####.#...###....#.....##......#....#...#...##....#....#..#...#..#..#..#....#....###....#.###....###.#..#.......#####......##....####......#.......#.#....#...#......#...#####
..................................................####............................................................#............#...............................................................#........
.................................................#....#...........................................................#............#...............................................................#........
.................................................#....#...........................................................#............##..........................................................#..#.........
..................................................####............................................................#............#............................................................##..........
........................................................................................................................................................................................................
........................................................................................................................................................................................................
........................................................................................................................................................................................................
)"));
}

TEST_CASE_METHOD((MockDispay<10, 10>), "Text - cropped by display itself") {

    SECTION("Offset with x < 0") {
        print_text(d, {-2, 8}, "a", true);

        REQUIRE(get_picture(*this) == R"(
..........
####......
...#......
...#......
...#......
...#......
...#......
...#......
###.#.....
..........
)");
    }

    SECTION("Offset with y < 0") {
        print_text(d, {1, 6}, "a", true);

        REQUIRE(get_picture(*this) == R"(
.#....#...
.#....#...
.#....#...
.#....#...
.#....#...
.#....#...
..####.#..
..........
..........
..........
)");
    }

    SECTION("Offset with x > width") {
        print_text(d, {5, 8}, "a", true);

        REQUIRE(get_picture(*this) == R"(
..........
......####
.....#....
.....#....
.....#....
.....#....
.....#....
.....#....
......####
..........
)");
    }

    SECTION("Offset with y > height") {
        print_text(d, {1, 12}, "a", true);

        REQUIRE(get_picture(*this) == R"(
..........
..........
..........
..........
..........
..#####...
.#....#...
.#....#...
.#....#...
.#....#...
)");
    }
}

TEST_CASE_METHOD((MockDispay<50, 17>), "Text - non characters") {
    print_text(d, {2, 13}, "[`Aa]\\", true);
    d.set({0, 13}, true);

    REQUIRE(get_picture(buffer, displayWidth, displayHeight) == R"(
..................................................
..###..#....................###..#................
..#....#......###.............#..#................
..#.....#....#...#............#..#................
..#......#...#...#............#...#...............
..#..........#...#............#...#...............
..#..........#...#...#####....#...#...............
..#.........#.....#.#....#....#....#..............
..#.........#.....#.#....#....#....#..............
..#.........#.....#.#....#....#....#..............
..#.........#######.#....#....#.....#.............
..#.........#.....#.#....#....#.....#.............
..#.........#.....#.#....#....#.....#.............
#.#.........#.....#..####.#...#......#............
..#...........................#......#............
..###.......................###...................
..................................................
)");
}

TEST_CASE_METHOD((MockDispay<100, 17>), "Digits") {
    print_text(d, {2, 13}, "0123456789", true);
    d.set({0, 13}, true);

    REQUIRE(mirror_transpose(displayWidth, get_picture(buffer, displayWidth, displayHeight)) == mirror_transpose(displayWidth, R"(
....................................................................................................
....................................................................................................
....###......#....###......###........#...#######.....##....#######....###......###.................
...#...#....##...#...#....#...#......##...#.........##............#...#...#....#...#................
..#...#.#..#.#..#.....#..#.....#....#.#...#........#..............#..#.....#..#.....#...............
..#...#.#....#........#........#....#.#...#........#..............#..#.....#..#.....#...............
..#..#..#....#.......#........#....#..#....####....#.............#....#...#...#.....#...............
..#..#..#....#......#........#.....#..#........#...#.###.........#.....###.....#...##...............
..#..#..#....#.....#..........#...#...#.........#..##...#.......#.....#...#.....###.#...............
..#..#..#....#....#............#..#...#.........#..#.....#......#....#.....#........#...............
..#.#...#....#...#.............#..######........#..#.....#.....#.....#.....#........#...............
..#.#...#....#...#.......#.....#......#...#.....#..#.....#.....#.....#.....#........#...............
...#...#.....#..#.........#...#.......#....#...#....#...#.....#.......#...#.......##................
#...###......#..#######....###........#.....###......###......#........###......##..................
....................................................................................................
....................................................................................................
....................................................................................................
)"));
}

TEST_CASE_METHOD((MockDispay<85, 17>), "Symbols before digits") {
    print_text(d, {2, 13}, "! !\"#$%&'-./", true);
    d.set({0, 13}, true);

    //REQUIRE(mirror_transpose(displayWidth, get_picture(buffer, displayWidth, displayHeight)) == mirror_transpose(displayWidth, R"(
    REQUIRE(get_picture(buffer, displayWidth, displayHeight) == R"(
...................................#.................................................
...............#.#.................#............#...............#.................#..
..#........#...#.#.....#..#......#####....##....#.....###.......#.................#..
..#........#...#.#.....#..#.....#.....#..#..#..#.....#...#......#.................#..
..#........#...#.#.....#..#....#.........#..#..#.....#...#......#................#...
..#........#..#.#......#..#....#..........##..#......#..#......#.................#...
..#........#.........########...#.............#.......#.#........................#...
..#........#...........#..#......####........#.........#........................#....
..#........#..........#..#...........#.......#........##........................#....
..#........#..........#..#............#.....#........#..#..#.......#####........#....
..#........#..........#..#............#.....#..##...#....#.#...................#.....
..#........#........########..........#....#..#..#..#.....#....................#.....
......................#..#.....#.....#.....#..#..#..#....#.#..............##...#.....
#.#........#..........#..#......#####.....#....##....####...#.............##..#......
..................................#.......#...................................#......
..................................#..................................................
.....................................................................................
)");
}

TEST_CASE_METHOD((MockDispay<60, 17>), "Symbols after digits") {
    print_text(d, {2, 13}, ":;<=>?@", true);
    d.set({0, 13}, true);

    //REQUIRE(mirror_transpose(displayWidth, get_picture(buffer, displayWidth, displayHeight)) == mirror_transpose(displayWidth, R"(
    REQUIRE(get_picture(buffer, displayWidth, displayHeight) == R"(
............................................................
............................................................
...................................###.......#####..........
..................................#...#.....#.....#.........
.................................#.....#...#.......#........
..##..##......#...........#............#..#...##.#..#.......
..##..##.....#.............#...........#..#..#..##..#.......
............#....#######....#.........#...#..#...#..#.......
...........#.................#.......#....#..#...#..#.......
..........#...................#.....#.....#..#...#..#.......
...........#.................#......#.....#..#..##..#.......
............#....#######....#.......#.....#...##.#.#........
..##..##.....#.............#...............#......#.........
#.##..##......#...........#.........#.......#...............
.......#.....................................###............
......#.........................................###.........
............................................................
)");
}

TEST_CASE_METHOD((MockDispay<50, 17>), "Unknown symbols replaced with questionmarks") {
    std::string text;
    text.push_back('\x00');
    text.push_back('\x01');
    text.push_back('\x02');
    text.push_back('\x7f');
    text.push_back('\xff');
    print_text(d, {2, 13}, text, true);
    d.set({0, 13}, true);

    //REQUIRE(mirror_transpose(displayWidth, get_picture(buffer, displayWidth, displayHeight)) == mirror_transpose(displayWidth, R"(
    REQUIRE(get_picture(buffer, displayWidth, displayHeight) == R"(
..................................................
..................................................
....###......###......###......###......###.......
...#...#....#...#....#...#....#...#....#...#......
..#.....#..#.....#..#.....#..#.....#..#.....#.....
........#........#........#........#........#.....
........#........#........#........#........#.....
.......#........#........#........#........#......
......#........#........#........#........#.......
.....#........#........#........#........#........
.....#........#........#........#........#........
.....#........#........#........#........#........
..................................................
#....#........#........#........#........#........
..................................................
..................................................
..................................................
)");
}

TEST_CASE_METHOD((MockDispay<30, 17>), "Symbols after chars") {
    print_text(d, {2, 13}, "{|}~", true);
    d.set({0, 13}, true);

    //REQUIRE(mirror_transpose(displayWidth, get_picture(buffer, displayWidth, displayHeight)) == mirror_transpose(displayWidth, R"(
    REQUIRE(get_picture(buffer, displayWidth, displayHeight) == R"(
..............................
....##..#..##.................
...#....#....#................
...#....#....#....##..........
...#....#....#...#..#...#.....
...#....#....#...#...#..#.....
...#....#....#........##......
...#....#....#................
..#.....#.....#...............
...#....#....#................
...#....#....#................
...#....#....#................
...#....#....#................
#..#....#....#................
...#....#....#................
....##..#..##.................
..............................
)");
}

TEST_CASE_METHOD((MockDispay<29, 16>), "Chars can be cropped to any size") {
    print_text(d, point{2, 13}, rect{{3, 3}, {24, 13}}, "{|}~", true);

    //REQUIRE(mirror_transpose(displayWidth, get_picture(buffer, displayWidth, displayHeight)) == mirror_transpose(displayWidth, R"(
    REQUIRE(get_picture(buffer, displayWidth, displayHeight) == R"(
.............................
.............................
.............................
...#....#....#....##.........
...#....#....#...#..#........
...#....#....#...#...#.......
...#....#....#........##.....
...#....#....#...............
........#.....#..............
...#....#....#...............
...#....#....#...............
...#....#....#...............
...#....#....#...............
.............................
.............................
.............................
)");
}