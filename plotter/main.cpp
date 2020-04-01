/*
 * Copyright (c) 2014 - Franz Detro
 *
 * Some real world test program for motor control
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "menu.h"
#include "display.h"

using namespace ev3plotter;
#include <thread>
#include <chrono>
#include <string>
#include <numeric>
#include <iostream>
#include <functional>
#include <string_view>
#include <type_traits>

#include <fmt/core.h>
#include <fmt/ostream.h>

#include <named_type/named_type.hpp>

#include <type_traits>

namespace {
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

    template <typename T, typename TTag>
    using StrongInt = fluent::NamedType<T, TTag, fluent::Addable, fluent::Subtractable, fluent::Comparable, fluent::Incrementable, fluent::Hashable, fluent::Printable>;

    using menu_index = StrongInt<std::uint32_t, struct IndexTag>;
    using raw_pos = StrongInt<int, struct RawPosTag>;
    using normalized_pos = StrongInt<int, struct NormalizedTag>;
    using mm_pos = StrongInt<int, struct MmPosTag>;

    using has_more = fluent::NamedType<bool, struct HasMoreTag, fluent::Comparable>;

    class state;

    enum class event {
        up,
        down,
        left,
        right,
        ok
    };

    class IWidget {
    public:
        class widget_state{
        protected:
            widget_state() = default;

        public:
            virtual bool changed() noexcept = 0;
            virtual bool handle_event(event event) noexcept = 0;
            virtual void draw(ev3plotter::display &d) const noexcept = 0;
            virtual ~widget_state() = default;
        };

        virtual ~IWidget() = default;
        virtual std::unique_ptr<widget_state> make() const noexcept = 0;
    };

    constexpr int c_menuHeaderHeight = 25;
    constexpr int c_menuItemHeight = 20;
    constexpr int c_menuPadding = 3;
    constexpr int c_menuCutoutPadding = 0;
    constexpr int c_menuSpaceForMore = 20;

    class base_menu_state : public IWidget::widget_state {
    public:
        virtual std::string_view name() const noexcept = 0;
        virtual menu_index size() const noexcept = 0;
        virtual void loop_over_elements(menu_index from, menu_index to, std::function<bool(menu_index, std::string_view, has_more)> callback) const noexcept = 0;
        virtual void click(menu_index) const noexcept = 0;

        bool changed() noexcept override {
            return false;
        }

        bool handle_event(event event) noexcept override {
            switch (event) {
                case event::down:
                    return down_pressed();
                case event::up:
                    return up_pressed();
                case event::ok:
                    return ok_pressed();
                default:
                    return false;
            }
        }

        void draw(ev3plotter::display &d) const noexcept override {
            d.fill(false);

            int currentY = 0;
            const auto textEndX = d.width - c_menuCutoutPadding - 1;
            print_text(d, {c_menuPadding, currentY + c_menuHeaderHeight - c_menuPadding}, {{c_menuCutoutPadding, currentY + c_menuCutoutPadding}, {textEndX, currentY + c_menuHeaderHeight - c_menuCutoutPadding}}, name(), true);

            hline(d, {c_menuPadding, currentY + c_menuHeaderHeight + 1}, d.width - c_menuPadding * 2, true);
            currentY += c_menuHeaderHeight;

            const auto curr_size = size();
            loop_over_elements(static_cast<menu_index>(0), curr_size, [&](auto i, auto name, auto more) {
                if (currentY > d.height)
                {
                    return false;
                }

                const bool backgroundColor = (current_item_ == i);
                const bool fontColor = !backgroundColor;

                fill(d, {{0, currentY}, {d.width - 1, currentY + c_menuItemHeight}}, backgroundColor);
                print_text(d, {c_menuPadding, currentY + c_menuItemHeight - c_menuPadding}, {{c_menuCutoutPadding, currentY + c_menuCutoutPadding}, {textEndX - c_menuSpaceForMore, currentY + c_menuItemHeight - c_menuCutoutPadding}}, name, fontColor);

                if (more == has_more{true}) {
                    print_text(d, {d.width - c_menuSpaceForMore + c_menuPadding, currentY + c_menuItemHeight - c_menuPadding}, ">", fontColor);
                }


                hline(d, {c_menuPadding, currentY + c_menuItemHeight + 1}, d.width - c_menuPadding * 2, true);
                currentY += c_menuItemHeight;
                return true;
            });
        }

        private:
            bool down_pressed() {
                if (current_item_ + menu_index{1} < size()) {
                    current_item_ = current_item_ + menu_index{1};
                    return true;
                }

                return false;
            }

            bool up_pressed() {
                if (current_item_ != menu_index{0}) {
                    current_item_ = current_item_ - menu_index{1};
                    return true;
                }

                return false;
            }

            bool ok_pressed() {
                click(current_item_);
                return true;
            }

            menu_index current_item_{0};
    };

    class StaticMenu : public IWidget {
    public:
        class static_menu_state : public base_menu_state {
        public:
            static_menu_state(const StaticMenu& widget) noexcept : widget_{widget} {}

            std::string_view name() const noexcept override {
                return widget_.name_;
            }

            menu_index size() const noexcept override
            {
                return static_cast<menu_index>(widget_.items_.size());
            }

            void loop_over_elements(menu_index from, menu_index to, std::function<bool(menu_index, std::string_view, has_more)> callback) const noexcept  override {
                auto index = static_cast<decltype(items_)::difference_type>(from.get());
                for (auto i = widget_.items_.begin() + index; i != widget_.items_.begin() + static_cast<decltype(items_)::difference_type>(to.get()); ++i)
                {
                    if (!callback(menu_index{static_cast<std::uint32_t>(index)}, i->name, i->more)) {
                        return;
                    }

                    ++index;
                }
            }

            void click(menu_index index) const noexcept override {
                widget_.items_[index.get()].action();
            }

        private:
            const StaticMenu& widget_;
        };

        struct menu_item {
            std::string name;
            std::function<void()> action;
            has_more more{false};
        };

        StaticMenu(std::string name, std::vector<menu_item> items) : name_{std::move(name)}, items_{std::move(items)} {}
        StaticMenu(const StaticMenu &) = delete;
        StaticMenu& operator=(const StaticMenu&) = delete;

        std::unique_ptr<widget_state> make() const noexcept override {
            return std::make_unique<static_menu_state>(*this);
        }

    private:
        std::string name_;
        std::vector<menu_item> items_;
    };

    class Message : public IWidget {
    public:
        class message_state : public widget_state {
        public:
            message_state(const Message& widget) noexcept : widget_{widget}, current_text_{widget_.text_} {}

            bool changed() noexcept override {
                if (current_text_ != widget_.text_) {
                    //printf("Message with text '%s' changed to '%s'", current_text_.c_str(), widget_.text_.c_str());
                    current_text_ = widget_.text_;
                    return true;
                }

                return false;
            }

            bool handle_event(event event) noexcept override {
                switch (event) {
                    case event::ok:
                        widget_.click_();
                        return true;

                    default:
                        return false;
                }
            }

            void draw(ev3plotter::display& d) const noexcept override {
                d.fill(false);

                // Header
                print_text(d, {c_menuPadding, c_menuHeaderHeight - c_menuPadding}, widget_.header_, true);
                int y = c_menuHeaderHeight;

                // Lines of main text
                const auto print_line_of_text{[&y, &d](std::string_view text){
                    print_text(d, {c_menuPadding, y + c_menuItemHeight - c_menuPadding}, text, true);
                    y += c_menuItemHeight;
                }};

                std::size_t old_pos{0};
                std::size_t pos{0};
                while ((pos = current_text_.find('\n', old_pos)) != std::string::npos)
                {
                    print_line_of_text(std::string_view(current_text_).substr(old_pos, pos - old_pos));
                    old_pos = pos + 1;
                }

                print_line_of_text(std::string_view(current_text_).substr(old_pos));

                // Button
                const int button_width = d.width / 3;
                const int button_height = c_menuItemHeight;

                const rect buttonRect{
                    {(d.width - button_width) / 2, d.height - 1 - button_height},
                    {(d.width - button_width) / 2 + button_width, d.height - 1 }};
                fill(d, buttonRect, true);
                print_text(
                    d,
                    {buttonRect.topLeft.x + c_menuPadding, buttonRect.bottomRight.y - c_menuPadding},
                    buttonRect,
                    widget_.button_caption_,
                    false);
            }

        private:
            const Message& widget_;
            std::string current_text_;
        };

        Message(std::string header, std::string text, std::string button_caption, std::function<void()> click) :
            header_{std::move(header)}, text_{std::move(text)}, button_caption_{button_caption}, click_{std::move(click)} {}

        std::unique_ptr<widget_state> make() const noexcept override {
            return std::make_unique<message_state>(*this);
        }

        void update_text(std::string new_text) {
            text_ = new_text;
            changed_ = true;
        }

    private:
        bool changed() noexcept {
            if (changed_) {
                changed_ = false;
                return true;
            }

            return false;
        }
        std::string header_;
        std::string text_;
        std::string button_caption_;
        std::function<void()> click_;
        bool changed_;
    };

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

        void handle_events() {
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

        void set_widget(std::unique_ptr<IWidget::widget_state> widget) {
            widget_ = std::move(widget);
            changed_ = true;
        }

        void draw(ev3plotter::display& d) {
            if (changed()) {
                widget_->draw(d);
            }
        }

        bool changed() {
            if (changed_ || widget_->changed()) {
                changed_ = false;
                return true;
            }

            return false;
        }
    };

    std::string print_homing_results(const homing_results& results) {
        return fmt::format(
             "X: [{}, {}]-> {}\n"
             "Y: [{}, {}]-> {}\n"
             "Tool: [{}, {}]-> {}\n",
             results.x_min, results.x_max, std::abs((results.x_min - results.x_max).get()),
             results.y_min, results.y_max, std::abs((results.y_min - results.y_max).get()),
             results.tool_up_pos, results.tool_down_pos, std::abs((results.tool_up_pos - results.tool_down_pos).get()));
    }

    homing_results home(state& s, ev3plotter::display& d, const IWidget& prevWidget) {
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

        //auto wait{[](){std::this_thread::sleep_for(std::second{1});}};

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

        s.set_widget(homing_message.make());


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
        }};

        const auto finish_homing{[&](auto &motor, auto& store_pos) {
            if (stalled(motor))
            {
                motor.stop();
                store_pos = raw_pos{motor.position()};
                std::this_thread::sleep_for(std::chrono::milliseconds{500});
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

        while (true)
        {
            s.handle_events();
            s.draw(d);

            switch (local_state)
            {
            case home::start:
                start_motor(s.tool_motor, -30);
                local_state = home::homing_tool_up;
                break;

            case home::homing_tool_up:
                if (finish_homing(s.tool_motor, results.tool_up_pos)) {
                    start_homing(s.x_motor, +50, "x min (left)", home::homing_x_left);
                }
                break;

            case home::homing_x_left:
                if (finish_homing(s.x_motor, results.x_min)) {
                    start_homing(s.x_motor, -50, "x max (right)", home::homing_x_right);
                }
                break;

            case home::homing_x_right:
                if (finish_homing(s.x_motor, results.x_max)) {
                    start_homing(s.y_motor, -40, "y min", home::homing_y_min);
                }
                break;

            case home::homing_y_min:
                if (finish_homing(s.y_motor, results.y_min)) {
                    start_homing(s.y_motor, +40, "y max", home::homing_y_max);
                }
                break;

            case home::homing_y_max:
                if (finish_homing(s.y_motor, results.y_max)) {
                    local_state = home::go_for_homing_tool_down;
                }
                break;

            case home::go_for_homing_tool_down:
                start_homing(s.tool_motor, +20, "tool down", home::homing_tool_down);
                break;

            case home::homing_tool_down:
                if (finish_homing(s.tool_motor, results.tool_down_pos)) {
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

    namespace commands {
        void go(state& s, std::optional<raw_pos> x, std::optional<raw_pos> y, std::optional<raw_pos> z) {
            bool stop{false};

            if (x) {
                s.x_motor.set_speed_sp(100).set_position_sp(x->get()).run_to_abs_pos();
            }

            if (y) {
                s.y_motor.set_speed_sp(100).set_position_sp(y->get()).run_to_abs_pos();
            }

            if (z) {
                s.tool_motor.set_speed_sp(100).set_position_sp(z->get()).run_to_abs_pos();
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
    }

    namespace pos {
        namespace detail {
            raw_pos to_raw(raw_pos min, raw_pos max, normalized_pos val) {
                return (min < max)
                    ? std::clamp(min + raw_pos{val.get()}, min, max)
                    : std::clamp(min - raw_pos{val.get()}, max, min);
            }

            normalized_pos to_norm(raw_pos min, raw_pos max, raw_pos val) {
            return normalized_pos{((min < max)
                ? std::clamp(val - min, raw_pos{0}, max - min)
                : std::clamp(val + min, raw_pos{0}, min - max)).get()};
            }
        }

        raw_pos x(const homing_results& h, normalized_pos val) {
            return detail::to_raw(h.x_min, h.x_max, val);
        }

        raw_pos y(const homing_results& h, normalized_pos val) {
            return detail::to_raw(h.y_min, h.y_max, val);
        }

        raw_pos z(const homing_results& h, normalized_pos val) {
            return detail::to_raw(h.tool_up_pos, h.tool_down_pos, val);
        }

        normalized_pos read_x(const state& s) noexcept {
            return detail::to_norm(s.homed_->y_min, s.homed_->x_max, raw_pos{s.x_motor.position()});
        }

        normalized_pos read_y(const state& s) noexcept {
            return detail::to_norm(s.homed_->y_min, s.homed_->y_max, raw_pos{s.y_motor.position()});
        }

        normalized_pos z_travel(const homing_results& h) noexcept {
            return normalized_pos{std::abs((h.tool_down_pos - h.tool_up_pos).get())};
        }
    }
}


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

    const IWidget* show_homing_limits_return_widget = main_menu_ptr;
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
            { "Go to x0", [&]{ if_homed([&]{ commands::go(s, pos::x(*s.homed_, normalized_pos{0}), {}, {});}); } },
            { "Drive x 300", [&]{ if_homed([&]{ commands::go(s, pos::x(*s.homed_, pos::read_x(s) + normalized_pos{300}), {}, {}); }); } },
            { "Go to y0", [&]{ if_homed([&]{ commands::go(s, {}, pos::y(*s.homed_, normalized_pos{0}), {}); }); } },
            { "Drive y 300", [&]{ if_homed([&]{ commands::go(s, {}, pos::y(*s.homed_, pos::read_y(s) + normalized_pos{300}), {}); }); } },
            { "Tool up", [&]{ if_homed([&]{ commands::go(s, {}, {}, pos::z(*s.homed_, normalized_pos{0})); }); } },
            { "Tool down", [&]{ if_homed([&]{ commands::go(s, {}, {}, pos::z(*s.homed_, normalized_pos{0}) + raw_pos{pos::z_travel(*s.homed_).get()}); }); } }
        }
    };

    utilities_menu_ptr = &utilities_menu;

    StaticMenu main_menu{
        "Main menu", {
            {"home", [&]() {
                    s.homed_ = home(s, d, *main_menu_ptr);
                    show_homing_results.update_text(print_homing_results(*s.homed_));
                }},
            {"display required connections", [&]() { s.set_widget(message.make()); }},
            {"show homing results", [&](){ s.set_widget(show_homing_results.make()); } },
            {"utilities", [&]{ s.set_widget(utilities_menu.make()); }, has_more{true}},
            {"exit", [&]() { s.set_widget(exit_menu.make()); } }
        }
    };


    main_menu_ptr = &main_menu;

    std::this_thread::sleep_for(std::chrono::milliseconds{1000});

    s.set_widget(main_menu.make());
    s.draw(d);

    while (!exit) {
        s.handle_events();
        s.draw(d);
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
    }

    return 0;
}
