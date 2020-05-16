#include <server.h>
#include <catch2.hpp>

#include <fmt/format.h>

using namespace ev3plotter;
using namespace Catch::literals;

namespace {
    ServerMessage parse_and_expect_message(std::string_view message) {
        INFO("Message: " << message);
        const auto result{detail::parse_message(message)};
        REQUIRE(result.index() == 0);
        return std::get<ServerMessage>(result);
    }

    void parse_and_expect_error(std::string_view message, std::string_view expectedError) {
        INFO("Message: " << message);
        const auto result{detail::parse_message(message)};
        REQUIRE(result.index() == 1);
        REQUIRE(std::get<detail::ParseError>(result).Error == expectedError);
    }
}

TEST_CASE("Parsing unknown command fails") {
    auto result{detail::parse_message("unknown")};
    REQUIRE(result.index() == 1);
    REQUIRE(std::get<detail::ParseError>(result).Error == "Unknown GCode command 'unknown'");
}

TEST_CASE("Parsing G0/G1") {
    const auto runTest{[](auto command) {
        INFO("Command " << command);

        const auto message{parse_and_expect_message(fmt::format("{} X1.1 Y2.2 Z-3.3 F100.1", command))};
        REQUIRE(message.Command == GCodeCommand::Go);
        REQUIRE(*message.X == 1.1_a);
        REQUIRE(*message.Y == 2.2_a);
        REQUIRE(*message.Z == -3.3_a);
        REQUIRE(*message.F == 100.1_a);
    }};

    runTest("G0");
    runTest("G1");
    runTest("G00");
    runTest("G01");
}

TEST_CASE("Parsing incomplete G0") {
    const auto message{parse_and_expect_message("G0 X1.1 Z-3.3")};
    REQUIRE(message.Command == GCodeCommand::Go);
    REQUIRE(*message.X == 1.1_a);
    REQUIRE(!message.Y.has_value());
    REQUIRE(*message.Z == -3.3_a);
    REQUIRE(!message.F.has_value());
}

TEST_CASE("G0 failures") {
    parse_and_expect_error("G", "Could not parse '' command number");
    parse_and_expect_error("G X10", "Could not parse '' command number");
    parse_and_expect_error("Ga X10", "Could not parse 'a' command number");
    parse_and_expect_error("G0 X", "Could not parse '' into float");
    parse_and_expect_error("G0 X1 Y-1 Z", "Could not parse '' into float");
    parse_and_expect_error("G0 Xaa", "Could not parse 'aa' into float");
}

TEST_CASE("Parsing simple commands") {
    REQUIRE(parse_and_expect_message("G20").Command == GCodeCommand::UseInches);
    REQUIRE(parse_and_expect_message("G21").Command == GCodeCommand::UseMm);
    REQUIRE(parse_and_expect_message("G90").Command == GCodeCommand::AbsolutePositioning);
    REQUIRE(parse_and_expect_message("G91").Command == GCodeCommand::RelativePositioning);
}

TEST_CASE("Parsing 'G28' command") {
    const auto message{parse_and_expect_message("G28")};
    REQUIRE(message.Command == GCodeCommand::Home);

    REQUIRE(!message.X.has_value());
    REQUIRE(!message.Y.has_value());
    REQUIRE(!message.Z.has_value());
}

TEST_CASE("Parsing 'G28 X Z' command") {
    const auto message{parse_and_expect_message("G28 X Z")};
    REQUIRE(message.Command == GCodeCommand::Home);

    REQUIRE(*message.X == 1._a);
    REQUIRE(!message.Y.has_value());
    REQUIRE(*message.Z == 1._a);
}

TEST_CASE("G28 failures") {
    parse_and_expect_error("G28 Xx", "Unexpected value 'Xx' in G28 command");
    parse_and_expect_error("G28 XY", "Unexpected value 'XY' in G28 command");
    parse_and_expect_error("G28 X Zz", "Unexpected value 'Zz' in G28 command");
}