#include "mqueue/message_queue.h"

#include <mqueue.h>
#include <exception>
#include <string_view>
#include <fmt/format.h>

using namespace ev3plotter;

class message_queue::impl {
public:
    impl(const char* name, type_safe::flag_set<option> options) {
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

        const ::mq_attr attributes{[options]{
             ::mq_attr a;
             a.mq_flags = options.is_set(option::non_blocking) ? O_NONBLOCK : 0;
             a.mq_maxmsg = 10;
             a.mq_msgsize = 1024;
             a.mq_curmsgs = 0;
             return a;
        }()};

        constexpr ::mode_t permissions{S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH };
        queue_ = ::mq_open(name, open_flags, permissions, &attributes);

        if (queue_ == -1) {
            throw std::runtime_error{fmt::format("Could not open '{}' pipe: {}", name, errno)};
        }
    }

    void send(std::string_view msg) {
        ::mq_send(queue_, msg.data(), msg.size(), 0);
    }

    void receive(std::span)

    ~impl() {
        ::mq_close(queue_);
    }

private:
    ::mqd_t queue_;
};

message_queue::message_queue(const char * name, type_safe::flag_set<option> options) : impl_{std::make_unique<impl>(name, options)} {
}

message_queue::~message_queue() = default;