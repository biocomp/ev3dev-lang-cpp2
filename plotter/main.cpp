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

struct Motor
{
    Motor(std::string nameSrc, const char* port) : name{std::move(nameSrc)}, motor{port}
    {}

    std::string name;
    ev3dev::large_motor motor;
};

// template <typename T>
// struct Array
// {
//     template <std::size_t N>
//     constexpr Array(T (&arr)[N]) noexcept : data{arr}, size{N}
//     {}

//     T* data;
//     std::size_t size;
// };

template <typename... TMotors>
void drive(int speed, int distance, TMotors&&... motors)
{
    std::string names;
    auto addName{[&names](const auto& motor){ names += " " + motor.name; }};

    {
        std::initializer_list<int> dummy{(addName(motors), 0)...};
        (void)dummy;
    }

    std::cout << "Driving " << names << ", to " << distance << "at speed " << speed << "\n";

    auto startDriving{[distance, speed](Motor& motor){
        motor.motor.set_position_sp(distance).set_speed_sp(speed).run_to_rel_pos();
    }};

    {
        std::initializer_list<int> dummy{(startDriving(motors), 0)...};
        (void)dummy;
    }

    auto running{[](const Motor& motor){
        return motor.motor.state().count("running") != 0;
    }};

    auto allStopped{[&](){
        std::initializer_list<int> runningCounts{running(motors)...};
        return std::accumulate(runningCounts.begin(), runningCounts.end(), 0, [](auto a, auto b){ return a + b; }) == 0;
    }};

    while (!allStopped())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "Done driving " << names << "!\n";
}

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

void wait_for_back_press() {
    bool backPressed{false};
    while (!backPressed)
    {
        backPressed = ev3dev::button::back.pressed ();
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
    }
}

bool verify_device(const ev3dev::lcd& display) {
    if (display.bits_per_pixel() != 1)
    {
        //std::cout << "########\n";
        //std::cout << "display does not have 1 bits per pixel! It has " << display.bits_per_pixel() << ". Failing..." << std::endl;
        //wait_for_back_press();
        //return false;
    }

    return true;
}

namespace {
    enum class menu_index : std::uint32_t {};
    enum class has_more
    {
        yes,
        no
    };

    class state;

    enum class event {
        up,
        down,
        left,
        right,
        ok
    };

    template <typename>
    struct is_strong_def : std::false_type{};

    template <>
    struct is_strong_def<menu_index> : std::true_type{
    };

    #define STRONG_DEF_SFINAE(TType_) \
    std::enable_if_t< \
        is_strong_def< \
            std::remove_reference_t< \
                std::remove_cv_t<TType_> \
            > \
        >::value>* = nullptr

    template <typename TEnum, STRONG_DEF_SFINAE(TEnum)>
    std::underlying_type_t<TEnum> as_int(TEnum e)
    {
        return static_cast<std::underlying_type_t<TEnum>>(e);
    }

    template <typename TEnum, STRONG_DEF_SFINAE(TEnum)>
    TEnum operator+(TEnum a, TEnum b) {
        return TEnum{as_int(a) + as_int(b)};
    }

    template <typename TEnum, STRONG_DEF_SFINAE(TEnum)>
    TEnum operator-(TEnum a, TEnum b) {
        return TEnum{as_int(a) - as_int(b)};
    }

    class IWidget {
    public:
        class widget_state{
        protected:
            widget_state() = default;

        public:
            virtual bool handle_event(event event) noexcept = 0;
            virtual void draw(ev3plotter::display &d) const noexcept = 0;
            virtual ~widget_state() = default;
        };

        virtual ~IWidget() = default;
        virtual std::unique_ptr<widget_state> make() const noexcept = 0;
    };

    constexpr int c_menuHeaderHeight = 30;
    constexpr int c_menuItemHeight = 25;
    constexpr int c_menuPadding = 5;
    constexpr int c_menuCutoutPadding = 1;
    constexpr int c_menuSpaceForMore = 20;

    class base_menu_state : public IWidget::widget_state {
    public:
        virtual std::string_view name() const noexcept = 0;
        virtual menu_index size() const noexcept = 0;
        virtual void loop_over_elements(menu_index from, menu_index to, std::function<bool(menu_index, std::string_view, has_more)> callback) const noexcept = 0;
        virtual void click(menu_index) const noexcept = 0;

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
            loop_over_elements(static_cast<menu_index>(0), curr_size, [&](auto i, auto name, auto /*more*/) {
                if (currentY > d.height)
                {
                    return false;
                }

                const bool backgroundColor = (current_item_ == i);
                const bool fontColor = !backgroundColor;

                fill(d, {{0, currentY}, {d.width - 1, currentY + c_menuItemHeight}}, backgroundColor);
                print_text(d, {c_menuPadding, currentY + c_menuItemHeight - c_menuPadding}, {{c_menuCutoutPadding, currentY + c_menuCutoutPadding}, {textEndX - c_menuSpaceForMore, currentY + c_menuItemHeight - c_menuCutoutPadding}}, name, fontColor);

                hline(d, {c_menuPadding, currentY + c_menuItemHeight + 1}, d.width - c_menuPadding * 2, true);
                currentY += c_menuItemHeight;
                return true;
            });
        }

        private:
            bool down_pressed() {
                if (as_int(current_item_) + 1 < as_int(size())) {
                    current_item_ = current_item_ + menu_index{1};
                    return true;
                }

                return false;
            }

            bool up_pressed() {
                if (as_int(current_item_) != 0) {
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
                auto index = static_cast<decltype(items_)::difference_type>(from);
                for (auto i = widget_.items_.begin() + index; i != widget_.items_.begin() + static_cast<decltype(items_)::difference_type>(to); ++i)
                {
                    if (!callback(static_cast<menu_index>(index), i->name, i->more)) {
                        return;
                    }

                    ++index;
                }
            }

            void click(menu_index index) const noexcept override {
                widget_.items_[static_cast<std::uint32_t>(index)].action();
            }

        private:
            const StaticMenu& widget_;
        };

        struct menu_item {
            std::string name;
            std::function<void()> action;
            has_more more;
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
            message_state(const Message& widget) noexcept : widget_{widget} {}

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
                while ((pos = widget_.text_.find('\n', old_pos)) != std::string::npos)
                {
                    print_line_of_text(std::string_view(widget_.text_).substr(old_pos, pos - old_pos));
                    old_pos = pos + 1;
                }

                print_line_of_text(std::string_view(widget_.text_).substr(old_pos));

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
        };

        Message(std::string header, std::string text, std::string button_caption, std::function<void()> click) :
            header_{std::move(header)}, text_{std::move(text)}, button_caption_{button_caption}, click_{std::move(click)} {}

        std::unique_ptr<widget_state> make() const noexcept override {
            return std::make_unique<message_state>(*this);
        }

    private:
        std::string header_;
        std::string text_;
        std::string button_caption_;
        std::function<void()> click_;
    };

// void draw_something(ev3dev::lcd& lcd) {
//     auto* buffer = lcd.frame_buffer();

//     display d{buffer, static_cast<int>(lcd.resolution_x()), static_cast<int>(lcd.resolution_y())};

//     // std::this_thread::sleep_for(std::chrono::seconds{2});
//     // display.fill(0xff);
//     // std::this_thread::sleep_for(std::chrono::seconds{2});


//     bool backPressed{false};
//     int x = 0;
//     while (!backPressed)
//     {
//         if ((x + 40) >= d.width) {
//             x = 0;
//         }

//         d.fill(false);
//         rectangle(d, {{x, 10}, {x + 40, 10 + 40}}, true);
//         print_text(d, {x + 1, 11}, "ABAB", false);
//         print_text(d, {x + 41, 11}, "ABBAA", true);

//         backPressed = ev3dev::button::back.pressed ();
//         std::this_thread::sleep_for(std::chrono::milliseconds{100});
//         //++x;
//     }
// }

    struct state {
        std::unique_ptr<IWidget::widget_state> widget_;
        bool widget_changed_{true};

        button down_button{ev3dev::button::down};
        button up_button{ev3dev::button::up};
        button ok_button{ev3dev::button::enter};

        ev3dev::medium_motor tool_motor{ev3dev::OUTPUT_A};
        ev3dev::large_motor x_motor{ev3dev::OUTPUT_B};
        ev3dev::large_motor y_motor{ev3dev::OUTPUT_C};

        bool handle_events() {
            bool something_changed{widget_changed()};

            if (down_button.pressed()) {
                something_changed = something_changed || widget_->handle_event(event::down);
            }

            if (up_button.pressed()) {
                something_changed = something_changed || widget_->handle_event(event::up);
            }

            if (ok_button.pressed()) {
                something_changed = something_changed || widget_->handle_event(event::ok);
            }

            return something_changed;
        }

        void set_widget(std::unique_ptr<IWidget::widget_state> widget) {
            widget_ = std::move(widget);
            widget_changed_ = true;
        }

        void draw(ev3plotter::display& d) {
            widget_->draw(d);
        }

        bool widget_changed() {
            if (widget_changed_)
            {
                widget_changed_ = false;
                return true;
            }

            return false;
        }
    };

    void home(state& s, ev3plotter::display& d, const IWidget& prevWidget) {
        bool stop{false};

        //auto wait{[](){std::this_thread::sleep_for(std::second{1});}};

        const auto finish{[&] {
            s.set_widget(prevWidget.make());
            stop = true;
        }};

        Message homing_message{[&](){
            if (s.tool_motor.connected()) {
                return Message{
                    "Homing...",
                    "Please wait while\nall axes have\nhomed\nPress 'ok' to stop.",
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

        s.set_widget(homing_message.make());

        enum
        {
            start_homing,
            homing_tool_down,
        } local_state{start_homing};

        const auto has_state{[&](auto& motor, const auto& state) {
            const auto& current_states = motor.state();
            return current_states.find(state) != current_states.end();
        }};

        while (!stop)
        {
            if (s.handle_events()) {
                s.draw(d);
            }

            switch (local_state) {
            case start_homing:
                s.tool_motor.reset();
                s.tool_motor.set_polarity(s.tool_motor.polarity_normal);
                // s.tool_motor.set_stop_action(s.tool_motor.stop_action_hold);
                // s.tool_motor.set_duty_cycle_sp(40);
                // s.tool_motor.run_direct();
                s.tool_motor.set_speed_sp(100).run_forever();
                local_state = homing_tool_down;
                break;

            case homing_tool_down:
                if (has_state(s.tool_motor, s.tool_motor.state_stalled)) {
                    s.tool_motor.stop();
                    finish();
                }
                break;
            }

            std::this_thread::sleep_for(std::chrono::seconds{2});
        }
    }
}

int main()
{
    bool exit = false;
    ev3dev::lcd display{};

    if (!verify_device(display))
    {
        return -1;
    }

    state s;

    const StaticMenu *main_menu_ptr{};

    StaticMenu exit_menu{
        "Exit?",
        {
            {"yes", [&exit] { exit = true; }, has_more::no},
            {"no",  [&]() { s.set_widget(main_menu_ptr->make()); }, has_more::no }
        }
    };

    Message message{"Please connect motors as follows:",
        "Output A: tool head motor\n"
        "Output B: X motor\n"
        "Output C: Y motor\n",
        "Close", [&]() { s.set_widget(main_menu_ptr->make()); }
    };

    ev3plotter::display d{display.frame_buffer(), static_cast<int>(display.resolution_x()), static_cast<int>(display.resolution_y())};


    StaticMenu main_menu{
        "Main menu",
        {
            {"home", [&]() { home(s, d, *main_menu_ptr); }, has_more::no},
            {"display required connections", [&]() { s.set_widget(message.make()); }, has_more::no},
            {"exit", [&]() { s.set_widget(exit_menu.make()); }, has_more::no }
        }
    };

    main_menu_ptr = &main_menu;

    std::this_thread::sleep_for(std::chrono::milliseconds{1000});

    s.set_widget(main_menu.make());
    s.draw(d);

    while (!exit) {
        //s.handle_events();

        if (s.handle_events())
        {
            s.draw(d);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds{100});
    }

    //draw_something(display);

    //wait_for_back_press();

    //Motor x{"X", ev3dev::OUTPUT_A};
    //Motor y{"Y", ev3dev::OUTPUT_B};

    //x.motor.

    //drive(100, 500, x, y);
    //drive(100, -500, x, y);

    return 0;
}
