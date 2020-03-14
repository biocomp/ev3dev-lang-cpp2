#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include <vector>
#include <sstream>
#include <cstdlib>
#include <ev3dev.h>

namespace ev3 = ev3dev;

void populate_arena(const std::vector<const char*> &devices) {
    std::ostringstream command;
    command << FAKE_SYS "/populate_arena.py";
    for (auto d : devices) command << " " << d;

    system(FAKE_SYS "/clean_arena.py");
    system(command.str().c_str());
}

struct MockSystem : ev3::ISystem {
    struct MockOstream : ev3::file_ostream {
        MockOstream(const std::string&) {}

        bool is_open() const override { return true; }
        void close() override { }
        void clear() override { }

        std::ostream& get() override { return _stream; }
        const std::ostream& get() const override { return _stream; }

        std::ostringstream _stream;
    };

    struct MockIstream : ev3::file_ostream {
        MockIstream(const std::string&) {}

        bool is_open() const override { return true; }
        void close() override {  }
        void clear() override {  }

        std::istream& get() override { return _stream; }
        const std::istream& get() const override { return _stream; }

        std::istringstream _stream;
    };

    std::unique_ptr<ev3::file_ostream> OpenForWrite(const std::string &path) const override {
        return std::make_unique<MockOstream>(path);
    }

    std::unique_ptr<ev3::file_istream> OpenForRead(const std::string &path) const override {
        return std::make_unique<MockIstream>(path);
    }

    void System(const char *command) const override
    {
        system_calls.emplace_back(command);
    }

    mutable std::vector<std::string> system_calls;
};

TEST_CASE( "Device" ) {
    populate_arena({"medium_motor:0@ev3-ports:outA", "infrared_sensor:0@ev3-ports:in1"});

    MockSystem sys;
    ev3::device d{sys};

    SECTION("connect any motor") {
        d.connect(SYS_ROOT "/tacho-motor/", "motor", {});
        REQUIRE(d.connected());
    }

    SECTION("connect specific motor") {
        d.connect(SYS_ROOT "/tacho-motor/", "motor0", {});
        REQUIRE(d.connected());
    }

    SECTION("connect a motor by driver name") {
        d.connect(SYS_ROOT "/tacho-motor/", "motor",
                {{std::string("driver_name"), {std::string("lego-ev3-m-motor")}}});
        REQUIRE(d.connected());
    }

    SECTION("connect a motor by address") {
        d.connect(SYS_ROOT "/tacho-motor/", "motor",
                {{std::string("address"), {ev3::OUTPUT_A}}});
        REQUIRE(d.connected());
    }

    SECTION("invalid driver name") {
        d.connect(SYS_ROOT "/tacho-motor/", "motor",
                {{std::string("driver_name"), {std::string("not-valid")}}});
        REQUIRE(!d.connected());
    }

    SECTION("connect a sensor") {
        d.connect(SYS_ROOT "/lego-sensor/", "sensor", {});
        REQUIRE(d.connected());
    }
}

TEST_CASE("Medium Motor") {
    populate_arena({"medium_motor:0@ev3-ports:outA"});

    MockSystem sys;
    ev3::medium_motor m{ev3::OUTPUT_AUTO, sys};

    REQUIRE(m.connected());
    REQUIRE(m.device_index() == 0);

    SECTION("reading same attribute twice") {
        REQUIRE(m.driver_name() == "lego-ev3-m-motor");
        REQUIRE(m.driver_name() == "lego-ev3-m-motor");
    }

    SECTION("check attribute values") {
        std::set<std::string> commands = {
            "run-forever", "run-to-abs-pos", "run-to-rel-pos", "run-timed",
            "run-direct", "stop", "reset"
            };

        std::set<std::string> state = {"running"};

        REQUIRE(m.count_per_rot() == 360);
        REQUIRE(m.commands()      == commands);
        REQUIRE(m.duty_cycle()    == 0);
        REQUIRE(m.duty_cycle_sp() == 42);
        REQUIRE(m.polarity()      == "normal");
        REQUIRE(m.address()       == "ev3-ports:outA");
        REQUIRE(m.position()      == 42);
        REQUIRE(m.position_sp()   == 42);
        REQUIRE(m.ramp_down_sp()  == 0);
        REQUIRE(m.ramp_up_sp()    == 0);
        REQUIRE(m.speed()         == 0);
        REQUIRE(m.speed_sp()      == 0);
        REQUIRE(m.state()         == state);
        REQUIRE(m.stop_action()   == "coast");
        REQUIRE(m.time_sp()       == 1000);
    }
}

TEST_CASE("Infrared Sensor") {
    populate_arena({"infrared_sensor:0@ev3-ports:in1"});

    MockSystem sys;
    ev3::infrared_sensor s{ev3::INPUT_AUTO, sys};

    REQUIRE(s.connected());

    REQUIRE(s.device_index()    == 0);
    REQUIRE(s.bin_data_format() == "s8");
    REQUIRE(s.num_values()      == 1);
    REQUIRE(s.address()         == "ev3-ports:in1");
    REQUIRE(s.value(0)          == 16);

    std::vector<char> v(1);
    s.bin_data(v.data());

    REQUIRE(v[0] == 16);
    REQUIRE(s.bin_data() == v);
}
