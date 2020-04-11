#ifndef EV3PLOTTER_MESSAGEQUEUE_H
#define EV3PLOTTER_MESSAGEQUEUE_H

#include <gsl/span>
#include <memory>
#include <string_view>
#include <type_safe/flag_set.hpp>

namespace ev3plotter {

class message_queue {
  public:
    enum class option { read, write, non_blocking, remove_on_destruction, _flag_set_size };

    enum class send_result { success, failure_queue_full, failure };

    enum class receive_result { success, failure_no_messages, failure };

    message_queue(std::string_view name, std::size_t max_message_size, type_safe::flag_set<option> options);
    ~message_queue();

    send_result send(std::string_view msg);
    receive_result receive(gsl::span<char>& buffer);

    std::size_t message_size() const noexcept;

  private:
    class impl;

    std::unique_ptr<impl> impl_;
};
} // namespace ev3plotter

#endif // EV3PLOTTER_MESSAGEQUEUE_H