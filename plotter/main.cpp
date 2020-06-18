#include <atomic>
#include <chrono>
#include <display.h>
#include <driver.h>
#include <ev3dev.h>
#include <functional>
#include <iostream>
#include "server.h"
#include <mutex>
#include <numeric>
#include <scheduler.h>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <widgets.h>

#include <fmt/format.h>

using namespace ev3plotter;

namespace {
    std::optional<raw_pos> calc_x(const state& state, std::optional<double> x)
    {
        if (! x) {
            return {};
        }

        const auto& scale{state.gcode_state_.use_mm ? state.gcode_state_.c_stepsToMm : state.gcode_state_.c_stepsToInches };
        const auto relative_steps{*x / scale[0]};

        if (state.gcode_state_.relative_moves) {
            return pos::x(*state.homed_, pos::advanced_x(state, normalized_pos{static_cast<int>(relative_steps)}));
        } else {
            return pos::x(*state.homed_, normalized_pos{static_cast<int>(relative_steps)});
        }
    }

    std::optional<raw_pos> calc_y(const state& state, std::optional<double> y)
    {
        if (! y) {
            return {};
        }

        const auto& scale{state.gcode_state_.use_mm ? state.gcode_state_.c_stepsToMm : state.gcode_state_.c_stepsToInches };
        const auto relative_steps{*y / scale[1]};

        if (state.gcode_state_.relative_moves) {
            return pos::y(*state.homed_, pos::advanced_y(state, normalized_pos{static_cast<int>(relative_steps)}));
        } else {
            return pos::y(*state.homed_, normalized_pos{static_cast<int>(relative_steps)});
        }
    }

    std::optional<raw_pos> calc_z(const state& state, std::optional<double> z)
    {
        if (! z) {
            return {};
        }

        const auto& scale{state.gcode_state_.use_mm ? state.gcode_state_.c_stepsToMm : state.gcode_state_.c_stepsToInches };
        const auto relative_steps{*z / scale[2]};

        if (state.gcode_state_.relative_moves) {
            return pos::z(*state.homed_, pos::advanced_z(state, normalized_pos{static_cast<int>(relative_steps)}));
        } else {
            return pos::z(*state.homed_, normalized_pos{static_cast<int>(relative_steps)});
        }
    }

    void handle_server_event(
        state& state,
        const ServerMessage& message,
        const ev3plotter::IWidget& prevWidget,
        std::function<void(const std::optional<HandlerError>&)> handler) {
        switch (message.Command) {
        case GCodeCommand::Go:
            if (state.homed_) {
                auto [speed_x, speed_y] = pos::calc_speeds(state, message.X, message.Y);
                commands::go(
                    state,
                    state.scheduler_,
                    calc_x(state, message.X),
                    calc_y(state, message.Y),
                    calc_z(state, message.Z),
                    speed_x,
                    speed_y,
                    [handler] { handler({}); });
            } else {
                handler(HandlerError{"Can't go - not homed!"});
            }
            break;

        case GCodeCommand::UseInches:
            state.gcode_state_.use_mm = false;
            handler({});
            break;

        case GCodeCommand::UseMm:
            state.gcode_state_.use_mm = true;
            handler({});
            break;

        case GCodeCommand::Home:
            commands::home(state, state.scheduler_, prevWidget, [handler, &state] (auto homing_results) {
                if (homing_results.index() == 0) {
                    state.homed_ = std::get<0>(homing_results);
                    handler({});
                } else {
                    handler(HandlerError{std::get<1>(homing_results)});
                }
            });
            break;

        case GCodeCommand::AbsolutePositioning:
            state.gcode_state_.relative_moves = false;
            handler({});
            break;

        case GCodeCommand::RelativePositioning:
            state.gcode_state_.relative_moves = true;
            handler({});
            break;

        default:
            handler(HandlerError{fmt::format("Don't know how to handle {} command", message.Command)});
            break;
        }
    }
} // namespace

int main() {
    std::atomic_bool exit{false};

    std::optional<Server> server;
    try
    {
        server.emplace();
    } catch (const std::runtime_error&) {};

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
                  commands::go(s, sch, {}, pos::y(*s.homed_, pos::read_y(s) + normalized_pos{10}), {}, nullptr);
              });
          }},
         {"y+100",
          [&] {
              if_homed([&] {
                  commands::go(s, sch, {}, pos::y(*s.homed_, pos::read_y(s) + normalized_pos{100}), {}, nullptr);
              });
          }},
         {"y-10",
          [&] {
              if_homed([&] {
                  commands::go(s, sch, {}, pos::y(*s.homed_, pos::read_y(s) + normalized_pos{-10}), {}, nullptr);
              });
          }},
         {"y-100",
          [&] {
              if_homed([&] { commands::go(s, sch, {}, raw_pos{s.x_motor.position() - 100}, {}, nullptr);
              });
          }},{"x+100,y+100",
          [&] {
              if_homed([&] {
                  commands::go(s, sch, raw_pos{s.x_motor.position() + 100}, raw_pos{s.y_motor.position() + 100}, {}, nullptr);
              });
          }},{"x+100,y-100",
          [&] {
              if_homed([&] {
                  commands::go(s, sch, raw_pos{s.x_motor.position() + 100}, raw_pos{s.y_motor.position() - 100}, {}, nullptr);
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
                               commands::home(s, sch, *main_menu_ptr, [&](auto results) {
                                  if (results.index() == 0) {
                                      s.homed_ = std::get<0>(results);
                                      show_homing_results.update_text(print_homing_results(*s.homed_));
                                  }
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

        if (server) {
            server->handle_events([&](auto&& msg, auto&& callback) {
                handle_server_event(s, std::forward<decltype(msg)>(msg), *main_menu_ptr, std::forward<decltype(callback)>(callback));
                //callback(HandlerError{"Parsing succeeded, but I can't handle this!"});
            });
        }

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
