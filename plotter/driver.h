#ifndef EV3PLOTTER_DRIVER_H_DEFINED
#define EV3PLOTTER_DRIVER_H_DEFINED

#include <ev3dev.h>
#include "common_definitions.h"
#include "widgets.h"
#include "gcode_state.h"

#include <functional>
#include <variant>
#include <string>

namespace ev3plotter {
    struct button {
        button(ev3dev::button& b) : b_{b} {}

        bool pressed() noexcept {
            bool old_pressed = prev_pressed_;
            prev_pressed_ = b_.pressed();
            if (!old_pressed && prev_pressed_) {
                return true;
            }

            return false;
        }

    private:
        const ev3dev::button& b_;
        bool prev_pressed_{false};
    };

    struct homing_results{
        raw_pos tool_up_pos{0};
        raw_pos tool_down_pos{0};

        raw_pos x_min{0};
        raw_pos x_max{0};

        raw_pos y_min{0};
        raw_pos y_max{0};
    };

    class Scheduler;

    struct state {
        state(Scheduler& scheduler, ev3dev::ISystem& sys = ev3dev::default_system) : scheduler_{scheduler}, tool_motor{ev3dev::OUTPUT_A, sys},x_motor{ev3dev::OUTPUT_B, sys}, y_motor{ev3dev::OUTPUT_C, sys} {}

        std::unique_ptr<IWidget::widget_state> widget_;
        bool changed_{false};
        std::optional<homing_results> homed_;
        Scheduler& scheduler_;
        GCodeState gcode_state_;

        button down_button{ev3dev::button::down};
        button up_button{ev3dev::button::up};
        button ok_button{ev3dev::button::enter};

        ev3dev::medium_motor tool_motor;
        ev3dev::large_motor x_motor;
        ev3dev::large_motor y_motor;

        void handle_events();
        void set_widget(std::unique_ptr<IWidget::widget_state> widget);
        bool draw(ev3plotter::display &d, bool force_redraw);
        bool changed();
    };

    std::string print_homing_results(const homing_results &results);

    namespace commands {
        void home(state &s, Scheduler& scheduler, const IWidget &prevWidget, std::function<void(std::variant<homing_results, std::string>)> done);
        void go(state &s, Scheduler& scheduler, std::optional<raw_pos> x, std::optional<raw_pos> y, std::optional<raw_pos> z, std::function<void()> done);
    }

    namespace pos {
        namespace detail {
            raw_pos to_raw(raw_pos min, raw_pos max, normalized_pos val);
            normalized_pos to_norm(raw_pos min, raw_pos max, raw_pos val);

            template <typename T> T constexpr clamp(T min, T max, T val) noexcept {
                return (min < max) ? std::clamp(val, min, max) : std::clamp(val, max, min);
            }
        }
        raw_pos x(const homing_results &h, normalized_pos val);
        raw_pos y(const homing_results &h, normalized_pos val);
        raw_pos z(const homing_results &h, normalized_pos val);

        normalized_pos advanced_x(const state& s, normalized_pos by) noexcept;
        normalized_pos advanced_y(const state& s, normalized_pos by) noexcept;
        normalized_pos advanced_z(const state& s, normalized_pos by) noexcept;

        raw_pos advanced_x(const state& s, raw_pos by) noexcept;
        raw_pos advanced_y(const state& s, raw_pos by) noexcept;
        raw_pos advanced_z(const state& s, raw_pos by) noexcept;

        normalized_pos read_x(const state &s) noexcept;
        normalized_pos read_y(const state& s) noexcept;
        normalized_pos read_z(const state& s) noexcept;

        normalized_pos x_travel(const homing_results &h) noexcept;
        normalized_pos y_travel(const homing_results &h) noexcept;
        normalized_pos z_travel(const homing_results &h) noexcept;
    } // namespace pos
}

#endif // EV3PLOTTER_DRIVER_H_DEFINED