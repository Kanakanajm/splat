#include <catch2/catch_test_macros.hpp>

TEST_CASE("catch2 wiring is functional", "[smoke]") {
    REQUIRE(1 + 1 == 2);
}
