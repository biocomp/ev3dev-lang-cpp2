#ifndef EV3PLOTTER_WIDGETS_H_DEFINED
#define EV3PLOTTER_WIDGETS_H_DEFINED

#include "common_definitions.h"
#include "display.h"

namespace ev3plotter {
    using menu_index = StrongInt<std::uint32_t, struct IndexTag>;
    using has_more = fluent::NamedType<bool, struct HasMoreTag, fluent::Comparable>;

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
            virtual void draw(display &d) const noexcept = 0;
            virtual ~widget_state() = default;
        };

        virtual ~IWidget() = default;
        virtual std::unique_ptr<widget_state> make() const noexcept = 0;
    };

    constexpr int c_menuHeaderHeight = 25;
    constexpr int c_menuItemHeight = 20;
    constexpr int c_menuPadding = 4;
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

        bool handle_event(event event) noexcept override;
        void draw(ev3plotter::display &d) const noexcept override;
    private:
        bool down_pressed();
        bool up_pressed();
        bool ok_pressed();

        menu_index current_item_{0};
    };

    class StaticMenu : public IWidget {
    public:
        struct menu_item {
            std::string name;
            std::function<void()> action;
            has_more more{false};
        };

        StaticMenu(std::string name, std::vector<menu_item> items) : name_{std::move(name)}, items_{std::move(items)} {}
        StaticMenu(const StaticMenu &) = delete;
        StaticMenu& operator=(const StaticMenu&) = delete;

        std::unique_ptr<widget_state> make() const noexcept override;

    private:
        class static_menu_state;

        std::string name_;
        std::vector<menu_item> items_;
    };

    class Message : public IWidget {
    public:

        Message(std::string header, std::string text, std::string button_caption, std::function<void()> click) :
            header_{std::move(header)}, text_{std::move(text)}, button_caption_{button_caption}, click_{std::move(click)} {}

        std::unique_ptr<widget_state> make() const noexcept override;

        void update_text(std::string new_text);

    private:
        class message_state;

        bool changed() noexcept;

        std::string header_;
        std::string text_;
        std::string button_caption_;
        std::function<void()> click_;
        bool changed_;
    };
}

#endif // EV3PLOTTER_WIDGETS_H_DEFINED