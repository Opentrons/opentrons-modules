#include "catch2/catch.hpp"
#include "test/test_tasks.hpp"
#include "test/test_thermistor_policy.hpp"

TEST_CASE("thermistor task functionality") {
    auto *tasks = tasks::BuildTasks();
    TestThermistorPolicy policy;
    policy.sleep_ms(123);
    policy._imeas_adc_val = 123;
    WHEN("thermistor task runs once") {
        tasks->_thermistor_task.run_once(policy);
        THEN("a Thermistor Message is sent to the thermal task") {
            REQUIRE(tasks->_thermal_queue.has_message());
            auto msg = tasks->_thermal_queue.backing_deque.front();
            REQUIRE(std::holds_alternative<messages::ThermistorReadings>(msg));
            auto therms = std::get<messages::ThermistorReadings>(msg);
            REQUIRE(therms.timestamp == policy.get_time_ms());
            REQUIRE(therms.heatsink == decltype(policy)::READBACK_VALUE);
            REQUIRE(therms.plate == decltype(policy)::READBACK_VALUE);
            REQUIRE(therms.imeas == policy._imeas_adc_val);
        }
        THEN("the adc was initialized") { REQUIRE(policy._initialized); }
        THEN("two channels of the adc were read") {
            // Once per each channel + initialization
            REQUIRE(policy._lock_count == 3);
        }
    }
}
