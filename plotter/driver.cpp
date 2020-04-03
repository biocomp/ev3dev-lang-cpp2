#include "driver.h"

#include <fmt/core.h>
#include <fmt/ostream.h>

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

        //std::cout << "state::draw 1...\n";
        std::string_view overlay_text{"[{}|{},{}]"};
        char buffer[256];
        if (homed_) {
            overlay_text = {
                buffer,
                fmt::format_to_n(buffer, std::size(buffer), overlay_text, pos::read_z(*this), pos::read_x(*this), pos::read_y(*this)).size};
        } else {
            overlay_text = {
                buffer,
                fmt::format_to_n(buffer, std::size(buffer), overlay_text, tool_motor.connected() ? '?' : 'x', x_motor.connected() ? '?' : 'x', y_motor.connected() ? '?' : 'x').size};
        }

        //std::cout << "state::draw 2...\n";
        print_text(d, {d.width/2, c_menuHeaderHeight - c_menuPadding}, overlay_text, true);

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
            results.x_min, results.x_max, std::abs((results.x_min - results.x_max).get()),
            results.y_min, results.y_max, std::abs((results.y_min - results.y_max).get()),
            results.tool_up_pos, results.tool_down_pos, std::abs((results.tool_up_pos - results.tool_down_pos).get()));
}

// ###############
// commands
// ###############

homing_results commands::home(state &s, ev3plotter::display &d, const IWidget &prevWidget)
{
    //std::cout << "Entering home...\n";
    enum class home
    {
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
        stop
    } local_state{home::start};

    const auto finish{[&] {
        s.set_widget(prevWidget.make());
        local_state = home::stop;
    }};

    int current_step = 1;
    const auto make_step_text{[&current_step](auto step_text) {
        return std::string{"Step"} + std::to_string(current_step++) + " of 6: " + step_text + "\nPress 'ok' to stop.";
    }};

    Message homing_message{[&](){
        if (s.tool_motor.connected()) {
            return Message{
                "Homing, please wait...",
                make_step_text("tool up"),
                "Stop",
                finish};
        } else {
            return Message{
                "Homing failed :(",
                "Tool motor not connected!\n",
                "Stop",
                finish};
        }
        }()};

    homing_results results;


    Message results_message{
        "Homing results:",
        "",
        "Exit",
        finish
    };

    //std::cout << "Setting home_message...\n";
    s.set_widget(homing_message.make());
    //std::cout << "Done with home_message...\n";


    const auto has_state{[&](auto& motor, const auto& state) {
        const auto& current_states = motor.state();
        return current_states.find(state) != current_states.end();
    }};

    const auto stalled{[&](auto &motor) { return has_state(motor, motor.state_stalled); }};

    const auto start_motor{[](auto &motor, auto cycle_sp) {
        motor
            .set_polarity(motor.polarity_normal)
            .set_duty_cycle_sp(cycle_sp)
            .run_direct();
        std::this_thread::sleep_for(std::chrono::milliseconds{300});
    }};

    const auto finish_homing{[&](auto &motor, auto change_pos_by, auto& store_pos) {
        if (stalled(motor))
        {
            store_pos = raw_pos{motor.position() + change_pos_by};
            motor.stop();
            std::this_thread::sleep_for(std::chrono::milliseconds{300});
            return true;
        }

        return false;
    }};

    const auto start_homing{[&](auto& motor, auto cycle_sp, auto step_text, auto next_state){
        homing_message.update_text(make_step_text(step_text));
        start_motor(motor, cycle_sp);
        local_state = next_state;
    }};


    s.tool_motor.reset();
    s.x_motor.reset();
    s.y_motor.reset();

    //std::cout << "Reset motors...\n";
    auto prev_draw_time{std::chrono::steady_clock::now()};

    while (true)
    {
        //std::cout << "draw loop :: #1...\n";

        s.handle_events();

        //std::cout << "draw loop :: #2...\n";

        const auto now = std::chrono::steady_clock::now();
        if (s.draw(d, now - prev_draw_time > std::chrono::milliseconds{200})) {
            prev_draw_time = now;
        }

        //std::cout << "draw loop :: #3...\n";

        switch (local_state)
        {
        case home::start:
            start_motor(s.tool_motor, -30);
            local_state = home::homing_tool_up;
            break;

        case home::homing_tool_up:
            if (finish_homing(s.tool_motor, +20, results.tool_up_pos)) {
                start_homing(s.x_motor, +50, "x min (left)", home::homing_x_left);
            }
            break;

        case home::homing_x_left:
            if (finish_homing(s.x_motor, -30, results.x_min)) {
                start_homing(s.x_motor, -50, "x max (right)", home::homing_x_right);
            }
            break;

        case home::homing_x_right:
            if (finish_homing(s.x_motor, +30, results.x_max)) {
                start_homing(s.y_motor, -40, "y min", home::homing_y_min);
            }
            break;

        case home::homing_y_min:
            if (finish_homing(s.y_motor, +350, results.y_min)) {
                start_homing(s.y_motor, +40, "y max", home::homing_y_max);
            }
            break;

        case home::homing_y_max:
            if (finish_homing(s.y_motor, -30, results.y_max)) {
                local_state = home::go_for_homing_tool_down;
            }
            break;

        case home::go_for_homing_tool_down:
            start_homing(s.tool_motor, +20, "tool down", home::homing_tool_down);
            break;

        case home::homing_tool_down:
            if (finish_homing(s.tool_motor, -15, results.tool_down_pos)) {
                local_state = home::show_results;
            }
            break;

        case home::show_results:
            results_message.update_text(print_homing_results(results));
            s.set_widget(results_message.make());
            local_state = home::showing_results;
            break;

        case home::showing_results:
            break;

        case home::stop:
            return results;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
}

void commands::go(state& s, std::optional<raw_pos> x, std::optional<raw_pos> y, std::optional<raw_pos> z) {
    bool stop{false};

    if (x) {
        s.x_motor.set_duty_cycle_sp(100).set_speed_sp(100).set_position_sp(x->get()).run_to_abs_pos();
    }

    if (y) {
        s.y_motor.set_duty_cycle_sp(100).set_speed_sp(100).set_position_sp(y->get()).run_to_abs_pos();
    }

    if (z) {
        s.tool_motor.set_duty_cycle_sp(100).set_speed_sp(100).set_position_sp(z->get()).run_to_abs_pos();
    }

    const auto position_reached{[&] {
        bool all_reached{true};
        if (x) {
            all_reached = all_reached && raw_pos{s.x_motor.position()} == *x;
        }

        if (y) {
            all_reached = all_reached && raw_pos{s.y_motor.position()} == *y;
        }

        if (z) {
            all_reached = all_reached && raw_pos{s.tool_motor.position()} == *z;
        }

        return all_reached;
    }};

    while (!(stop || position_reached())) {
        stop = s.ok_button.pressed();

        std::this_thread::sleep_for(std::chrono::milliseconds{200});
    }
}

// ###############
// pos
// ###############

raw_pos pos::detail::to_raw(raw_pos min, raw_pos max, normalized_pos val) {
    return (min < max)
        ? std::clamp(min + raw_pos{val.get()}, min, max)
        : std::clamp(min - raw_pos{val.get()}, max, min);
}

normalized_pos pos::detail::to_norm(raw_pos min, raw_pos max, raw_pos val) {
    return normalized_pos{((min < max)
        ? std::clamp(val - min, raw_pos{0}, max - min)
        : std::clamp(min - val, raw_pos{0}, min - max)).get()};
}

raw_pos pos::x(const homing_results& h, normalized_pos val) {
    return detail::to_raw(h.x_min, h.x_max, val);
}

raw_pos pos::y(const homing_results& h, normalized_pos val) {
    return detail::to_raw(h.y_min, h.y_max, val);
}

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

normalized_pos pos::z_travel(const homing_results& h) noexcept {
    return normalized_pos{std::abs((h.tool_down_pos - h.tool_up_pos).get())};
}