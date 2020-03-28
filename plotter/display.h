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
            return r.bottomRight.y - r.topLeft.y + 1~;
        }
    };

    struct display {
        display(unsigned char* buffer_, int width_, int height_) :
            buffer{buffer_}, width{width_}, height{height_}, m_tempBuffer(static_cast<std::size_t>(width_*height_*4), 0xff)
        {
            assert(width_ > 0);
            assert(height_ > 0);
        }

        void set(point p, rect crop, bool val) {
            // if (p.x < std::max(crop.topLeft.x, 0) || p.x >= std::min(width, crop.bottomRight.x)) { return; }
            // if (p.y < std::max(crop.topLeft.y, 0) || p.y >= std::min(height, crop.bottomRight.y)) { return; }


            if (p.x < std::max(crop.topLeft.x, 0) || p.x > std::min(width-1, crop.bottomRight.x)) { return; }
            if (p.y < std::max(crop.topLeft.y, 0) || p.y > std::min(height-1, crop.bottomRight.y)) { return; }

            const auto index = (p.y * width + p.x) * 4;
            const auto color = static_cast<unsigned char>(val ? 0 : 0xff); // 0xff is used for empty pixels for some reason.
            // m_tempBuffer[index] = color;
            // m_tempBuffer[index+1] = color;
            // m_tempBuffer[index+2] = color;
            // m_tempBuffer[index+3] = color;

            assert(index >= 0);
            assert(index < (width*height*4) - 3);

            buffer[index] = color;
            buffer[index+1] = color;
            buffer[index+2] = color;
            buffer[index+3] = color;
        }

        void set(point p, bool val) {
            set(p, {{0, 0}, {width-1, height-1}}, val);
        }

        void fill(bool val) {
            std::fill(buffer, buffer + width*height*4, val ? 0 : 0xff);
        }

        void draw() const {
           // std::copy(m_tempBuffer.begin(), m_tempBuffer.end(), buffer);
        }

        unsigned char* buffer;
        int width;
        int height;

        std::vector<unsigned char> m_tempBuffer;
    };

    inline void fill(display& d, rect points, bool color) {
        for (auto px = std::max(points.topLeft.x, 0); px != std::min(d.width, points.bottomRight.x + 1); ++px) {
            for (auto py = std::max(points.topLeft.y, 0); py != std::min(d.height, points.bottomRight.y + 1); ++py) {
                d.set({px, py}, color);
            }
        }
    }

    inline void hline(display& d, point p, int length, bool color) {
        for (auto px = std::max(0, p.x); px != std::min(d.width, p.x + length); ++px) {
            d.set({px, p.y}, color);
        }
    }

    inline void vline(display& d, point p, int length, bool color) {
        for (auto py = std::max(0, p.y); py != std::min(d.height, p.y + length); ++py) {
            d.set({p.x, py}, color);
        }
    }

    inline void rectangle(display& d, rect points, bool color) {
        const auto w = width(points);
        const auto h = height(points);
        hline(d, points.topLeft, w, color);
        hline(d, {points.topLeft.x, points.bottomRight.y}, w, color);
        vline(d, {points.topLeft.x, points.topLeft.y + 1}, h - 2, color);
        vline(d, {points.bottomRight.x, points.topLeft.y + 1}, h - 2, color);
    }

    void print_text(display& d, point where, std::string_view text, bool color);
    void print_text(display& d, point where, rect crop, std::string_view text, bool color);
}

#endif // EV3PLOTTER_DISPLAY_HEADER