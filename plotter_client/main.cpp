#include <mqueue/message_queue.h>
#include <fmt/format.h>

using namespace ev3plotter;

int main(int argc, const char**) {
    if (argc == 2) {
        fmt::print("Got 2 arguments - server\n");

        message_queue to_srv{"/ev3plotter_input", message_queue::option::read};
        message_queue from_srv{"/ev3plotter_output", message_queue::option::write};
    } else {
        fmt::print("Got {} arguments - client\n", argc);

        message_queue to_srv{"/ev3plotter_input", message_queue::option::write};
        message_queue from_srv{"/ev3plotter_output", message_queue::option::read};
    }
}