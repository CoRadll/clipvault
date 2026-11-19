#include <catch_amalgamated.hpp>

#include "clipvault/util.hpp"

TEST_CASE("util_add 做加法")
{
	REQUIRE(util_add(1, 2) == 3);
	REQUIRE(util_add(-1, 1) == 0);
}