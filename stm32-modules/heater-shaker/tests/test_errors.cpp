#include <array>
#include <cstring>
#include <utility>

#include "catch2/catch.hpp"
#include "heater-shaker/errors.hpp"

using error_pair = std::pair<errors::ErrorCode, errors::MotorErrorOffset>;

SCENARIO("testing error writing") {
    GIVEN("a buffer long enough for an error") {
        auto buffer = std::string(64, 'c');
        WHEN("writing an error into it") {
            auto written =
                errors::write_into(buffer.begin(), buffer.end(),
                                   errors::ErrorCode::USB_TX_OVERRUN);
            THEN("the error is written into the buffer") {
                REQUIRE_THAT(buffer, Catch::Matchers::StartsWith(
                                         "ERR001:tx buffer overrun OK\n"));
                AND_THEN("the length was appropriately returned") {
                    REQUIRE(written ==
                            buffer.begin() + strlen("ERR001:tx "
                                                    "buffer overrun OK\n"));
                }
            }
        }
    }

    GIVEN("a buffer too small for an error") {
        auto buffer = std::string(2, 'c');
        WHEN("trying to write an error into it") {
            auto written =
                errors::write_into(buffer.begin(), buffer.end(),
                                   errors::ErrorCode::INTERNAL_QUEUE_FULL);
            THEN(
                "the error written into only the space available in the "
                "buffer") {
                REQUIRE_THAT(buffer,
                             Catch::Matchers::Equals(std::string("ER")));
                AND_THEN("the amount written is 0") {
                    REQUIRE(written == buffer.begin() + 2);
                }
            }
        }
    }

    GIVEN("motor error translation tables") {
        auto test_case = GENERATE(
            error_pair{errors::ErrorCode::MOTOR_FOC_DURATION,
                       errors::MotorErrorOffset::FOC_DURATION},
            error_pair{errors::ErrorCode::MOTOR_BLDC_OVERVOLT,
                       errors::MotorErrorOffset::OVER_VOLT},
            error_pair{errors::ErrorCode::MOTOR_BLDC_UNDERVOLT,
                       errors::MotorErrorOffset::UNDER_VOLT},
            error_pair{errors::ErrorCode::MOTOR_BLDC_OVERTEMP,
                       errors::MotorErrorOffset::OVER_TEMP},
            error_pair{errors::ErrorCode::MOTOR_BLDC_STARTUP_FAILED,
                       errors::MotorErrorOffset::START_UP},
            error_pair{errors::ErrorCode::MOTOR_BLDC_SPEEDSENSOR_FAILED,
                       errors::MotorErrorOffset::SPEED_FDBK},
            error_pair{errors::ErrorCode::MOTOR_BLDC_DRIVER_FAULT,
                       errors::MotorErrorOffset::OVERCURRENT},
            error_pair{errors::ErrorCode::MOTOR_BLDC_DRIVER_ERROR,
                       errors::MotorErrorOffset::SW_ERROR});
        WHEN((std::string("converting error code ") +
              std::to_string(
                  static_cast<std::underlying_type<errors::ErrorCode>::type>(
                      test_case.first)) +
              " back and forth")) {
            THEN("from_motor_error should work") {
                REQUIRE(errors::from_motor_error(1 << test_case.second,
                                                 test_case.second) ==
                        test_case.first);
            }
            THEN("to_motor_error should work") {
                REQUIRE(errors::to_motor_error(test_case.first) ==
                        (1 << test_case.second));
            }
        }
    }
}
