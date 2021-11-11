#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"
#include "test/task_builder.hpp"

TEST_CASE("Build tasks") {
    auto tasks = TaskBuilder::build();
    REQUIRE(static_cast<bool>(tasks) == true);
}
