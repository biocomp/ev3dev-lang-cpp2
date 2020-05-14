#include "server.h"

#include <fmt/format.h>

#include <cassert>

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

void parse_val(std::string_view val, double& result) { 
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

    result = temp_result;
}


ServerMessage read_home_command(std::string_view rest) {
    ServerMessage m;
    
    m.Command = GCodeCommand::Home;
    for_each_token(rest, [&](auto token) {
        if (token == "X") {
            m.X = 1;
        } else if (token == "Y") {
            m.Y = 1;
        } else if (token == "Z") {
            m.Z = 1;
        } else {
            throw detail::ParseError{fmt::format("Unexpected value '{}' in G28 command", token)};
        }
    });

    return m;
}
} // namespace

std::variant<ServerMessage, detail::ParseError> detail::parse_message(std::string_view message) {
    struct CheckResult {
        std::string_view rest;
        GCodeCommand command;
    };

    try {
        const auto checkCommand{[message]() -> std::optional<CheckResult> { 
            if (message.size() > 0) {
                const auto letter{message[0]};
                const auto rest{message.substr(1)};
                auto spacePos{rest.find(' ')};

                std::string_view remaining;
                if (spacePos == std::string_view::npos) {
                    spacePos = rest.size();
                    remaining = {};
                } else {
                    remaining = {rest.substr(spacePos + 1)};
                }

                const std::string numberStr{rest.substr(0, spacePos)};
                
                try {
                    if (letter == 'G') {
                        switch (std::stoi(numberStr)) {
                        case 0:
                        case 1:
                            return CheckResult{remaining, GCodeCommand::Go};
                        case 20:
                            return CheckResult{remaining, GCodeCommand::UseInches};
                        case 21:
                            return CheckResult{remaining, GCodeCommand::UseMm};
                        case 28:
                            return CheckResult{remaining, GCodeCommand::Home};
                        case 90:
                            return CheckResult{remaining, GCodeCommand::AbsolutePositioning};
                        case 91:
                            return CheckResult{remaining, GCodeCommand::RelativePositioning};
                        }
                    }
                }
                catch (const std::invalid_argument& e) {
                    throw detail::ParseError{"Could not parse '" + numberStr + "' command number"};
                }
            }

            return {};
        }};

        const auto readGo{[&] (auto rest){
            ServerMessage result;
            result.Command = GCodeCommand::Go;
            for_each_token(rest, [&](auto token) {
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
        }};

        const auto simpleCommand{[] (auto command) {
            ServerMessage m{};
            m.Command = command;
            return m;
        }};

        if (const auto checkResult = checkCommand()) {
            switch (checkResult->command) {
            case GCodeCommand::Go:
                return readGo(checkResult->rest);
            case GCodeCommand::UseInches:
            case GCodeCommand::UseMm:
                return simpleCommand(checkResult->command);
            case GCodeCommand::Home:
                return read_home_command(checkResult->rest);
            case GCodeCommand::AbsolutePositioning:
            case GCodeCommand::RelativePositioning:
                return simpleCommand(checkResult->command);
            case GCodeCommand::Unknown:
                assert("Unhandled case");
            }
        }
    }
    catch (const detail::ParseError& e) { return e; }
    catch (const std::invalid_argument& e) {
        return detail::ParseError{e.what()};
    }
    
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
            handler(parsed, [&, parsed, messageCopy = std::string{message}](const auto& handlerError) {
                if (! handlerError) {
                    write_queue_.send(fmt::format("Done handling '{}'", messageCopy));
                } else {
                    write_queue_.send(fmt::format("Failed handling: '{}': {}", messageCopy, handlerError->Error));
                }
            });
        } else {
            write_queue_.send(
                fmt::format("Failed to parse '{}': {}", message, std::get<detail::ParseError>(result).Error));
        }
    }
}