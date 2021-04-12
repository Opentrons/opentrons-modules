#include "catch2/catch.hpp"
#include "hal/double_buffer.hpp"

SCENARIO("double buffer swapping") {
    GIVEN("a double buffer") {
        auto db = double_buffer::DoubleBuffer<uint8_t, 256>();
        REQUIRE(db.committed() != db.accessible());
        WHEN("calling swap") {
            THEN("the accessors should be swapped") {
                auto old_committed = db.committed();
                auto old_accessible = db.accessible();
                db.swap();
                REQUIRE(old_committed == db.accessible());
                REQUIRE(old_accessible == db.committed());
            }
        }
    }
}
