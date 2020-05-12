#include <server.h>
#include <catch2.hpp>

using namespace ev3plotter;

TEST_CASE("Parsing unknown command fails") {
    auto result{detail::parse_message("unknown")};
    REQUIRE(result.index() == 1);
    REQUIRE(std::get<detail::ParseError>(result).Error == "Unknown GCode command 'unknown'");
}

TEST_CASE("Parsing G0") {
    auto result{detail::parse_message("G0 X1.1 Y2.2 Z3.3 F100.1")};
    REQUIRE(result.index() == 0);

    const auto& message{std::get<ServerMessage>(result)};
    REQUIRE(message.Command == GCodeCommand::Go);
    REQUIRE(message.X->get() == 1.1f);
    REQUIRE(message.Y->get() == 2.2f);
    REQUIRE(message.Z->get() == 3.3f);
    REQUIRE(message.F->get() == 100.1f);
}