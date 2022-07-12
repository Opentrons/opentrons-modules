#include "catch2/catch.hpp"
#include "test/test_tasks.hpp"

struct FakePolicy {
    auto operator()() -> void {}
};

TEST_CASE("thermal task message handling") {
    auto *tasks = tasks::BuildTasks();
    FakePolicy policy;
    thermistor_conversion::Conversion<lookups::NXFT15XV103FA2B030> converter(
        decltype(tasks->_thermal_task)::THERMISTOR_CIRCUIT_BIAS_RESISTANCE_KOHM,
        decltype(tasks->_thermal_task)::ADC_BIT_MAX, false);
    WHEN("new thermistor readings are received") {
        auto plate_count = converter.backconvert(25.00);
        auto hs_count = converter.backconvert(50.00);
        auto thermistors_msg = messages::ThermistorReadings{
            .timestamp = 1000,
            .plate = plate_count,
            .heatsink = hs_count,
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
        THEN("the ADC readings are properly converted to temperatures") {
            auto readings = tasks->_thermal_task.get_readings();
            REQUIRE_THAT(readings.plate_temp,
                         Catch::Matchers::WithinAbs(25.00, 0.01));
            REQUIRE_THAT(readings.heatsink_temp,
                         Catch::Matchers::WithinAbs(50.00, 0.01));
        }
        AND_WHEN("a GetTempDebug message is received") {
            tasks->_thermal_queue.backing_deque.push_back(
                messages::GetTempDebugMessage{.id = 123});
            tasks->_thermal_task.run_once(policy);
            THEN("the message is consumed") {
                REQUIRE(!tasks->_thermal_queue.has_message());
            }
            THEN("a response is sent to the comms task with the correct data") {
                REQUIRE(tasks->_comms_queue.has_message());
                REQUIRE(std::holds_alternative<messages::GetTempDebugResponse>(
                    tasks->_comms_queue.backing_deque.front()));
                auto response = std::get<messages::GetTempDebugResponse>(
                    tasks->_comms_queue.backing_deque.front());
                REQUIRE(response.responding_to_id == 123);
                REQUIRE_THAT(response.plate_temp,
                             Catch::Matchers::WithinAbs(25.00, 0.01));
                REQUIRE_THAT(response.heatsink_temp,
                             Catch::Matchers::WithinAbs(50.00, 0.01));
                REQUIRE(response.plate_adc == plate_count);
                REQUIRE(response.heatsink_adc == hs_count);
            }
        }
    }
}
