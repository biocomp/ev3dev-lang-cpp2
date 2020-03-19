#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include <vector>
#include <sstream>
#include <cstdlib>
#include <ev3dev.h>
#include <unordered_map>
#include <string_view>

namespace ev3 = ev3dev;

namespace
{
    struct MockSystem : ev3::ISystem {
        struct MockOstream : ev3::file_ostream {
            MockOstream(const std::string&, const std::string&) : _is_open{true} {}
            MockOstream(const std::string&) : _is_open{false}  {}

            bool is_open() const override { return true; }
            void close() override { }
            void clear() override { }

            std::ostream& get() override { return _stream; }
            const std::ostream& get() const override { return _stream; }

        private:
            bool _is_open;
            std::ostringstream _stream;
        };

        struct MockIstream : ev3::file_istream {
            MockIstream(const std::string&, const std::string& contents) : _is_open{true}, _stream{contents} {}
            MockIstream(const std::string&) : _is_open{false} {}

            bool is_open() const override { return true; }
            void close() override {  }
            void clear() override {  }

            std::istream& get() override { return _stream; }
            const std::istream& get() const override { return _stream; }

        private:
            bool _is_open;
            std::istringstream _stream;
        };

        std::unique_ptr<ev3::file_ostream> OpenForWrite(const std::string &path) const override {
            return make_stream<MockOstream>(path);
        }

        std::unique_ptr<ev3::file_istream> OpenForRead(const std::string &path) const override {
            return make_stream<MockIstream>(path);
        }

        void System(const char *command) const override {
            system_calls.emplace_back(command);
        }

        const std::string& get_sys_root() const override {
            return sys_root;
        }

        void ListFiles(ev3::zstring_ref dir, const std::function<bool(ev3::zstring_ref)>& fileFound) const override {
            auto foundDir = dirs.find(std::string{dir.c_str(), dir.size()});
            if (foundDir != dirs.end()){
                for (auto&& f : foundDir->second) {
                    if (!fileFound(f)) {
                        break;
                    }
                }
            }
        }

        void populate_arena(std::initializer_list<ev3::zstring_ref> devices) {
            for (auto&& device : devices) {
                const auto semicolonPos = device.find(':');
                const auto dev_type = device.substr(0, semicolonPos);
                const auto after_dev_type = device.substr(semicolonPos + 1, device.size());
                const auto index_pos = after_dev_type.find('@');

                const auto index = after_dev_type.substr(0, index_pos);
                const auto address = after_dev_type.substr(index_pos + 1, after_dev_type.size());

                add_device(dev_type, index, address);
            }
        }

    private:
        template <typename TStream>
        std::unique_ptr<TStream> make_stream(const std::string& path) const {
            auto found = files.find(path);
            if (found != files.end()) {
                return std::make_unique<TStream>(path, found->second);
            } else {
                return std::make_unique<TStream>(path);
            }
        }

        void add_device(const ev3::string_ref& dev_type, const ev3::string_ref& index, const ev3::string_ref& address) {
            const auto path{_mock_device_path.find(dev_type)};
            assert (path != _mock_device_path.end());

            const auto fullPath{std::string{path->second.first} + '/' + std::string{path->second.second}};
            const auto data{_mock_device_data.find(fullPath)};
            assert(data != _mock_device_data.end());

            const auto filePathPrefix = sys_root + '/' + fullPath + std::string{index} + '/';
            for (auto&& f : data->second) {
                files[filePathPrefix + f.first] = f.second;
            }
            files[filePathPrefix + "address"] = address;

            dirs[sys_root + '/' + std::string{path->second.first} + '/'].push_back(std::string{path->second.second} + std::string{index});
        }

        struct device_data {
            std::string path;
            std::unordered_map<std::string, std::string> attributes;
        };

        static const std::unordered_map<ev3::string_ref, std::pair<ev3::zstring_ref, ev3::zstring_ref>> _mock_device_path;
        static const std::unordered_map<ev3::string_ref, std::unordered_map<std::string, std::string>> _mock_device_data;

    public:
        std::unordered_map<std::string, std::string> files;
        std::unordered_map<std::string, std::vector<std::string>> dirs;
        std::string sys_root{"/some/sys/root"};
        mutable std::vector<std::string> system_calls;
    };

    const std::unordered_map<ev3::string_ref, std::pair<ev3::zstring_ref, ev3::zstring_ref>> MockSystem::_mock_device_path{
        {"infrared_sensor", {"lego-sensor", "sensor"}},
        {"touch_sensor", {"lego-sensor", "sensor"}},
        {"medium_motor", {"tacho-motor", "motor"}},
        {"large_motor", {"tacho-motor", "motor"}}
    };

    const std::unordered_map<ev3::string_ref, std::unordered_map<std::string, std::string>> MockSystem::_mock_device_data{
        {"lego-sensor/sensor", {
            {"driver_name", "lego-ev3-ir"},
            {"device_index","0"},
            {"bin_data_format","s8"},
            {"bin_data","\x10\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"},
            {"num_values","1"},
            {"value0","16"}
        }},
        {"tacho-motor/motor", {
            {"driver_name", "lego-ev3-m-motor"},
            {"count_per_rot", "360"},
            {"commands", "run-forever run-to-abs-pos run-to-rel-pos run-timed run-direct stop reset"},
            {"duty_cycle", "0"},
            {"duty_cycle_sp", "42"},
            {"polarity", "normal"},
            {"position", "42"},
            {"position_sp", "42"},
            {"ramp_down_sp", "0"},
            {"ramp_up_sp", "0"},
            {"speed", "0"},
            {"speed_sp", "0"},
            {"state", "running"},
            {"stop_action", "coast"},
            {"time_sp", "1000"}
        }}
    };

}

TEST_CASE( "Device" ) {
    MockSystem sys;
    sys.populate_arena({"medium_motor:0@ev3-ports:outA", "infrared_sensor:0@ev3-ports:in1   "});

    ev3::device d{sys};

    SECTION("connect any motor") {
        d.connect(sys.get_sys_root() +  "/tacho-motor/", "motor", {});
        REQUIRE(d.connected());
    }

    SECTION("connect specific motor") {
        d.connect(sys.get_sys_root() +  "/tacho-motor/", "motor0", {});
        REQUIRE(d.connected());
    }

    SECTION("connect a motor by driver name") {
        d.connect(sys.get_sys_root() +  "/tacho-motor/", "motor",
                {{std::string("driver_name"), {std::string("lego-ev3-m-motor")}}});
        REQUIRE(d.connected());
    }

    SECTION("connect a motor by address") {
        d.connect(sys.get_sys_root() +  "/tacho-motor/", "motor",
                {{std::string("address"), {ev3::OUTPUT_A}}});
        REQUIRE(d.connected());
    }

    SECTION("invalid driver name") {
        d.connect(sys.get_sys_root() +  "/tacho-motor/", "motor",
                {{std::string("driver_name"), {std::string("not-valid")}}});
        REQUIRE(!d.connected());
    }

    SECTION("connect a sensor") {
        d.connect(sys.get_sys_root() +  "/lego-sensor/", "sensor", {});
        REQUIRE(d.connected());
    }
}

TEST_CASE("Medium Motor") {
    MockSystem sys;
    sys.populate_arena({"medium_motor:0@ev3-ports:outA"});
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
    MockSystem sys;
    sys.populate_arena({"infrared_sensor:0@ev3-ports:in1"});
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
