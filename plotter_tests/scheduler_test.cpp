#include "catch2.hpp"

#include <scheduler.h>

using namespace ev3plotter;

namespace {
struct Results : public std::string {
    auto Add(std::string val) {
        return [this, val] { append(val); };
    }
};
} // namespace

TEST_CASE("Scheduling 3 operations") {
    Scheduler s;

    Results results;
    s.schedule([&] {
        results += "a";
        s.schedule(results.Add("c"));
    });
    s.schedule(results.Add("b"));

    REQUIRE(results == "");
    s.run();
    REQUIRE(results == "abc");
    results.clear();
    s.run();
    REQUIRE(results == "");
}

TEST_CASE("Scheduling no operations") {
    Scheduler s;
    s.run();
}

// Dependency on timing makes this test very flaky.
// Good enough for this small project though.
// Ideally, need to add mock clock and waiting to Scheduler.
TEST_CASE("Scheduling with time") {
    Scheduler s;

    Results results;
    s.schedule(std::chrono::milliseconds{500}, results.Add("a"));
    s.schedule(results.Add("b"));

    s.run();
    REQUIRE(results == "ba");
}

TEST_CASE("Scheduling with priority - smaller priority runs first") {
    Scheduler s;

    Results results;
    s.schedule(priority{3}, results.Add("d"));
    s.schedule(results.Add("a"));
    s.schedule(priority{2}, results.Add("c"));
    s.schedule(priority{0}, [&] {
        results += "a";
        s.schedule(priority{0}, results.Add("b"));
    });

    s.run();
    REQUIRE(results == "aabcd");
}

// Dependency on timing makes this test very flaky.
// Good enough for this small project though.
// Ideally, need to add mock clock and waiting to Scheduler.
TEST_CASE("Scheduling with time and priority, and from within callbacks") {
    Scheduler s;

    Results results;
    s.schedule(priority{3}, results.Add("d"));
    s.schedule(priority{2}, [&] {
        results += "c";
        s.schedule(priority{0}, std::chrono::milliseconds{500}, results.Add("e"));
    });
    s.schedule(priority{0}, [&] {
        results += "a";
        s.schedule(priority{0}, results.Add("b"));
        s.schedule(priority{0}, std::chrono::milliseconds{1000}, results.Add("f"));
    });
    s.schedule(priority{0}, [&] {
        s.schedule(priority{0}, std::chrono::milliseconds{1500}, results.Add("g"));
    });

    s.run();
    REQUIRE(results == "abcdefg");
}

TEST_CASE("Self-scheduling can make an infinite loop") {
    Results results;

    Scheduler scheduler;
    std::function<void()> loop;
    int count{0};
    loop = [&] {
        if (count != 10) {
            results += std::to_string(count);
            scheduler.schedule(priority{10}, loop);
            ++count;
        }
    };

    scheduler.schedule(loop);
    scheduler.run();

    REQUIRE(results == "0123456789");
}