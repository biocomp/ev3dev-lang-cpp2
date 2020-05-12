#include "server.h"

#include <fmt/format.h>

using namespace ev3plotter;

namespace {

class ServerEvent {
    std::string payload;
    std::function<void()> finished;
};

template <typename TCallback>
void for_each_token(std::string_view sv, TCallback callback) { 
    std::size_t prev_pos{0};
    while (prev_pos != std::string_view::npos && prev_pos != sv.size()) {
        const auto next_pos{sv.find(' ', prev_pos)};

        if (next_pos - prev_pos != 0) {
            callback(sv.substr(prev_pos, next_pos - prev_pos));
        }

        prev_pos = next_pos == std::string_view::npos ? std::string_view::npos : next_pos + 1;
    }
}

void parse_val(std::string_view val, mm_pos& result) { 
    if (val.size() >= 64) {
        throw detail::ParseError{fmt::format("Value '{}' is too long", val)};
    }

    float temp_result;
    char buffer[64];
    std::memcpy(buffer, val.data(), val.size());
    buffer[val.size()] = '\0';
    if (std::sscanf(buffer, "%f", &temp_result) != 1) {
        throw detail::ParseError{fmt::format("Could not parse '{}' into float", val)};
    }

    result = mm_pos{temp_result};
}

} // namespace

std::variant<ServerMessage, detail::ParseError> detail::parse_message(std::string_view message) {
    try {
        const auto checkCommand{[message](std::string_view with) -> std::optional<std::string_view> { 
            if (message.substr(0, with.size()) == with) {
                return message.substr(with.size());
            }

            return {};
        }};

        if (auto rest = checkCommand("G0")) {
            ServerMessage result;
            result.Command = GCodeCommand::Go;
            for_each_token(*rest, [&](auto token) {
                switch (token[0]) {
                case 'X':
                    parse_val(token.substr(1), result.X.emplace(0));
                    break;
                case 'Y':
                    parse_val(token.substr(1), result.Y.emplace(0));
                    break;
                case 'Z':
                    parse_val(token.substr(1), result.Z.emplace(0));
                    break;
                case 'F':
                    parse_val(token.substr(1), result.F.emplace(0));
                    break;
                }
            });

            return result;
        }
    }
    catch (const detail::ParseError& e) { return e; }
    
    return detail::ParseError{fmt::format("Unknown GCode command '{}'", message)};
}

Server::Server()
    : read_queue_{"/ev3plotter_input", c_maxMessageSize, message_queue::option::read | message_queue::option::remove_on_destruction}
    , write_queue_{"/ev3plotter_output", c_maxMessageSize, message_queue::option::write} {}

void Server::handle_events(
    std::function<void(ServerMessage, std::function<void(const std::optional<HandlerError>&)>)> handler) {
    std::array<char, c_maxMessageSize> buffer;
    gsl::span bufferSpan{buffer};

    while (read_queue_.receive(bufferSpan) == message_queue::receive_result::success) {
        std::string_view message{bufferSpan.data(), bufferSpan.size()};

        auto result{detail::parse_message(message)};
        if (result.index() == 0) {
            auto parsed{std::get<ServerMessage>(result)};
            handler(parsed, [&, parsed](const auto& handlerError) {
                if (! handlerError) {
                    write_queue_.send(fmt::format("Done handling '{}'", parsed.Raw));
                } else {
                    write_queue_.send(fmt::format("Failed handling: '{}': {}", parsed.Raw, handlerError->Error));
                }
            });
        } else {
            write_queue_.send(
                fmt::format("Failed to parse '{}': {}", message, std::get<detail::ParseError>(result).Error));
        }
    }
}