#include "mqueue/message_queue.h"

#include <exception>
#include <fmt/format.h>
#include <mqueue.h>
#include <string_view>

using namespace ev3plotter;

class message_queue::impl {
  public:
    impl(std::string_view name, std::size_t max_message_size, type_safe::flag_set<option> options)
        : name_{name}
        , remove_on_destruction_{options.is_set(option::remove_on_destruction)} {
        int open_flags{O_CREAT}; // Create if doesn't exist
        if (options.is_set(option::read) && options.is_set(option::write)) {
            open_flags |= O_RDWR;
        } else if (options.is_set(option::read)) {
            open_flags |= O_RDONLY;
        } else if (options.is_set(option::write)) {
            open_flags |= O_WRONLY;
        }

        if (options.is_set(option::non_blocking)) {
            open_flags |= O_NONBLOCK;
        }

        {
            const ::mq_attr attributes{[&] {
                ::mq_attr a;
                a.mq_flags = options.is_set(option::non_blocking) ? O_NONBLOCK : 0;
                a.mq_maxmsg = 10;
                a.mq_msgsize = static_cast<long>(max_message_size);
                a.mq_curmsgs = 0;
                return a;
            }()};

            // constexpr ::mode_t permissions{S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH };
            constexpr ::mode_t permissions{0660};
            queue_ = ::mq_open(name_.c_str(), open_flags, permissions, &attributes);

            if (queue_ == -1) {
                throw std::runtime_error{
                    fmt::format("Could not open '{}' pipe: {}({})", name_, errno, std::strerror(errno))};
            }
        }

        ::mq_attr attributes;
        if (::mq_getattr(queue_, &attributes) == -1) {
            throw std::runtime_error{fmt::format(
                "Could not read attributes of successfully opened '{}' pipe: {}({})",
                name_,
                errno,
                std::strerror(errno))};
        }
        message_size_ = static_cast<std::size_t>(attributes.mq_msgsize);
    }

    send_result send(std::string_view msg) {
        if (::mq_send(queue_, msg.data(), msg.size(), 0) == -1) {
            return errno == EAGAIN ? send_result::failure_queue_full : send_result::failure;
        }

        return send_result::success;
    }

    receive_result receive(gsl::span<char>& buffer) {
        const auto msg_size = ::mq_receive(queue_, buffer.data(), buffer.size(), nullptr);
        if (msg_size == -1) {
            return errno == EAGAIN ? receive_result::failure_no_messages : receive_result::failure;
        }

        buffer = {buffer.data(), static_cast<std::size_t>(msg_size)};
        return receive_result::success;
    }

    std::size_t message_size() const noexcept { return message_size_; }

    ~impl() {
        ::mq_close(queue_);
        if (remove_on_destruction_) {
            ::mq_unlink(name_.c_str());
        }
    }

  private:
    std::string name_;
    ::mqd_t queue_;
    bool remove_on_destruction_;
    std::size_t message_size_{0};
};

message_queue::message_queue(std::string_view name, std::size_t max_message_size, type_safe::flag_set<option> options)
    : impl_{std::make_unique<impl>(name, max_message_size, options)} {}

message_queue::~message_queue() = default;

message_queue::send_result message_queue::send(std::string_view msg) { return impl_->send(msg); }

message_queue::receive_result message_queue::receive(gsl::span<char>& buffer) { return impl_->receive(buffer); }

std::size_t message_queue::message_size() const noexcept { return impl_->message_size(); }