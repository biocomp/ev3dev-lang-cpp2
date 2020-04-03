#include <widgets.h>
#include <display.h>
#include <driver.h>
#include <ev3dev.h>

#include <thread>
#include <chrono>
#include <string>
#include <numeric>
#include <iostream>
#include <functional>
#include <string_view>
#include <type_traits>

using namespace ev3plotter;

int main()
{
    bool exit = false;
    ev3dev::lcd display{};

    state s;

    const StaticMenu *main_menu_ptr{};

    StaticMenu exit_menu{
        "Exit?",
        {
            {"yes", [&exit] { exit = true; }},
            {"no",  [&]() { s.set_widget(main_menu_ptr->make()); } }
        }
    };

    Message message{"Please connect motors as follows:",
        "Output A: tool head motor\n"
        "Output B: X motor\n"
        "Output C: Y motor\n",
        "Close", [&]() { s.set_widget(main_menu_ptr->make()); }
    };

    ev3plotter::display d{display.frame_buffer(), static_cast<int>(display.resolution_x()), static_cast<int>(display.resolution_y())};

    const IWidget* show_homing_limits_return_widget{nullptr};
    Message show_homing_results{
        "Homing results:",
        "Homing not done!",
        "Exit",
        [&]{ s.set_widget(show_homing_limits_return_widget->make()); }
    };

    const IWidget* utilities_menu_ptr{nullptr};
    const auto if_homed{[&](auto do_when_homed){
        if (s.homed_) {
            do_when_homed();
        } else {
            show_homing_limits_return_widget = utilities_menu_ptr;
            s.set_widget(show_homing_results.make());
        }
    }};

    StaticMenu utilities_menu {
        "Utilities", {
            { "< Back", [&]{ show_homing_limits_return_widget = main_menu_ptr; s.set_widget(main_menu_ptr->make()); } },
            { "x = 0", [&]{ if_homed([&]{ commands::go(s, pos::x(*s.homed_, normalized_pos{0}), {}, {});}); } },
            { "x+10", [&]{ if_homed([&]{ commands::go(s, pos::x(*s.homed_, pos::read_x(s) + normalized_pos{10}), {}, {}); }); } },
            { "x+100", [&]{ if_homed([&]{ commands::go(s, pos::x(*s.homed_, pos::read_x(s) + normalized_pos{100}), {}, {}); }); } },
            { "x-10", [&]{ if_homed([&]{ commands::go(s, pos::x(*s.homed_, pos::read_x(s) + normalized_pos{-10}), {}, {}); }); } },
            { "x-100", [&]{ if_homed([&]{ commands::go(s, pos::x(*s.homed_, pos::read_x(s) + normalized_pos{-100}), {}, {}); }); } },
            { "y = 0", [&]{ if_homed([&]{ commands::go(s, {}, pos::y(*s.homed_, normalized_pos{0}), {}); }); } },
            { "y+10", [&]{ if_homed([&]{ commands::go(s, pos::y(*s.homed_, pos::read_y(s) + normalized_pos{10}), {}, {}); }); } },
            { "y+100", [&]{ if_homed([&]{ commands::go(s, pos::y(*s.homed_, pos::read_y(s) + normalized_pos{100}), {}, {}); }); } },
            { "y-10", [&]{ if_homed([&]{ commands::go(s, pos::y(*s.homed_, pos::read_y(s) + normalized_pos{-10}), {}, {}); }); } },
            { "y-100", [&]{ if_homed([&]{ commands::go(s, pos::y(*s.homed_, pos::read_y(s) + normalized_pos{-100}), {}, {}); }); } },
            { "Tool up", [&]{ if_homed([&]{ commands::go(s, {}, {}, pos::z(*s.homed_, normalized_pos{0})); }); } },
            { "Tool down", [&]{ if_homed([&]{ commands::go(s, {}, {}, pos::z(*s.homed_, normalized_pos{0}) + raw_pos{pos::z_travel(*s.homed_).get()}); }); } }
        }
    };

    utilities_menu_ptr = &utilities_menu;

    StaticMenu main_menu{
        "Main menu", {
            {"home", [&]() {
                    s.homed_ = commands::home(s, d, *main_menu_ptr);
                    show_homing_results.update_text(print_homing_results(*s.homed_));
                }},
            {"display required connections", [&]() { s.set_widget(message.make()); }},
            {"show homing results", [&](){ s.set_widget(show_homing_results.make()); } },
            {"utilities", [&]{ s.set_widget(utilities_menu.make()); }, has_more{true}},
            {"exit", [&]() { s.set_widget(exit_menu.make()); } }
        }
    };


    main_menu_ptr = &main_menu;
    show_homing_limits_return_widget = &main_menu;

    s.set_widget(main_menu.make());

    auto prev_draw_time{std::chrono::steady_clock::now()};
    while (!exit) {
        s.handle_events();

        const auto now = std::chrono::steady_clock::now();
        if (s.draw(d, now - prev_draw_time > std::chrono::milliseconds{200})) {
            prev_draw_time = now;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
    }

    return 0;
}
