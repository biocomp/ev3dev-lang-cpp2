#include "display.h"

namespace {
    struct arr_of_chars{
        template <std::size_t N>
        constexpr arr_of_chars(const char (&array)[N]) noexcept : ptr_{array}, size_{N - 1} {}
        constexpr char operator[](std::uint32_t i) const noexcept { return ptr_[i]; }

        constexpr const char* begin() noexcept { return ptr_; }
        constexpr const char* end() noexcept { return ptr_ + size_; }

        constexpr std::size_t size() const noexcept { return size_; }
    private:
        const char* ptr_;
        std::size_t size_;
    };

    template <typename T>
    struct arr{
        template <std::size_t N>
        constexpr arr(const T (&array)[N]) noexcept : ptr_{array}, size_{N - 1} {}
        constexpr const T& operator[](std::uint32_t i) const noexcept { return ptr_[i]; }

        constexpr const T* begin() noexcept { return ptr_; }
        constexpr const T* end() noexcept { return ptr_ + size_; }

        constexpr std::size_t size() const noexcept { return size_; }
    private:
        const T* ptr_;
        std::size_t size_;
    };


    struct point {
        std::uint8_t x;
        std::uint8_t y;
    };

    template <typename T, std::size_t MaxSize>
    struct static_vector{
        static_assert(std::is_trivial_v<T>, "T must be trivial");

        constexpr static_vector() noexcept : buf_{}, size_{0} {}

        constexpr void push_back(const T &val) { buf_[size_++] = val; };
        constexpr std::size_t size() const noexcept { return size_; }
        constexpr T& operator[](std::size_t i) noexcept { return buf_[i]; }
        constexpr const T& operator[] (std::size_t i)const  noexcept { return buf_[i]; }

        constexpr const T* begin() const noexcept { return buf_; }
        constexpr const T* end() const noexcept { return buf_ + size_; }

    private:
        T buf_[MaxSize];
        std::size_t size_;
    };

    struct parsed_glyph {
        point topLeft;
        point bottomRight;
        std::uint8_t advance;
        point origin;
        static_vector<point, 40> glyph_path;
    };

    constexpr void ensure(bool condition) {
        if (!condition) {
            throw std::exception{};
        }
    }

    constexpr parsed_glyph parse_glyph(std::uint8_t glyphWidth, arr_of_chars glyph) noexcept {
        ensure(glyph.size() % glyphWidth == 0);
        const auto glyphHeight = static_cast<std::uint8_t>(glyph.size()) / glyphWidth;

        constexpr std::uint8_t c_max = std::numeric_limits<std::uint8_t>::max();
        constexpr std::uint8_t c_min = std::numeric_limits<std::uint8_t>::min();

        std::uint8_t x[]{c_max, c_min};
        std::uint8_t y[]{c_max, c_min};

        std::uint8_t originXy[]{c_max, c_max};

        std::uint8_t advance{0};

        const auto emptyPixel{[](char p) { return p == ' ' || p == '.'; }};
        const auto originPixel{[](char p) { return p == '.' || p == '*'; }};

        decltype(parsed_glyph::glyph_path) path{};

        for (std::uint8_t line = 0; line != glyphHeight; ++line) {
            for (std::uint8_t col = 0; col != glyphWidth; ++col) {
                const auto p = glyph[static_cast<std::uint32_t>(line * glyphWidth + col)];
                if (!emptyPixel(p)) {
                    if (col < x[0]) {
                        x[0] = col;
                    } else if (col > x[1]) {
                        x[1] = col;
                    }

                    if (line < y[0]) {
                        y[0] = line;
                    } else if (line > y[1]) {
                        y[1] = line;
                    }

                    path.push_back({col, line});
                }

                if (originPixel(p)) {
                    if (originXy[0] == c_max) {
                        originXy[0] = col;
                        originXy[1] = line;
                    } else {
                        advance = static_cast<std::uint8_t>(col - originXy[0] + 1);
                    }
                }
            }
        }

        ensure(x[0] != c_max);
        ensure(x[1] != c_min);
        ensure(y[0] != c_max);
        ensure(y[1] != c_min);
        ensure(originXy[0] != c_max);
        ensure(originXy[1] != c_max);
        ensure(advance != 0);

        return { {x[0], y[0]}, {x[1], y[1]}, advance, {originXy[0], originXy[1]}, path };
    }

    namespace test
    {
        constexpr char test_char[] =
            "  ###  "
            " #   # "
            " #   # "
            ". ####."
            "     # "
            "     # "
            "     ##";

        constexpr auto parse_glyph_test = parse_glyph(7, test_char);

        static_assert(parse_glyph_test.topLeft.x == 1, "topLeft.x");
        static_assert(parse_glyph_test.topLeft.y == 0, "topLeft.y");
        static_assert(parse_glyph_test.bottomRight.x == 6, "bottomRight.x");
        static_assert(parse_glyph_test.bottomRight.y == 6, "bottomRight.y");

        static_assert(parse_glyph_test.origin.x == 0, "origin.x");
        static_assert(parse_glyph_test.origin.y == 3, "origin.y");

        static_assert(parse_glyph_test.advance == 7, "advance");
    }

    namespace test2
    {
        constexpr char test_char2[] =
            " # "
            ".#.";

        constexpr auto parse_glyph_test2 = parse_glyph(3, test_char2);
        static_assert(parse_glyph_test2.topLeft.x == 1, "toplLeft2.x");
        static_assert(parse_glyph_test2.topLeft.y == 0, "toplLeft2.y");
        static_assert(parse_glyph_test2.bottomRight.x == 1, "bottomRight2.x");
        static_assert(parse_glyph_test2.bottomRight.y == 1, "bottomRight2.y");
        static_assert(parse_glyph_test2.advance == 3, "advance2");
        static_assert(parse_glyph_test2.glyph_path[0].x == 1, "path2[0].x");
        static_assert(parse_glyph_test2.glyph_path[0].y == 0, "path2[0].y");
        static_assert(parse_glyph_test2.glyph_path[1].x == 1, "path2[1].x");
        static_assert(parse_glyph_test2.glyph_path[1].y == 1, "path2[1].y");
        static_assert(parse_glyph_test2.glyph_path.size() == 2, "path2.size()");
        } // namespace test2

    struct ch{
        constexpr ch(std::uint8_t glyphWidth, arr_of_chars glyph) noexcept : glyph_{parse_glyph(glyphWidth, glyph)} {}

        constexpr std::uint8_t width() const noexcept { return static_cast<std::uint8_t>(glyph_.bottomRight.x - glyph_.topLeft.x); }
        constexpr std::uint8_t height() const noexcept { return static_cast<std::uint8_t>(glyph_.bottomRight.y - glyph_.topLeft.y); }

        constexpr const auto& glyph() const noexcept { return glyph_; }

    private:
        parsed_glyph glyph_;
    };

    constexpr ch A{8,{
        "  ###   "
        " #   #  "
        " #   #  "
        " #   #  "
        " #   #  "
        "#     # "
        "#     # "
        "#     # "
        "####### "
        "#     # "
        "#     # "
        "*     #."
    }};

    static_assert(A.glyph().origin.x == 0, "A.origin.x");
    static_assert(A.glyph().origin.y == 11, "A.origin.y");

    constexpr ch B{9,{
        " ####    "
        " #   #   "
        " #    #  "
        " #    #  "
        " #   #   "
        " #####   "
        " #    #  "
        " #     # "
        " #     # "
        " #     # "
        " #    #  "
        ".#####  ."
    }};

    constexpr ch C{10,{
        "    ##    "
        "  ##  ##  "
        " #      # "
        " #        "
        "#         "
        "#         "
        "#         "
        "#         "
        " #        "
        " #      # "
        "  ##  ##  "
        ".   ##   ."
    }};

    constexpr ch D{9,{
        " #####   "
        " #    #  "
        " #     # "
        " #     # "
        " #     # "
        " #     # "
        " #     # "
        " #     # "
        " #     # "
        " #     # "
        " #    #  "
        ".#####  ."
    }};

    constexpr ch E{9,{
        " ####### "
        " #       "
        " #       "
        " #       "
        " #       "
        " #       "
        " ####    "
        " #       "
        " #       "
        " #       "
        " #       "
        ".#######."
    }};

    constexpr ch F{9,{
        " ####### "
        " #       "
        " #       "
        " #       "
        " #       "
        " #       "
        " ####    "
        " #       "
        " #       "
        " #       "
        " #       "
        ".#      ."
    }};

    constexpr ch G{10,{
        "  #####   "
        " #     #  "
        " #      # "
        "#         "
        "#         "
        "#         "
        "#         "
        "#     ### "
        "#       # "
        " #      # "
        " #     #  "
        ". #####  ."
    }};

    constexpr ch H{9,{
        " #     # "
        " #     # "
        " #     # "
        " #     # "
        " #     # "
        " #     # "
        " ####### "
        " #     # "
        " #     # "
        " #     # "
        " #     # "
        ".#     #."
    }};

    constexpr ch I{5,{
        " ### "
        "  #  "
        "  #  "
        "  #  "
        "  #  "
        "  #  "
        "  #  "
        "  #  "
        "  #  "
        "  #  "
        "  #  "
        ".###."
    }};

    constexpr ch J{8,{
        "      # "
        "      # "
        "      # "
        "      # "
        "      # "
        "      # "
        "      # "
        "      # "
        "      # "
        "#    #  "
        "#    #  "
        ".####  ."
    }};

    constexpr ch K{9,{
        " #     # "
        " #     # "
        " #    #  "
        " #    #  "
        " #   #   "
        " #   #   "
        " ####    "
        " #   #   "
        " #   #   "
        " #    #  "
        " #    #  "
        ".#     #."
    }};

    constexpr ch L{9,{
        " #       "
        " #       "
        " #       "
        " #       "
        " #       "
        " #       "
        " #       "
        " #       "
        " #       "
        " #       "
        " #       "
        ".#######."
    }};

    constexpr ch M{11,{
        " #       # "
        " ##     ## "
        " # #   # # "
        " # #   # # "
        " # #   # # "
        " #  # #  # "
        " #   #   # "
        " #       # "
        " #       # "
        " #       # "
        " #       # "
        ".#       #."
    }};

    constexpr ch N{9,{
        " #     # "
        " ##    # "
        " ##    # "
        " # #   # "
        " # #   # "
        " #  #  # "
        " #  #  # "
        " #   # # "
        " #   # # "
        " #    ## "
        " #    ## "
        ".#     #."
    }};

    constexpr ch O{12,{
        "     ##     "
        "   ##  ##   "
        "  #      #  "
        "  #      #  "
        " #        # "
        " #        # "
        " #        # "
        " #        # "
        "  #      #  "
        "  #      #  "
        "   ##  ##   "
        ".    ##    ."
    }};

    constexpr ch P{9,{
        " #####   "
        " #    #  "
        " #     # "
        " #     # "
        " #     # "
        " #    #  "
        " #####   "
        " #       "
        " #       "
        " #       "
        " #       "
        ".#      ."
    }};

    constexpr ch Q{12,{
        "     ##     "
        "   ##  ##   "
        "  #      #  "
        "  #      #  "
        " #        # "
        " #        # "
        " #        # "
        " #        # "
        "  #    # #  "
        "  #     ##  "
        "   ##  ###  "
        ".    ##   #."
    }};

    constexpr ch R{9,{
        " #####   "
        " #    #  "
        " #     # "
        " #     # "
        " #     # "
        " #    #  "
        " #####   "
        " #  #    "
        " #   #   "
        " #    #  "
        " #     # "
        ".#     #."
    }};

    constexpr ch S{10,{
        "   #####  "
        "  #     # "
        " #        "
        " #        "
        "  #       "
        "   ####   "
        "       #  "
        "        # "
        "        # "
        "        # "
        " #     #  "
        ". #####  ."
    }};

    constexpr ch T{9,{
        " ####### "
        "    #    "
        "    #    "
        "    #    "
        "    #    "
        "    #    "
        "    #    "
        "    #    "
        "    #    "
        "    #    "
        "    #    "
        ".   #   ."
    }};

    constexpr ch U{9,{
        " #     # "
        " #     # "
        " #     # "
        " #     # "
        " #     # "
        " #     # "
        " #     # "
        " #     # "
        " #     # "
        " #     # "
        "  #   #  "
        ".  ###  ."
    }};

    constexpr ch V{11,{
        " #       # "
        " #       # "
        " #       # "
        "  #     #  "
        "  #     #  "
        "  #     #  "
        "   #   #   "
        "   #   #   "
        "   #   #   "
        "    # #    "
        "    # #    "
        ".    #    ."
    }};

    constexpr ch W{11,{
        " #       # "
        " #       # "
        " #       # "
        " #   #   # "
        " #   #   # "
        "  #  #  #  "
        "  #  #  #  "
        "  # # # #  "
        "  # # # #  "
        "  # # # #  "
        "  # # # #  "
        ".  #   #  ."
    }};

    constexpr ch X{9,{
        " #     # "
        "  #   #  "
        "  #   #  "
        "   # #   "
        "   # #   "
        "    #    "
        "    #    "
        "   # #   "
        "   # #   "
        "  #   #  "
        "  #   #  "
        ".#     #."
    }};

    constexpr ch Y{9,{
        " #     # "
        " #     # "
        " #     # "
        "  #   #  "
        "  #   #  "
        "   # #   "
        "   # #   "
        "    #    "
        "    #    "
        "   #     "
        "   #     "
        ".#      ."
    }};

    constexpr ch Z{9,{
        " ####### "
        "       # "
        "      #  "
        "      #  "
        "     #   "
        "    #    "
        "    #    "
        "   #     "
        "  #      "
        "  #      "
        " #       "
        ".#######."
    }};

    constexpr ch open_sq_bracket{5,{
        " ### "
        " #   "
        " #   "
        " #   "
        " #   "
        " #   "
        " #   "
        " #   "
        " #   "
        " #   "
        " #   "
        " #   "
        ".#  ."
        " #   "
        " ### "
    }};

    constexpr ch backslash{7,{
        " #     "
        " #     "
        " #     "
        "  #    "
        "  #    "
        "  #    "
        "   #   "
        "   #   "
        "   #   "
        "    #  "
        "    #  "
        "    #  "
        ".    #."
        "     # "
    }};

    constexpr ch close_sq_bracket{5,{
        " ### "
        "   # "
        "   # "
        "   # "
        "   # "
        "   # "
        "   # "
        "   # "
        "   # "
        "   # "
        "   # "
        "   # "
        ".  #."
        "   # "
        " ### "
    }};

    constexpr ch caret{9,{
        "    #    "
        "   # #   "
        "   # #   "
        "  #   #  "
        "  #   #  "
        " #     # "
        " #     # "
        "         "
        "         "
        "         "
        "         "
        ".       ."
    }};

    constexpr ch underline{9,{
        ".       ."
        "         "
        " ####### "
    }};

    constexpr ch grave_accent{5,{
        " #   "
        " #   "
        "  #  "
        "   # "
        "     "
        "     "
        "     "
        "     "
        "     "
        "     "
        "     "
        "     "
        ".   ."
    }};

    constexpr ch a{8,{
        "  ##### "
        " #    # "
        " #    # "
        " #    # "
        " #    # "
        " #    # "
        " #    # "
        ". #### *"
    }};

    constexpr ch b{8,{
        " #      "
        " #      "
        " #      "
        " #      "
        " #####  "
        " #    # "
        " #    # "
        " #    # "
        " #    # "
        " #    # "
        " #    # "
        ".##### ."
    }};

    constexpr ch c{8,{
        "   ###  "
        "  #   # "
        " #      "
        " #      "
        " #      "
        " #      "
        "  #   # "
        ".  ### ."
    }};

    constexpr ch d{8,{
        "      # "
        "      # "
        "      # "
        "      # "
        "  ##### "
        " #    # "
        " #    # "
        " #    # "
        " #    # "
        " #    # "
        " #    # "
        ". #### *"
    }};

    constexpr ch e{8,{
        "   ###  "
        "  #   # "
        " #    # "
        " #    # "
        " #####  "
        " #      "
        "  #   # "
        ".  ### ."
    }};

    constexpr ch f{7,{
        "  ###  "
        " #   # "
        " #   # "
        " #     "
        "###    "
        " #     "
        " #     "
        " #     "
        " #     "
        " #     "
        " #     "
        ".#    ."
    }};

    constexpr ch g{8,{
        "  ####  "
        " #    # "
        " #    # "
        " #    # "
        " #    # "
        "  ####  "
        " #      "
        ".##    ."
        "  ####  "
        " #    # "
        " #    # "
        "  ####  "
    }};

    constexpr ch h{8,{
        " #      "
        " #      "
        " #      "
        " #      "
        " #####  "
        " #    # "
        " #    # "
        " #    # "
        " #    # "
        " #    # "
        " #    # "
        ".#    #."
    }};

    constexpr ch i{4,{
        " #  "
        "    "
        " #  "
        " #  "
        " #  "
        " #  "
        " #  "
        " #  "
        " #  "
        ". #."
    }};

    constexpr ch j{7,{
        "     # "
        "       "
        "     # "
        "     # "
        "     # "
        "     # "
        "     # "
        "     # "
        "     # "
        "     # "
        "     # "
        "     # "
        "     # "
        " #  #  "
        ". ##  ."
    }};

    constexpr ch k{8,{
        " #      "
        " #      "
        " #      "
        " #      "
        " #    # "
        " #   #  "
        " #  #   "
        " ###    "
        " #  #   "
        " #   #  "
        " #    # "
        ".#    #."
    }};

    constexpr ch l{4,{
        " #  "
        " #  "
        " #  "
        " #  "
        " #  "
        " #  "
        " #  "
        " #  "
        " #  "
        " #  "
        " #  "
        ".# ."
    }};

    constexpr ch m{9,{
        " # # ##  "
        " ## #  # "
        " #  #  # "
        " #  #  # "
        " #  #  # "
        " #  #  # "
        " #  #  # "
        ".#  #  #."
    }};

    constexpr ch n{8,{
        " #####  "
        " #    # "
        " #    # "
        " #    # "
        " #    # "
        " #    # "
        " #    # "
        ".#    #."
    }};

    constexpr ch o{9,{
        "   ###   "
        "  #   #  "
        " #     # "
        " #     # "
        " #     # "
        " #     # "
        "  #   #  "
        ".  ###  ."
    }};

    constexpr ch p{8,{
        " # ###  "
        " ##   # "
        " #    # "
        " #    # "
        " #    # "
        " #    # "
        " ##   # "
        ".# ### ."
        " #      "
        " #      "
        " #      "
        " #      "
    }};

    constexpr ch q{8,{
        "  ### # "
        " #   ## "
        " #    # "
        " #    # "
        " #    # "
        " #    # "
        " #   ## "
        ". ### #."
        "      # "
        "      # "
        "      ##"
        "      # "
    }};

    constexpr ch r{8,{
        " # ###  "
        " ##   # "
        " #      "
        " #      "
        " #      "
        " #      "
        " #      "
        ".#     ."
    }};

    constexpr ch s{8,{
        "  ##### "
        " #      "
        " #      "
        "  ####  "
        "      # "
        "      # "
        "      # "
        ".##### ."
    }};

    constexpr ch t{8,{
        "   #    "
        "   #    "
        "   #    "
        "   #    "
        " #####  "
        "   #    "
        "   #    "
        "   #    "
        "   #    "
        "   #    "
        "   #    "
        ".   ## ."
    }};

    constexpr ch u{8,{
        " #    # "
        " #    # "
        " #    # "
        " #    # "
        " #    # "
        " #    # "
        " #    # "
        ". #### ."
    }};

    constexpr ch v{9,{
        " #     # "
        " #     # "
        "  #   #  "
        "  #   #  "
        "   # #   "
        "   # #   "
        "    #    "
        ".   #   ."
    }};

    constexpr ch w{9,{
        " #     # "
        " #     # "
        " #     # "
        "  # # #  "
        "  # # #  "
        "  # # #  "
        "   # #   "
        ".  # #  ."
    }};

    constexpr ch x{7,{
        " #   # "
        " #   # "
        "  # #  "
        "   #   "
        "  # #  "
        "  # #  "
        " #   # "
        ".#   #."
    }};

    constexpr ch y{9,{
        " #     # "
        " #     # "
        "  #    # "
        "  #    # "
        "   #  #  "
        "   #  #  "
        "    # #  "
        ".    # . "
        "     #   "
        "     #   "
        " #  #    "
        "  ##     "
    }};

    constexpr ch z{8,{
        " ###### "
        "      # "
        "     #  "
        "    #   "
        "   #    "
        "  #     "
        " #      "
        ".######."
    }};

    constexpr ch chars_[] = {
        A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P,Q,R,S,T,U,V,W,X,Y,Z,
        open_sq_bracket,backslash,close_sq_bracket,caret,underline,grave_accent,
        a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y,z};
}

void ev3plotter::print_text(ev3plotter::Display& d, std::uint32_t xpos, std::uint32_t ypos, std::string_view text, bool color) {
    for (auto c : text) {
        const auto& found_ch = chars_[c - 'A'];
        const auto& glyph = found_ch.glyph();

        const auto xoffset_from_origin = glyph.origin.x - static_cast<int>(glyph.topLeft.x);
        const auto yoffset_from_origin = -(glyph.origin.y - static_cast<int>(glyph.topLeft.y));

        const auto curr_x_offset = static_cast<int>(xpos) + xoffset_from_origin;
        const auto curr_y_offset = static_cast<int>(ypos) + yoffset_from_origin;
        for (auto&& p : found_ch.glyph().glyph_path) {
            const auto x = curr_x_offset + p.x;
            const auto y = curr_y_offset + p.y;

            if (x >= 0 && y >= 0 && x < static_cast<int>(d.width) && y < static_cast<int>(d.height)) {
                d.set(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), color);
            }
        }

        xpos += static_cast<std::uint32_t>(glyph.advance);
    }
}