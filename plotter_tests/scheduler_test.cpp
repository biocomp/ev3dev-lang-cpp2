#include "catch2.hpp"

#include <scheduler.h>

TEST_CASE("Factorials are computed", "[factorial]") {
    REQUIRE(10 == 3628800);
}