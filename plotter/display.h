#ifndef EV3PLOTTER_DISPLAY_HEADER
#define EV3PLOTTER_DISPLAY_HEADER

#include <cstdint>
#include <bitset>
#include <vector>
#include <algorithm>

namespace ev3plotter
{
    struct Display {
        Display(unsigned char* buffer_, std::uint32_t width_, std::uint32_t height_) :
            buffer{buffer_}, width{width_}, height{height_}, m_tempBuffer(width_*height_*4, 0xff) {}

        void set(std::uint32_t x, std::uint32_t y, bool val) {
            const auto index = (y * width + x) * 4;
            const auto color = static_cast<unsigned char>(val ? 0 : 0xff); // 0xff is used for empty pixels for some reason.
            // m_tempBuffer[index] = color;
            // m_tempBuffer[index+1] = color;
            // m_tempBuffer[index+2] = color;
            // m_tempBuffer[index+3] = color;

            buffer[index] = color;
            buffer[index+1] = color;
            buffer[index+2] = color;
            buffer[index+3] = color;
        }

        void fill(bool val) {
            std::fill(buffer, buffer + width*height*4, val ? 0 : 0xff);
        }

        void draw() const {
           // std::copy(m_tempBuffer.begin(), m_tempBuffer.end(), buffer);
        }

        unsigned char* buffer;
        std::uint32_t width;
        std::uint32_t height;

        std::vector<unsigned char> m_tempBuffer;
    };

    inline void rectangle(Display& d, std::uint32_t x, std::uint32_t y, std::uint32_t width, std::uint32_t height, bool color) {
        for (auto px = x; px != std::min(d.width, x + width); ++px) {
            for (auto py = y; py != std::min(d.height, y + height); ++py) {
                d.set(px, py, color);
            }
        }
    }

    inline void hline(Display& d, std::uint32_t x, std::uint32_t y, std::uint32_t length, bool color) {
        for (auto px = x; px != std::min(d.width, x + length); ++px) {
            d.set(px, y, color);
        }
    }

    inline void vline(Display& d, std::uint32_t x, std::uint32_t y, std::uint32_t length, bool color) {
        for (auto py = y; py != std::min(d.height, y + length); ++py) {
            d.set(x, py, color);
        }
    }

    void print_text(Display& d, std::uint32_t x, std::uint32_t y, std::string_view text, bool color);
}

#endif // EV3PLOTTER_DISPLAY_HEADER