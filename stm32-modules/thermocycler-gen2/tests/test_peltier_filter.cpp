
#include "catch2/catch.hpp"
#include "thermocycler-gen2/peltier_filter.hpp"

TEST_CASE("peltier filter functionality") {
    using namespace peltier_filter;
    auto subject = PeltierFilter();
    REQUIRE(subject.get_last() == 0.0F);
    WHEN("setting power outside of the filter limits") {
        const auto TIME_DELTA = GENERATE(0.01, 0.05);
        const auto SETTING = GENERATE(1.0, -1.0);

        auto result = subject.set_filtered(SETTING, TIME_DELTA);
        THEN("the result is filtered") {
            auto expected = MAX_DELTA * TIME_DELTA * (SETTING > 0 ? 1 : -1);
            REQUIRE_THAT(result, Catch::Matchers::WithinAbs(expected, 0.01));
            AND_WHEN("doing it again") {
                result = subject.set_filtered(SETTING, TIME_DELTA);
                THEN("the result is doubled") {
                    REQUIRE_THAT(
                        result, Catch::Matchers::WithinAbs(expected * 2, 0.01));
                }
            }
        }
        THEN("getting the last result matches expected") {
            REQUIRE(subject.get_last() == result);
        }
    }
    WHEN("setting power within filter limits") {
        const auto TIME_DELTA = GENERATE(0.1, 0.5, 1.0);
        const auto SETTING = GENERATE(1.0, -1.0, -0.245, 0.64);
        auto result = subject.set_filtered(SETTING, TIME_DELTA);
        THEN("the result is not filtered at all") {
            REQUIRE_THAT(result, Catch::Matchers::WithinAbs(SETTING, 0.01));
        }
    }
}