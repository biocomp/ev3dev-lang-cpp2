#include "driver.h"

#include "scheduler.h"

#include <fmt/core.h>
#include <fmt/ostream.h>
#include <fmt/ranges.h>
#include <thread>

using namespace ev3plotter;

// ###############
// state
// ###############

void state::handle_events() {
    if (down_button.pressed()) {
        changed_ = changed_ || widget_->handle_event(event::down);
    }

    if (up_button.pressed()) {
        changed_ = changed_ || widget_->handle_event(event::up);
    }

    if (ok_button.pressed()) {
        changed_ = changed_ || widget_->handle_event(event::ok);
    }
}

void state::set_widget(std::unique_ptr<IWidget::widget_state> widget) {
    widget_ = std::move(widget);
    changed_ = true;
}

bool state::draw(ev3plotter::display& d, bool force_redraw) {
    if (changed() || force_redraw) {
        widget_->draw(d);

        // std::cout << "state::draw 1...\n";
        std::string_view overlay_text{"[{}|{},{}]"};
        char buffer[256];
        if (homed_) {
            overlay_text = {buffer,
                            fmt::format_to_n(
                                buffer,
                                std::size(buffer),
                                overlay_text,
                                pos::read_z(*this),
                                pos::read_x(*this),
                                pos::read_y(*this))
                                .size};
        } else {
            overlay_text = {buffer,
                            fmt::format_to_n(
                                buffer,
                                std::size(buffer),
                                overlay_text,
                                tool_motor.connected() ? '?' : 'x',
                                x_motor.connected() ? '?' : 'x',
                                y_motor.connected() ? '?' : 'x')
                                .size};
        }

        // std::cout << "state::draw 2...\n";
        print_text(d, {d.width / 2, c_menuHeaderHeight - c_menuPadding}, overlay_text, true);

        return true;
    }

    return false;
}

bool state::changed() {
    if (changed_ || widget_->changed()) {
        changed_ = false;
        return true;
    }

    return false;
}

// ###############
//
// ###############

std::string ev3plotter::print_homing_results(const homing_results& results) {
    return fmt::format(
        "X: [{}, {}]-> {}\n"
        "Y: [{}, {}]-> {}\n"
        "Tool: [{}, {}]-> {}\n",
        results.x_min,
        results.x_max,
        std::abs((results.x_min - results.x_max).get()),
        results.y_min,
        results.y_max,
        std::abs((results.y_min - results.y_max).get()),
        results.tool_up_pos,
        results.tool_down_pos,
        std::abs((results.tool_up_pos - results.tool_down_pos).get()));
}

// ###############
// commands
// ###############

void commands::home(
    state& s,
    Scheduler& scheduler,
    const IWidget& prevWidget,
    std::function<void(homing_results)> done) {
    class HomeState : public std::enable_shared_from_this<HomeState> {
      public:
        HomeState(
            state& s,
            Scheduler& scheduler,
            const IWidget& prevWidget,
            std::function<void(homing_results)> done)
            : 
            isValid_ {s.tool_motor.connected() && s.x_motor.connected() && s.y_motor.connected()},
            homing_message_{[&]() {
                if (isValid_) {
                    return Message{"Homing, please wait...", make_step_text(current_step_, "tool up"), "Stop", [this] {
                                       finish(true);
                                   }};
                } else {
                    std::vector<std::string> notConnected{};
                    if (! s.tool_motor.connected()) {
                        notConnected.push_back("tool");
                    }
                    if (! s.x_motor.connected()) {
                        notConnected.push_back("x");
                    }
                    if (! s.y_motor.connected()) {
                        notConnected.push_back("y");
                    }
                    return Message{"Homing failed :(", fmt::format("{} motor\nnot connected!\n", notConnected), "Stop", [this] { finish(false); }};
                }
            }()}
            , s_{s}
            , scheduler_{scheduler}
            , prevWidget_{prevWidget}
            , done_{std::move(done)}
            {
                s_.set_widget(homing_message_.make());
            }

        void step() {
            switch (local_state_) {
            case home::start:
                s_.tool_motor.reset();
                s_.x_motor.reset();
                s_.y_motor.reset();

                start_motor(s_.tool_motor, -30);
                local_state_ = home::homing_tool_up;
                break;

            case home::homing_tool_up:
                if (finish_homing(s_.tool_motor, +20, results_.tool_up_pos)) {
                    start_homing(s_.x_motor, +50, "x min (left)", home::homing_x_left);
                }
                break;

            case home::homing_x_left:
                if (finish_homing(s_.x_motor, -30, results_.x_min)) {
                    start_homing(s_.x_motor, -50, "x max (right)", home::homing_x_right);
                }
                break;

            case home::homing_x_right:
                if (finish_homing(s_.x_motor, +30, results_.x_max)) {
                    start_homing(s_.y_motor, -40, "y min", home::homing_y_min);
                }
                break;

            case home::homing_y_min:
                if (finish_homing(s_.y_motor, +350, results_.y_min)) {
                    start_homing(s_.y_motor, +40, "y max", home::homing_y_max);
                }
                break;

            case home::homing_y_max:
                if (finish_homing(s_.y_motor, -30, results_.y_max)) {
                    local_state_ = home::go_for_homing_tool_down;
                }
                break;

            case home::go_for_homing_tool_down:
                start_homing(s_.tool_motor, +20, "tool down", home::homing_tool_down);
                break;

            case home::homing_tool_down:
                if (finish_homing(s_.tool_motor, -15, results_.tool_down_pos)) {
                    local_state_ = home::show_results;
                }
                break;

            case home::show_results:
                results_message.update_text(print_homing_results(results_));
                s_.set_widget(results_message.make());
                local_state_ = home::showing_results;
                break;

            case home::showing_results:
                break;

            case home::stop:
                return done_(results_);
            
            case home::stop_failed:
                return;
            }

            scheduler_.schedule(std::chrono::milliseconds{10}, [this_ = shared_from_this()]{ this_->step(); });
        }

      private:
        bool isValid_;

        enum class home {
            start,
            homing_tool_up,
            homing_x_left,
            homing_x_right,
            homing_y_min,
            homing_y_max,
            go_for_homing_tool_down,
            homing_tool_down,
            show_results,
            showing_results,
            stop,
            stop_failed
        } local_state_{isValid_ ? home::start : home::showing_results};

        int current_step_{1};

        Message homing_message_;

        homing_results results_;

        Message results_message{"Homing results:", "", "Exit", [this] { finish(true); }};

        state& s_;
        Scheduler& scheduler_;
        const IWidget& prevWidget_;

        std::function<void(homing_results)> done_;

        static std::string make_step_text(int& current_step, std::string stepText) {
            return std::string{"Step"} + std::to_string(current_step++) + " of 6: " + stepText +
                "\nPress 'ok' to stop.";
        }

        void finish(bool success) {
            s_.set_widget(prevWidget_.make());
            if (success) {
                local_state_ = home::stop;
            } else {
                local_state_ = home::stop_failed;
            }
        }

        auto has_state(ev3dev::motor& motor, const std::string& state) {
            const auto& current_states = motor.state();
            return current_states.find(state) != current_states.end();
        }

        bool stalled(ev3dev::motor& motor) { return has_state(motor, motor.state_stalled); }

        void start_motor(ev3dev::motor& motor, int cycle_sp) {
            motor.set_polarity(motor.polarity_normal).set_duty_cycle_sp(cycle_sp).run_direct();
            std::this_thread::sleep_for(std::chrono::milliseconds{300});
        }

        bool finish_homing(ev3dev::motor& motor, int change_pos_by, raw_pos& store_pos) {
            if (stalled(motor)) {
                store_pos = raw_pos{motor.position() + change_pos_by};
                motor.stop();
                // std::this_thread::sleep_for(std::chrono::milliseconds{300});
                return true;
            }

            return false;
        }

        void start_homing(ev3dev::motor& motor, int cycle_sp, const std::string& step_text, home next_state) {
            homing_message_.update_text(make_step_text(current_step_, step_text));
            start_motor(motor, cycle_sp);
            local_state_ = next_state;
        }
    };

    //auto this_ = std::make_shared<HomeState>(s, scheduler /*, d*/, prevWidget, std::move(done));
    scheduler.schedule([this_ = std::make_shared<HomeState>(s, scheduler /*, d*/, prevWidget, std::move(done))] { this_->step(); });
}

void commands::go(
    state& s,
    Scheduler& scheduler,
    std::optional<raw_pos> x,
    std::optional<raw_pos> y,
    std::optional<raw_pos> z,
    std::function<void()> done) {

    class GoState : public std::enable_shared_from_this<GoState> {
      public:
        GoState(
            state& s,
            Scheduler& scheduler,
            std::optional<raw_pos> x,
            std::optional<raw_pos> y,
            std::optional<raw_pos> z,
            std::function<void()> done)
            : s_{s}
            , scheduler_{scheduler}
            , x_{x}
            , y_{y}
            , z_{z}
            , done_{std::move(done)} {
            if (x) {
                s.x_motor.set_stop_action("hold").set_speed_sp(200).set_position_sp(x->get()).run_to_abs_pos();
            }

            if (y) {
                s.y_motor.set_stop_action("hold").set_speed_sp(200).set_position_sp(y->get()).run_to_abs_pos();
            }

            if (z) {
                s.tool_motor.set_stop_action("hold").set_speed_sp(200).set_position_sp(z->get()).run_to_abs_pos();
            }
        }

        void step() {
            if (s_.ok_button.pressed()) {
                return;
            }

            bool all_reached{true};
            if (x_) {
                all_reached = all_reached && raw_pos{s_.x_motor.position()} == *x_;
            }

            if (y_) {
                all_reached = all_reached && raw_pos{s_.y_motor.position()} == *y_;
            }

            if (z_) {
                all_reached = all_reached && raw_pos{s_.tool_motor.position()} == *z_;
            }

            if (! all_reached) {
                scheduler_.schedule(std::chrono::milliseconds{200}, [this_ = shared_from_this()] { this_->step(); });
            } else {
                if (done_) {
                    done_();
                }
            }
        }

      private:
        state& s_;
        Scheduler& scheduler_;
        bool stop_{false};
        std::optional<raw_pos> x_, y_, z_;
        std::function<void()> done_;
    };

    scheduler.schedule([self_ = std::make_shared<GoState>(s, scheduler, x, y, z, std::move(done))] { self_->step(); });
}

// ###############
// pos
// ###############

raw_pos pos::detail::to_raw(raw_pos min, raw_pos max, normalized_pos val) {
    return (min < max) ? std::clamp(min + raw_pos{val.get()}, min, max)
                       : std::clamp(min - raw_pos{val.get()}, max, min);
}

normalized_pos pos::detail::to_norm(raw_pos min, raw_pos max, raw_pos val) {
    return normalized_pos{
        ((min < max) ? std::clamp(val - min, raw_pos{0}, max - min) : std::clamp(min - val, raw_pos{0}, min - max))
            .get()};
}

raw_pos pos::x(const homing_results& h, normalized_pos val) { return detail::to_raw(h.x_min, h.x_max, val); }

raw_pos pos::y(const homing_results& h, normalized_pos val) { return detail::to_raw(h.y_min, h.y_max, val); }

raw_pos pos::z(const homing_results& h, normalized_pos val) {
    return detail::to_raw(h.tool_up_pos, h.tool_down_pos, val);
}

normalized_pos pos::read_x(const state& s) noexcept {
    return detail::to_norm(s.homed_->x_min, s.homed_->x_max, raw_pos{s.x_motor.position()});
}

normalized_pos pos::read_y(const state& s) noexcept {
    return detail::to_norm(s.homed_->y_min, s.homed_->y_max, raw_pos{s.y_motor.position()});
}

normalized_pos pos::read_z(const state& s) noexcept {
    return detail::to_norm(s.homed_->tool_down_pos, s.homed_->tool_up_pos, raw_pos{s.tool_motor.position()});
}

normalized_pos pos::advanced_x(const state& s, normalized_pos by) noexcept {
    return detail::clamp(read_x(s) + by, normalized_pos{0}, x_travel(*s.homed_));
}

normalized_pos pos::advanced_y(const state& s, normalized_pos by) noexcept {
    return detail::clamp(read_y(s) + by, normalized_pos{0}, y_travel(*s.homed_));
}

normalized_pos pos::advanced_z(const state& s, normalized_pos by) noexcept {
    return detail::clamp(read_z(s) + by, normalized_pos{0}, z_travel(*s.homed_));
}

raw_pos pos::advanced_x(const state& s, raw_pos by) noexcept {
    return detail::clamp(raw_pos{s.x_motor.position()} + by, s.homed_->x_min, s.homed_->x_max);
}

raw_pos pos::advanced_y(const state& s, raw_pos by) noexcept {
    return detail::clamp(raw_pos{s.y_motor.position()} + by, s.homed_->y_min, s.homed_->y_max);
}

raw_pos pos::advanced_z(const state& s, raw_pos by) noexcept {
    return detail::clamp(raw_pos{s.tool_motor.position()} + by, s.homed_->tool_up_pos, s.homed_->tool_down_pos);
}

normalized_pos pos::x_travel(const homing_results& h) noexcept { return normalized_pos{std::abs((h.x_min - h.x_max).get())}; }
normalized_pos pos::y_travel(const homing_results& h) noexcept { return normalized_pos{std::abs((h.y_min - h.y_max).get())}; }
normalized_pos pos::z_travel(const homing_results& h) noexcept {
    return normalized_pos{std::abs((h.tool_down_pos - h.tool_up_pos).get())};
}