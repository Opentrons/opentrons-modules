#include <vector>

#include "catch2/catch.hpp"
#include "thermocycler-refresh/plate_control.hpp"

using namespace thermal_general;

static constexpr double UPDATE_RATE_SEC = 0.005F;
static constexpr double ROOM_TEMP = 23.0F;
static constexpr double HOT_TEMP = 90.0F;
static constexpr double COLD_TEMP = 4.0F;

SCENARIO("Plate Control peltier testing") {
    GIVEN("A PlateControl object with room temperature thermistors") {
        std::vector<Thermistor> thermistors;
        for (int i = 0; i < (PeltierID::PELTIER_NUMBER * 2) + 1; ++i) {
            thermistors.push_back(Thermistor{
                .temp_c = ROOM_TEMP,
                .overtemp_limit_c = 105.0,
                .disconnected_error =
                    errors::ErrorCode::THERMISTOR_HEATSINK_DISCONNECTED,
                .short_error = errors::ErrorCode::THERMISTOR_HEATSINK_SHORT,
                .overtemp_error =
                    errors::ErrorCode::THERMISTOR_HEATSINK_OVERTEMP,
                .error_bit = (uint8_t)(1 << i)});
        }
        Peltier left{.id = PeltierID::PELTIER_LEFT,
                     .thermistors = Peltier::ThermistorPair(
                         thermistors.at(THERM_BACK_LEFT),
                         thermistors.at(THERM_FRONT_LEFT)),
                     .pid = PID(1, 0, 0, UPDATE_RATE_SEC, 1.0, -1.0)};
        Peltier right{.id = PeltierID::PELTIER_RIGHT,
                      .thermistors = Peltier::ThermistorPair(
                          thermistors.at(THERM_BACK_RIGHT),
                          thermistors.at(THERM_FRONT_RIGHT)),
                      .pid = PID(1, 0, 0, UPDATE_RATE_SEC, 1.0, -1.0)};
        Peltier center{.id = PeltierID::PELTIER_CENTER,
                       .thermistors = Peltier::ThermistorPair(
                           thermistors.at(THERM_BACK_CENTER),
                           thermistors.at(THERM_FRONT_CENTER)),
                       .pid = PID(1, 0, 0, UPDATE_RATE_SEC, 1.0, -1.0)};
        HeatsinkFan fan{.thermistor = thermistors.at(THERM_HEATSINK),
                        .pid = PID(1, 0, 0, UPDATE_RATE_SEC, 1.0, -1.0)};
        auto plateControl = plate_control::PlateControl(left, right, center,
                                                        fan, UPDATE_RATE_SEC);
        THEN("The temperature reads correctly") {
            REQUIRE(plateControl.plate_temp() == ROOM_TEMP);
            REQUIRE(plateControl.setpoint() == 0.0F);
        }
        WHEN("Setting a hot target temperature") {
            plateControl.set_new_target(HOT_TEMP);
            REQUIRE(plateControl.setpoint() == HOT_TEMP);
            THEN("Updating control should drive peltiers hot") {
                auto ctrl = plateControl.update_control();
                REQUIRE(ctrl.has_value());
                auto controlValues = ctrl.value();
                REQUIRE(controlValues.center_power > 0.0F);
                REQUIRE(controlValues.right_power > 0.0F);
                REQUIRE(controlValues.left_power > 0.0F);
            }
        }
        WHEN("Setting a cold target temperature") {
            plateControl.set_new_target(COLD_TEMP);
            REQUIRE(plateControl.setpoint() == COLD_TEMP);
            THEN("Updating control should drive peltiers cold") {
                auto ctrl = plateControl.update_control();
                REQUIRE(ctrl.has_value());
                auto controlValues = ctrl.value();
                REQUIRE(controlValues.center_power < 0.0F);
                REQUIRE(controlValues.right_power < 0.0F);
                REQUIRE(controlValues.left_power < 0.0F);
            }
        }
    }
}
