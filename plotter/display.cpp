#include "display.h"

namespace {
    struct arr_of_chars{
        template <std::uint32_t N>
        constexpr arr_of_chars(const char (&array)[N]) noexcept : ptr{array}, size{N - 1} {}
        constexpr char operator[](std::uint32_t i) const noexcept { return ptr[i]; }
        const char* ptr;

        const std::uint32_t size;
    };

    struct ch{
        constexpr ch(std::uint32_t p_width, arr_of_chars p_pixels) noexcept : width{p_width}, height{p_pixels.size / width}, pixels{p_pixels} {}

        constexpr bool operator()(std::uint32_t x, std::uint32_t y) const noexcept { return !(pixels[y*width + x] - 35); }
        std::uint32_t width;
        std::uint32_t height;
        arr_of_chars pixels;
    };

    constexpr ch A{4,{
        ".##."
        "#..#"
        "####"
        "#..#"
        "#..#"
        "#..#"
    }};

    constexpr ch chars_[] = {A};
}

void ev3plotter::print_text(ev3plotter::Display& d, std::uint32_t xpos, std::uint32_t ypos, std::string_view text, bool color) {
    for (auto c : text) {
        const auto& found_ch = chars_[c - 'A'];
        std::uint32_t cx = 0;
        std::uint32_t cy = 0;
        for (auto y = ypos; y != std::min(d.height, ypos + found_ch.height); ++y, ++cy) {
            cx = 0;
            for (auto x = xpos; x != std::min(d.width, xpos + found_ch.width); ++x, ++cx) {
                if (found_ch(cx, cy)) {
                    d.set(x, y, color);
                }
            }
        }

        xpos += found_ch.width + 1;
    }
}