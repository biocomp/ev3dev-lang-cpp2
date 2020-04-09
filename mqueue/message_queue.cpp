#include "mqueue/message_queue.h"

#include <mqueue.h>

using namespace ev3plotter;

class message_queue::impl {
public:
    impl(const char* name) noexcept {
        queue_ = ::mq_open(name);
    }

private:
    mqd_t queue_;
};

message_queue::message_queue(const char * name) : impl_{std::make_unique<impl>(name)}
}
