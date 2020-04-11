#include <fmt/format.h>
#include <mqueue/message_queue.h>
#include <thread>

using namespace ev3plotter;

int main(int argc, const char**) {

    constexpr const std::size_t c_maxMessageSize{256};
    if (argc == 2) {
        fmt::print("Got 2 arguments - server\n");

        // return run_server();

        message_queue to_srv{"/ev3plotter_input",
                             c_maxMessageSize,
                             message_queue::option::read | message_queue::option::remove_on_destruction |
                                 message_queue::option::non_blocking};
        message_queue from_srv{"/ev3plotter_output",
                               c_maxMessageSize,
                               message_queue::option::write | message_queue::option::remove_on_destruction |
                                   message_queue::option::non_blocking};

        std::vector<char> buffer_arr(to_srv.message_size());

        bool exit{false};
        while (! exit) {
            gsl::span<char> buffer{buffer_arr};
            switch (to_srv.receive(buffer)) {
            case message_queue::receive_result::success: {
                const std::string_view message{buffer.data(), buffer.size()};
                fmt::print("Received '{}'\n", message);

                if (message == "stop!") {
                    from_srv.send("stopping!");
                    fmt::print("'stop!' received, exiting\n");
                    exit = true;
                    break;
                }
                from_srv.send("ack!");
            } break;

            case message_queue::receive_result::failure_no_messages:
                break;

            case message_queue::receive_result::failure: {
                const char* err = std::strerror(errno);
                fmt::print("Not received: {}\n", err);
            }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds{500});
        }

        fmt::print("Exited!\n");
    } else {
        fmt::print("Got {} arguments - client\n", argc);

        //        return run_client();

        message_queue to_srv{"/ev3plotter_input",
                             c_maxMessageSize,
                             message_queue::option::write | message_queue::option::non_blocking};
        message_queue from_srv{"/ev3plotter_output",
                               c_maxMessageSize,
                               message_queue::option::read | message_queue::option::non_blocking};

        fmt::print("Enter message: ");

        std::vector<char> from_srv_buffer_arr(from_srv.message_size());
        char temp_buf[c_maxMessageSize + 1];

        bool retry{false};
        while (retry || ::fgets(temp_buf, std::size(temp_buf), stdin)) {
            retry = false;
            const std::string_view message_with_newline{temp_buf, std::strlen(temp_buf)};
            const auto message{message_with_newline.substr(0, message_with_newline.size() - 1)};

            fmt::print("sending '{}'...\n", message);
            switch (to_srv.send(message)) {
                case message_queue::send_result::success:
                    fmt::print("Send succeeded\n");
                    break;

                case message_queue::send_result::failure_queue_full:
                    fmt::print("Queue full, could not send, retrying...");
                    retry = true;
                    std::this_thread::sleep_for(std::chrono::milliseconds{500});
                    break;

                case message_queue::send_result::failure:
                    fmt::print("Send failed!");
                    break;
                }

            gsl::span<char> from_srv_buffer{from_srv_buffer_arr};
            while (from_srv.receive(from_srv_buffer) != message_queue::receive_result::success) {
                std::this_thread::sleep_for(std::chrono::milliseconds{500});
            }

            fmt::print("... response: {}\n", std::string_view{from_srv_buffer.data(), from_srv_buffer.size()});

            fmt::print("Enter message: ");
        }
    }
}