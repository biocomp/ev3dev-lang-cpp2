#include "widgets.h"

#include <fmt/core.h>

using namespace ev3plotter;

class StaticMenu::static_menu_state : public base_menu_state {
public:
    static_menu_state(const StaticMenu& widget) noexcept : widget_{widget} {}

    std::string_view name() const noexcept override {
        return widget_.name_;
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuseless-cast"
    menu_index size() const noexcept override
    {
        return static_cast<menu_index>(static_cast<std::uint32_t>(widget_.items_.size()));
    }
#pragma GCC diagnostic pop

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

std::unique_ptr<IWidget::widget_state> StaticMenu::make() const noexcept {
    return std::make_unique<static_menu_state>(*this);
}

// ######################################
// Message::message_state
// ######################################

class Message::message_state : public widget_state {
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
            while ((pos = current_text_.find('\n', old_pos)) != std::string::npos) {
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

// ##############################
// Message
// ##############################

std::unique_ptr<Message::widget_state> Message::make() const noexcept {
    return std::make_unique<message_state>(*this);
}

void Message::update_text(std::string new_text) {
    text_ = new_text;
    changed_ = true;
}

bool Message::changed() noexcept {
    if (changed_) {
        changed_ = false;
        return true;
    }

    return false;
}

// #########################################
// base_menu_state
// #########################################


bool base_menu_state::handle_event(event event) noexcept {
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

void base_menu_state::draw(ev3plotter::display &d) const noexcept {
    d.fill(false);

    const auto textEndX = d.width - c_menuCutoutPadding - 1;

    const auto curr_size = size();

    // Calculate ideal position of current element:
    const auto items_start_y = c_menuHeaderHeight;
    auto ideal_current_y = items_start_y + (d.height - items_start_y - c_menuItemHeight) / 2;

    // Adjust if here's not enough items below.
    auto visible_items_below_current = (d.height - (ideal_current_y + c_menuItemHeight)) / c_menuItemHeight + 1;
    const auto real_items_below_current = static_cast<int>(curr_size.get() - 1 - current_item_.get());
    if (visible_items_below_current > real_items_below_current) {
        ideal_current_y = d.height - (real_items_below_current + 1) * c_menuItemHeight;
    }

    // Adjusted if there's no enough items above.
    const auto items_above_current = std::min((ideal_current_y - items_start_y) / c_menuItemHeight + 1, static_cast<int>(current_item_.get()));
    ideal_current_y = std::min(ideal_current_y, items_above_current * c_menuItemHeight + items_start_y);


    // Adjust visible items below (ideal_current_y_ could have been changed)
    visible_items_below_current = (d.height - (ideal_current_y + c_menuItemHeight)) / c_menuItemHeight + 1;
    int currentY = ideal_current_y - items_above_current * c_menuItemHeight;

    constexpr const int c_scrollBarWidth{3};

    loop_over_elements(
        current_item_ - menu_index{static_cast<std::uint32_t>(items_above_current)},
        std::min(current_item_ + menu_index{static_cast<std::uint32_t>(visible_items_below_current)}, curr_size),
        [&](auto i, auto name, auto more) {
        if (currentY + c_menuItemHeight <= 0) {
            return true; // Not visible, but we shouldn't stop.
        }

        if (currentY > d.height) {
            return false;
        }

        const bool backgroundColor = (current_item_ == i);
        const bool fontColor = !backgroundColor;

        fill(d, {{0, currentY + 1}, {d.width - c_scrollBarWidth - 2, currentY + c_menuItemHeight}}, backgroundColor);
        print_text(d, {c_menuPadding, currentY + c_menuItemHeight - c_menuPadding}, {{c_menuCutoutPadding, currentY + c_menuCutoutPadding}, {textEndX - c_menuSpaceForMore, currentY + c_menuItemHeight - c_menuCutoutPadding}}, name, fontColor);

        if (more == has_more{true}) {
            print_text(d, {d.width - c_menuSpaceForMore + c_menuPadding, currentY + c_menuItemHeight - c_menuPadding}, ">", fontColor);
        }

        currentY += c_menuItemHeight;
        return true;
    });

    // Draw header
    fill(d, {{0, 0}, {d.width - 1, c_menuHeaderHeight}}, false);
    print_text(d, {c_menuPadding, c_menuHeaderHeight - c_menuPadding}, {{c_menuCutoutPadding, c_menuCutoutPadding}, {textEndX, c_menuHeaderHeight - c_menuCutoutPadding}}, name(), true);

    hline(d, {0, c_menuHeaderHeight + 1}, d.width, true);

    // Draw scroll bar
    const int all_items_height = static_cast<int>(curr_size.get() * c_menuItemHeight);
    const int visible_height = d.height - c_menuHeaderHeight;
    if (all_items_height > visible_height)
    {
        const double scale = static_cast<double>(visible_height) / all_items_height;

        const auto scroll_bar_start = c_menuHeaderHeight + static_cast<int>((static_cast<int>(current_item_.get()) * c_menuItemHeight - ideal_current_y + c_menuHeaderHeight) * scale);
        const auto scroll_bar_height = static_cast<int>(visible_height * scale);

        vline(d, {d.width - 3, c_menuHeaderHeight }, scroll_bar_start - 1, true);
        vline(d, {d.width - 3, scroll_bar_start + scroll_bar_height + 2}, d.height - (scroll_bar_height + 1 + scroll_bar_start), true);
        fill(d, {{d.width - c_scrollBarWidth, scroll_bar_start}, {d.width - 1, scroll_bar_start + scroll_bar_height}}, true);
    }
}

bool base_menu_state::down_pressed() {
    if (current_item_ + menu_index{1} < size()) {
        current_item_ = current_item_ + menu_index{1};
        return true;
    }

    return false;
}

bool base_menu_state::up_pressed() {
    if (current_item_ != menu_index{0}) {
        current_item_ = current_item_ - menu_index{1};
        return true;
    }

    return false;
}

bool base_menu_state::ok_pressed() {
    click(current_item_);
    return true;
}
