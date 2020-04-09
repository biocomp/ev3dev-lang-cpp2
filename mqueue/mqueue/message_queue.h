#ifndef EV3PLOTTER_MESSAGEQUEUE_H
#define EV3PLOTTER_MESSAGEQUEUE_H

#include <memory>
#include <type_safe/flag_set.hpp>

namespace ev3plotter {

    class message_queue {
    public:
        enum class option {
            read,
            write,
            non_blocking,
            _flag_set_size
        };

        message_queue(const char *name, type_safe::flag_set<option> options);

    private:
        class impl;

        std::unique_ptr<impl> impl_;
    };
} // namespace ev3plotter

#endif // EV3PLOTTER_MESSAGEQUEUE_H