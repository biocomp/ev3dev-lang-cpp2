#include <catch2.hpp>
#include <driver.h>
#include <scheduler.h>
#include <sstream>
#include <string>
#include <unordered_map>
#include <algorithm>

using namespace ev3plotter;

namespace {
struct MockWidget : IWidget {
    struct MockState : public widget_state {
        bool changed() noexcept {
            REQUIRE(false);
            return false; /* Should not be called */
        }
        bool handle_event(event) noexcept {
            REQUIRE(false);
            return false; /* Should not be called */
        }
        void draw(display&) const noexcept { REQUIRE(false); /* Should not be called */ }
    };

    std::unique_ptr<widget_state> make() const noexcept { return std::make_unique<MockState>(); }
};

struct MockSystem : ev3dev::ISystem {
private:
    struct FileData {
        FileData(std::string initial_contents) : read{initial_contents} {}
        std::istringstream read;
        std::ostringstream write;
    };

public:
    class MockMotor {
    public:
        MockMotor(std::unordered_map<std::string, FileData&> attributes) : attributes_{std::move(attributes)} {}

        auto get(const std::string& attribute) const noexcept -> decltype(auto) {
            return attributes_.at(attribute).write.str();
        }

        void set(const std::string& attribute, const std::string& value) noexcept {
            attributes_.at(attribute).read = std::istringstream{value};
        }
    private:
        std::unordered_map<std::string, FileData&> attributes_;
    };

    struct MockOstream : ev3dev::file_ostream {
        MockOstream(std::ostream& stream)
            : is_open_{true},
            stream_{stream} {}
        MockOstream()
            : is_open_{false} {}

        bool is_open() const override { return true; }
        void close() override {}
        void clear() override {}
        void prepare(const std::string&) override {}

        std::ostream& get() override { return stream_; }
        const std::ostream& get() const override { return stream_; }

      private:
        std::ostringstream defaultStream_;
        bool is_open_;
        std::ostream& stream_{defaultStream_};
    };

    struct MockIstream : ev3dev::file_istream {
        MockIstream(std::istream& stream)
            : is_open_{true}
            , stream_{stream} {}
        MockIstream()
            : is_open_{false} {}

        bool is_open() const override { return true; }
        void close() override {}
        void clear() override {}

        void prepare(const std::string&) override {
            stream_.clear(); // clear the `failbit` and `eofbit`
            stream_.seekg(0);
        }

        std::istream& get() override { return stream_; }
        const std::istream& get() const override { return stream_; }

      private:
        std::istringstream defaultStream_;
        bool is_open_;
        std::istream& stream_{defaultStream_};
    };

    std::unique_ptr<ev3dev::file_ostream> OpenForWrite(const std::string& path) const override {
        auto found = files_.find(path);
        if (found != files_.end()) {
            return std::make_unique<MockOstream>(found->second.write);
        } else {
            return std::make_unique<MockOstream>();
        }
    }

    std::unique_ptr<ev3dev::file_istream> OpenForRead(const std::string& path) const override {
        auto found = files_.find(path);
        if (found != files_.end()) {
            return std::make_unique<MockIstream>(found->second.read);
        } else {
            return std::make_unique<MockIstream>();
        }
    }

    void System(const char*) const override {
        INFO("Should not be called!");
        REQUIRE(false);
    }

    const std::string& get_sys_root() const override { return sys_root_; }

    void ListFiles(ev3dev::zstring_ref dir, const std::function<bool(ev3dev::zstring_ref)>& fileFound) const override {
        auto foundDir = dirs_.find(std::string{dir.c_str(), dir.size()});
        if (foundDir != dirs_.end()) {
            for (auto&& f : foundDir->second) {
                if (! fileFound(f)) {
                    break;
                }
            }
        }
    }

    void add_motor(
        int index,
        std::string address,
        std::string driver_name,
        std::initializer_list<std::pair<std::string, std::string>> otherAttributesAndValues) {
        dirs_[sys_root_ + "/tacho-motor/"].push_back("motor" + std::to_string(index));

        std::unordered_map<std::string, FileData&> attributes;
        const auto file_root = sys_root_ + "/tacho-motor/motor" + std::to_string(index) + "/";

        const auto add_attribute{[&](auto&& name, auto&& value) {
            const auto& file = files_.emplace(file_root + name, value).first;
            attributes.emplace(name, file->second);
        }};

        add_attribute("address", address);
        add_attribute("driver_name", driver_name);

        for (auto&& a : otherAttributesAndValues) {
            add_attribute(a.first, a.second);
        }

        motors_[index].emplace(attributes);
    }

    MockMotor& get_motor(int index) { return *motors_[index]; }

  private:
    mutable std::unordered_map<std::string, FileData> files_;
    std::unordered_map<std::string, std::vector<std::string>> dirs_;
    std::string sys_root_{"/some/sys/root"};
    std::optional<MockMotor> motors_[3];
};
} // namespace

TEST_CASE("calculate_speeds() for relative speeds") {
    constexpr const int c_defaultSpeed{200};
    Scheduler sch;
    MockSystem sys;
    state s{sch, sys};
    s.gcode_state_.relative_moves = true;
    s.gcode_state_.use_mm = true;

    struct TestData {
        std::string description;
        std::optional<double> x, y;
        int expected_x_speed, expected_y_speed;
    };

    for (auto&& t : {
             TestData{"No x or y coordinates - speeds are 0es", {}, {}, 0, 0},
             TestData{"Only x", 123, {}, c_defaultSpeed, 0},
             TestData{"Only -x", -123, {}, c_defaultSpeed, 0},
             TestData{"Only y", {}, 123, 0, c_defaultSpeed},
             TestData{"Only -y", {}, -123, 0, c_defaultSpeed},
             TestData{"x == y", 42.0*s.gcode_state_.c_stepsToMm[0], 42.0*s.gcode_state_.c_stepsToMm[1], 141, 141}, // 141 is 141.421356237 cast to int.
             TestData{"x == -y", 42.0*s.gcode_state_.c_stepsToMm[0], -42.0*s.gcode_state_.c_stepsToMm[1], 141, 141},
             TestData{"-x == -y", -42.0*s.gcode_state_.c_stepsToMm[0], -42.0*s.gcode_state_.c_stepsToMm[1], 141, 141},
             TestData{"x == y/6", 42.0*s.gcode_state_.c_stepsToMm[0], -7.0*s.gcode_state_.c_stepsToMm[1], 33, 5},
         }) {
        SECTION(t.description) {
            const auto speeds{pos::calc_speeds(s, t.x, t.y)};
            REQUIRE(speeds.x == t.expected_x_speed);
            REQUIRE(speeds.y == t.expected_y_speed);
        }
    }
}
TEST_CASE("calculate_speeds() for absolute speeds") {
    constexpr const int c_defaultSpeed{200};
    Scheduler sch;
    MockSystem sys;
    state s{sch, sys};
    s.gcode_state_.relative_moves = true;
    s.gcode_state_.use_mm = true;

    struct TestData {
        std::string description;
        std::optional<double> x, y;
        int expected_x_speed, expected_y_speed;
    };

    for (auto&& t : {
             TestData{"No x or y coordinates - speeds are 0es", {}, {}, 0, 0},
             TestData{"Only x", 123, {}, c_defaultSpeed, 0},
             TestData{"Only -x", -123, {}, c_defaultSpeed, 0},
             TestData{"Only y", {}, 123, 0, c_defaultSpeed},
             TestData{"Only -y", {}, -123, 0, c_defaultSpeed},
             TestData{"x == y", 42.0*s.gcode_state_.c_stepsToMm[0], 42.0*s.gcode_state_.c_stepsToMm[1], 141, 141}, // 141 is 141.421356237 cast to int.
             TestData{"x == -y", 42.0*s.gcode_state_.c_stepsToMm[0], -42.0*s.gcode_state_.c_stepsToMm[1], 141, 141},
             TestData{"-x == -y", -42.0*s.gcode_state_.c_stepsToMm[0], -42.0*s.gcode_state_.c_stepsToMm[1], 141, 141},
             TestData{"x == y/6", 42.0*s.gcode_state_.c_stepsToMm[0], -7.0*s.gcode_state_.c_stepsToMm[1], 33, 5},
         }) {
        SECTION(t.description) {
            const auto speeds{pos::calc_speeds(s, t.x, t.y)};
            REQUIRE(speeds.x == t.expected_x_speed);
            REQUIRE(speeds.y == t.expected_y_speed);
        }
    }
}

TEST_CASE("home() will run all motors till they stall and report stalled values") {
    //int step = 0;
    constexpr auto tool_motor{0};
    constexpr auto x_motor{1};
    constexpr auto y_motor{2};

    MockSystem sys;
    sys.add_motor(0, "ev3-ports:outA", ev3dev::motor::motor_medium, {{"command", ""}, {"position", "0"}, {"state", "stopped"}});
    sys.add_motor(1, "ev3-ports:outB", ev3dev::motor::motor_large, {{"command", ""}, {"position", "0"}, {"state", "stopped"}});
    sys.add_motor(2, "ev3-ports:outC", ev3dev::motor::motor_large, {{"command", ""}, {"position", "0"}, {"state", "stopped"}});

    state* statePtr{nullptr};

    int step{0};
    //std::array doneHoming{false, false, false};
    //void* internalWidget{nullptr};
    //bool dialogClosed{false};

    auto mock{[&] {
        const auto stopMotors{[&](int exceptThisOne){
            for (auto i = 0; i != 3; ++i) {
                if (i != exceptThisOne) {
                    sys.get_motor(i).set("state", "stopped");
                }
            }
        }};

        switch (step) {
        case 0:
            REQUIRE(sys.get_motor(tool_motor).get("command") == "resetrun-direct");
            sys.get_motor(tool_motor).set("position", "-30");
            sys.get_motor(tool_motor).set("state", "running stalled");
            break;

        case 1:
            sys.get_motor(tool_motor).set("state", "stopped");

            REQUIRE(sys.get_motor(x_motor).get("command") == "resetrun-direct");
            sys.get_motor(x_motor).set("position", "60");
            sys.get_motor(x_motor).set("state", "running stalled");
            break;

        case 2:
            sys.get_motor(x_motor).set("state", "stopped");

            REQUIRE(sys.get_motor(x_motor).get("command") == "resetrun-directstoprun-direct");
            sys.get_motor(x_motor).set("position", "-60");
            sys.get_motor(x_motor).set("state", "running stalled");
            break;

        case 3:
            sys.get_motor(x_motor).set("state", "stopped");

            REQUIRE(sys.get_motor(y_motor).get("command") == "resetrun-direct");
            sys.get_motor(y_motor).set("position", "-1000");
            sys.get_motor(y_motor).set("state", "running stalled");
            break;

        case 4:
            sys.get_motor(y_motor).set("state", "stopped");

            REQUIRE(sys.get_motor(y_motor).get("command") == "resetrun-directstoprun-direct");
            sys.get_motor(y_motor).set("position", "1000");
            sys.get_motor(y_motor).set("state", "running stalled");
            break;

        case 5:
            // Supposed to be going for better tool down homing position. But doesn't yet.
            break;

        case 6:
            sys.get_motor(y_motor).set("state", "stopped");

            REQUIRE(sys.get_motor(tool_motor).get("command") == "resetrun-directstoprun-direct");
            sys.get_motor(tool_motor).set("position", "40");
            sys.get_motor(tool_motor).set("state", "running stalled");
            break;

        case 7: // Next step will show the dialog, and this one does nothing.
            break;

        case 8:
            statePtr->widget_->handle_event(ev3plotter::event::ok);
            break;

        default:
            break;
        }

        ++step;
    }};

    Scheduler scheduler{mock};

    state s{scheduler, sys};
    statePtr = &s;
    MockWidget widget;
    bool done{false};

    homing_results results;
    commands::home(s, scheduler, widget, [&](auto&& r) {
        done = true;
        results = std::get<0>(r);
    });

    scheduler.run();

    REQUIRE(results.x_min.get() == 30);
    REQUIRE(results.x_max.get() == -30);
    REQUIRE(results.y_min.get() == -650);
    REQUIRE(results.y_max.get() == 970);
    REQUIRE(results.tool_up_pos.get() == -10);
    REQUIRE(results.tool_down_pos.get() == 25);
}
