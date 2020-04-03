#ifndef EV3PLOTTER_DRIVER_H_DEFINED
#define EV3PLOTTER_DRIVER_H_DEFINED

#include <ev3dev.h>
#include "common_definitions.h"
#include "widgets.h"

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

    using raw_pos = StrongInt<int, struct RawPosTag>;
    using normalized_pos = StrongInt<int, struct NormalizedTag>;
    using mm_pos = StrongInt<int, struct MmPosTag>;

    struct homing_results{
        raw_pos tool_up_pos{0};
        raw_pos tool_down_pos{0};

        raw_pos x_min{0};
        raw_pos x_max{0};

        raw_pos y_min{0};
        raw_pos y_max{0};
    };

    struct state {
        std::unique_ptr<IWidget::widget_state> widget_;
        bool changed_{false};
        std::optional<homing_results> homed_;

        button down_button{ev3dev::button::down};
        button up_button{ev3dev::button::up};
        button ok_button{ev3dev::button::enter};

        ev3dev::medium_motor tool_motor{ev3dev::OUTPUT_A};
        ev3dev::large_motor x_motor{ev3dev::OUTPUT_B};
        ev3dev::large_motor y_motor{ev3dev::OUTPUT_C};

        void handle_events();
        void set_widget(std::unique_ptr<IWidget::widget_state> widget);
        bool draw(ev3plotter::display &d, bool force_redraw);
        bool changed();
    };

    std::string print_homing_results(const homing_results &results);

    namespace commands {
        homing_results home(state &s, ev3plotter::display &d, const IWidget &prevWidget);
        void go(state &s, std::optional<raw_pos> x, std::optional<raw_pos> y, std::optional<raw_pos> z);
    }

    namespace pos {
        namespace detail {
            raw_pos to_raw(raw_pos min, raw_pos max, normalized_pos val);
            normalized_pos to_norm(raw_pos min, raw_pos max, raw_pos val);
        }
        raw_pos x(const homing_results &h, normalized_pos val);
        raw_pos y(const homing_results &h, normalized_pos val);
        raw_pos z(const homing_results &h, normalized_pos val);
        normalized_pos read_x(const state &s) noexcept;
        normalized_pos read_y(const state& s) noexcept;
        normalized_pos read_z(const state& s) noexcept;
        normalized_pos z_travel(const homing_results &h) noexcept;
    } // namespace pos
}

#endif // EV3PLOTTER_DRIVER_H_DEFINED