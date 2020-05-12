#ifndef EV3PLOTTER_SERVER_H_DEFINED
#define EV3PLOTTER_SERVER_H_DEFINED

#include "common_definitions.h"

#include <cstddef>
#include <functional>
#include <mqueue/message_queue.h>
#include <optional>
#include <variant>

namespace ev3plotter {
class state;
class Scheduler;

enum class GCodeCommand { 
    Unknown,
    Go 
};

struct ServerMessage {
    GCodeCommand Command{GCodeCommand::Unknown};
    std::string Raw;

    std::optional<mm_pos> X;
    std::optional<mm_pos> Y{0};
    std::optional<mm_pos> Z{0};
    std::optional<mm_pos> F{0};
};

struct HandlerError {
    std::string Error;
};

namespace detail {
    struct ParseError {
        std::string Error;
    };

    std::variant<ServerMessage, ParseError> parse_message(std::string_view message);
} // namespace detail

class Server {
    static constexpr const std::size_t c_maxMessageSize{256};

  public:
    Server();

    void
    handle_events(std::function<void(ServerMessage, std::function<void(const std::optional<HandlerError>&)>)> handler);

  private:
    message_queue read_queue_;
    message_queue write_queue_;
};
} // namespace ev3plotter

#endif // EV3PLOTTER_SERVER_H_DEFINED
