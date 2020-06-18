#ifndef EV3PLOTTER_DISPLAY_HEADER
#define EV3PLOTTER_DISPLAY_HEADER

#include <cstdint>
#include <bitset>
#include <vector>
#include <algorithm>
#include <cassert>

namespace ev3plotter
{
    struct point {
        int x;
        int y;
    };

    struct rect {
        point topLeft;
        point bottomRight;

        friend constexpr int width(const rect& r) noexcept {
            return r.bottomRight.x - r.topLeft.x + 1;
        }

        friend constexpr int height(const rect& r) noexcept {
            return r.bottomRight.y - r.topLeft.y + 1;
        }
    };

    struct display {
        display(unsigned char* buffer_, int width_, int height_) :
            buffer{buffer_}, width{width_ != 0 ? width_ : 100}, height{height_ != 0 ? height_ : 100}
        {
            assert(width_ >= 0);
            assert(height_ >= 0);
        }

        void set(point p, rect crop, bool val) {
            if (buffer) { // To allow for partial testing on WSL, where display is not working.
                if (p.x < std::max(crop.topLeft.x, 0) || p.x > std::min(width - 1, crop.bottomRight.x)) {
                    return;
                }
                if (p.y < std::max(crop.topLeft.y, 0) || p.y > std::min(height - 1, crop.bottomRight.y)) {
                    return;
                }

                const auto index = (p.y * width + p.x) * 4;
                const auto color =
                    static_cast<unsigned char>(val ? 0 : 0xff); // 0xff is used for empty pixels for some reason.

                assert(index >= 0);
                assert(index < (width * height * 4) - 3);

                buffer[index] = color;
                buffer[index + 1] = color;
                buffer[index + 2] = color;
                buffer[index + 3] = color;
            }
        }

        void set(point p, bool val) {
            set(p, {{0, 0}, {width-1, height-1}}, val);
        }

        void fill(bool val) {
            if (buffer) {
                std::fill(buffer, buffer + width * height * 4, val ? 0 : 0xff);
            }
        }

    private:
        unsigned char* buffer;

    public:
        const int width;
        const int height;
    };

    void fill(display &d, rect points, bool color) noexcept;
    void hline(display &d, point p, int length, bool color) noexcept;
    void vline(display &d, point p, int length, bool color) noexcept;
    void rectangle(display &d, rect points, bool color) noexcept;

    void print_text(display& d, point where, std::string_view text, bool color) noexcept;
    void print_text(display& d, point where, rect crop, std::string_view text, bool color) noexcept;
}

#endif // EV3PLOTTER_DISPLAY_HEADER