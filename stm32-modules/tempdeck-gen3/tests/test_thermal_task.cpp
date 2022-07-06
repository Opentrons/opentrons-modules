#include "catch2/catch.hpp"
#include "test/test_tasks.hpp"

struct FakePolicy {
    auto operator()() -> void {}
};

TEST_CASE("thermal task message handling") {
    auto *tasks = tasks::BuildTasks();
    FakePolicy policy;
    WHEN("new thermistor readings are received") {
        auto thermistors_msg = messages::ThermistorReadings{
            .timestamp = 1000,
            .plate = 456,
            .heatsink = 123,
        };
        tasks->_thermal_queue.backing_deque.push_back(thermistors_msg);
        tasks->_thermal_task.run_once(policy);
        THEN("the message is consumed") {
            REQUIRE(!tasks->_thermal_queue.has_message());
        }
        THEN("the readings are updated properly") {
            auto readings = tasks->_thermal_task.get_readings();
            REQUIRE(readings.heatsink_adc == thermistors_msg.heatsink);
            REQUIRE(readings.plate_adc == thermistors_msg.plate);
            REQUIRE(readings.last_tick == thermistors_msg.timestamp);
        }
    }
}
