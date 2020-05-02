#include <atomic>
#include <chrono>
#include <display.h>
#include <driver.h>
#include <ev3dev.h>
#include <functional>
#include <iostream>
#include <mqueue/message_queue.h>
#include <mutex>
#include <numeric>
#include <scheduler.h>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <widgets.h>

using namespace ev3plotter;

namespace {
constexpr const std::size_t c_maxMessageSize{256};
}

int main() {
    std::atomic_bool exit{false};

    message_queue read_queue{"/ev3plotter_input",
                             c_maxMessageSize,
                             message_queue::option::read | message_queue::option::remove_on_destruction};
    message_queue write_queue{"/ev3plotter_output", c_maxMessageSize, message_queue::option::write};

    Scheduler sch{};

    ev3dev::lcd display{};

    state s{sch};

    const StaticMenu* main_menu_ptr{};

    StaticMenu exit_menu{"Exit?",
                         {{"yes", [&exit] { exit = true; }}, {"no", [&]() { s.set_widget(main_menu_ptr->make()); }}}};

    Message message{"Please connect motors as follows:",
                    "Output A: tool head motor\n"
                    "Output B: X motor\n"
                    "Output C: Y motor\n",
                    "Close",
                    [&]() { s.set_widget(main_menu_ptr->make()); }};

    ev3plotter::display d{display.frame_buffer(),
                          static_cast<int>(display.resolution_x()),
                          static_cast<int>(display.resolution_y())};

    const IWidget* show_homing_limits_return_widget{nullptr};
    Message show_homing_results{"Homing results:", "Homing not done!", "Exit", [&] {
                                    s.set_widget(show_homing_limits_return_widget->make());
                                }};

    const IWidget* utilities_menu_ptr{nullptr};
    const auto if_homed{[&](auto do_when_homed) {
        if (s.homed_) {
            do_when_homed();
        } else {
            show_homing_limits_return_widget = utilities_menu_ptr;
            s.set_widget(show_homing_results.make());
        }
    }};

    StaticMenu utilities_menu{
        "Utilities",
        {{"< Back",
          [&] {
              show_homing_limits_return_widget = main_menu_ptr;
              s.set_widget(main_menu_ptr->make());
          }},
         {"x = 0",
          [&] { if_homed([&] { commands::go(s, sch, pos::x(*s.homed_, normalized_pos{0}), {}, {}, nullptr); }); }},
         {"x+10",
          [&] {
              if_homed([&] {
                  commands::go(s, sch, pos::x(*s.homed_, pos::read_x(s) + normalized_pos{10}), {}, {}, nullptr);
              });
          }},
         {"x+100",
          [&] {
              if_homed([&] {
                  commands::go(s, sch, pos::x(*s.homed_, pos::read_x(s) + normalized_pos{100}), {}, {}, nullptr);
              });
          }},
         {"x-10",
          [&] {
              if_homed([&] {
                  commands::go(s, sch, pos::x(*s.homed_, pos::read_x(s) + normalized_pos{-10}), {}, {}, nullptr);
              });
          }},
         {"x-100",
          [&] {
              if_homed([&] {
                  commands::go(s, sch, pos::x(*s.homed_, pos::read_x(s) + normalized_pos{-100}), {}, {}, nullptr);
              });
          }},
         {"y = 0",
          [&] { if_homed([&] { commands::go(s, sch, {}, pos::y(*s.homed_, normalized_pos{0}), {}, nullptr); }); }},
         {"y+10",
          [&] {
              if_homed([&] {
                  commands::go(s, sch, pos::y(*s.homed_, pos::read_y(s) + normalized_pos{10}), {}, {}, nullptr);
              });
          }},
         {"y+100",
          [&] {
              if_homed([&] {
                  commands::go(s, sch, pos::y(*s.homed_, pos::read_y(s) + normalized_pos{100}), {}, {}, nullptr);
              });
          }},
         {"y-10",
          [&] {
              if_homed([&] {
                  commands::go(s, sch, pos::y(*s.homed_, pos::read_y(s) + normalized_pos{-10}), {}, {}, nullptr);
              });
          }},
         {"y-100",
          [&] {
              if_homed([&] {
                  commands::go(s, sch, pos::y(*s.homed_, pos::read_y(s) + normalized_pos{-100}), {}, {}, nullptr);
              });
          }},
         {"Tool up",
          [&] { if_homed([&] { commands::go(s, sch, {}, {}, pos::z(*s.homed_, normalized_pos{0}), nullptr); }); }},
         {"Tool down", [&] {
              if_homed([&] {
                  commands::go(
                      s,
                      sch,
                      {},
                      {},
                      pos::z(*s.homed_, normalized_pos{0}) + raw_pos{pos::z_travel(*s.homed_).get()},
                      nullptr);
              });
          }}}};

    utilities_menu_ptr = &utilities_menu;

    StaticMenu main_menu{"Main menu",
                         {{"home",
                           [&]() {
                               commands::home(s, sch, *main_menu_ptr, [&](homing_results results) {
                                   s.homed_ = results;
                                   show_homing_results.update_text(print_homing_results(*s.homed_));
                               });
                           }},
                          {"display required connections", [&]() { s.set_widget(message.make()); }},
                          {"show homing results", [&]() { s.set_widget(show_homing_results.make()); }},
                          {"utilities", [&] { s.set_widget(utilities_menu.make()); }, has_more{true}},
                          {"exit", [&]() { s.set_widget(exit_menu.make()); }}}};

    main_menu_ptr = &main_menu;
    show_homing_limits_return_widget = &main_menu;

    s.set_widget(main_menu.make());

    auto prev_draw_time = Scheduler::clock::now();
    auto prev_loop_time = Scheduler::clock::now();
    constexpr auto c_loopTime = std::chrono::milliseconds{100};

    std::function<void()> loop;
    loop = [&] {
        s.handle_events();

        const auto now = Scheduler::clock::now();

        // Force redraw every 200 ms.
        if (s.draw(d, now - prev_draw_time > std::chrono::milliseconds{200})) {
            prev_draw_time = now;
        }

        if (!exit) {
            // Schedule self of the next loop, at a low priority
            sch.schedule(priority{10}, c_loopTime - (now - prev_loop_time), loop);
            prev_loop_time = now;
        }
    };

    sch.schedule(loop);
    sch.run();

    return 0;
}
