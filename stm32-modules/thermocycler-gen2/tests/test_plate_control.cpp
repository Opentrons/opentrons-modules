#include <vector>

#include "catch2/catch.hpp"
#include "thermocycler-gen2/plate_control.hpp"

using namespace thermal_general;

static constexpr double UPDATE_RATE_SEC = 0.005F;
static constexpr double ROOM_TEMP = 23.0F;
static constexpr double HOT_TEMP = 90.0F;
static constexpr double COLD_TEMP = 4.0F;
static constexpr double WARM_TEMP = 28.0F;
static constexpr double THRESHOLD = 0.5F;

// Useful test functions

static void set_temp(std::vector<Thermistor> &thermistors,
                     const double plate_temp, const double heatsink_temp) {
    for (auto &therm : thermistors) {
        therm.temp_c = plate_temp;
    }
    thermistors.at(THERM_HEATSINK).temp_c = heatsink_temp;
}

static void set_temp(std::vector<Thermistor> &thermistors,
                     const double plate_temp) {
    set_temp(thermistors, plate_temp, plate_temp);
}

TEST_CASE("PlateControl overshoot and undershoot calculation") {
    using namespace plate_control;
    const double input_volume = GENERATE(0.0F, 10.0F, 25.0F, 100.0F);
    const double input_temp = GENERATE(0.0F, 10.0F, 25.0F, 90.0F);
    WHEN("calculating overshoot") {
        const double output_temp_diff =
            (input_volume * PlateControl::OVERSHOOT_M_CONST) +
            PlateControl::OVERSHOOT_B_CONST;
        auto output =
            PlateControl::calculate_overshoot(input_temp, input_volume);
        THEN("overshoot is correct") {
            REQUIRE_THAT(output, Catch::Matchers::WithinAbs(
                                     input_temp + output_temp_diff, 0.001));
        }
    }
    WHEN("calculating undershoot") {
        const double output_temp_diff =
            (input_volume * PlateControl::UNDERSHOOT_M_CONST) +
            PlateControl::UNDERSHOOT_B_CONST;
        auto output =
            PlateControl::calculate_undershoot(input_temp, input_volume);
        THEN("overshoot is correct") {
            REQUIRE_THAT(output, Catch::Matchers::WithinAbs(
                                     input_temp + output_temp_diff, 0.001));
        }
    }
}

SCENARIO("PlateControl peltier control works") {
    GIVEN("a PlateControl object with room temperature thermistors") {
        constexpr double input_volume = 25.0F;
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
        auto plateControl =
            plate_control::PlateControl(left, right, center, fan);
        THEN("the temperature reads correctly") {
            REQUIRE(plateControl.plate_temp() == ROOM_TEMP);
            REQUIRE(plateControl.setpoint() == 0.0F);
        }
        WHEN("setting a hot target temperature with 10 second hold time") {
            plateControl.set_new_target(HOT_TEMP, input_volume, 10.0F);
            THEN("the plate control object is initialized properly") {
                REQUIRE(plateControl.setpoint() == HOT_TEMP);
                REQUIRE(plateControl.status() ==
                        plate_control::PlateStatus::INITIAL_HEAT);
                REQUIRE(plateControl.get_hold_time().first == 10.0F);
                REQUIRE(plateControl.get_hold_time().second == 10.0F);
            }
            THEN("updating control should drive peltiers hot") {
                auto ctrl = plateControl.update_control(UPDATE_RATE_SEC);
                REQUIRE(ctrl.has_value());
                auto controlValues = ctrl.value();
                REQUIRE(controlValues.center_power > 0.0F);
                REQUIRE(controlValues.right_power > 0.0F);
                REQUIRE(controlValues.left_power > 0.0F);
            }
            WHEN("simulating a ramp") {
                auto temperature = plateControl.plate_temp();
                while ((temperature += 1.0F) < (HOT_TEMP - THRESHOLD)) {
                    set_temp(thermistors, temperature);
                    auto ctrl = plateControl.update_control(UPDATE_RATE_SEC);
                    DYNAMIC_SECTION("temp at " << temperature) {
                        REQUIRE(ctrl.has_value());
                        REQUIRE(plateControl.status() ==
                                plate_control::PlateStatus::INITIAL_HEAT);
                    }
                }
                temperature = HOT_TEMP;
                set_temp(thermistors, temperature);

                auto ctrl = plateControl.update_control(UPDATE_RATE_SEC);
                REQUIRE(ctrl.has_value());
                REQUIRE(plateControl.status() ==
                        plate_control::PlateStatus::OVERSHOOT);
            }
            WHEN(
                "the thermistors hit the target temperature and control is "
                "updated") {
                set_temp(thermistors, HOT_TEMP);
                static_cast<void>(plateControl.update_control(UPDATE_RATE_SEC));
                THEN("plate control should be in overshoot mode") {
                    REQUIRE(plateControl.status() ==
                            plate_control::PlateStatus::OVERSHOOT);
                    REQUIRE(!plateControl.temp_within_setpoint());
                }
                AND_WHEN("the temperature is at the overshoot temperature") {
                    const auto real_target =
                        HOT_TEMP +
                        plate_control::PlateControl::calculate_overshoot(
                            HOT_TEMP, input_volume);
                    set_temp(thermistors, real_target);
                    static_cast<void>(
                        plateControl.update_control(UPDATE_RATE_SEC));
                    THEN("temp_within_setpoint still returns false") {
                        REQUIRE(!plateControl.temp_within_setpoint());
                    }
                }
                AND_WHEN("holding at temperature for >10 seconds") {
                    static_cast<void>(plateControl.update_control(
                        plateControl.OVERSHOOT_TIME));
                    THEN("the plate should move to Holding mode") {
                        REQUIRE(plateControl.status() ==
                                plate_control::PlateStatus::STEADY_STATE);
                        REQUIRE(plateControl.temp_within_setpoint());
                    }
                    static_cast<void>(
                        plateControl.update_control(UPDATE_RATE_SEC));
                    THEN("hold time should decrease") {
                        double remaining_hold, total_hold;
                        std::tie(remaining_hold, total_hold) =
                            plateControl.get_hold_time();
                        REQUIRE_THAT(remaining_hold,
                                     Catch::Matchers::WithinAbs(
                                         total_hold - UPDATE_RATE_SEC, 0.001));
                    }
                    AND_WHEN(
                        "control is updated for long enough to exceed the hold "
                        "time") {
                        static_cast<void>(plateControl.update_control(12.0F));
                        THEN("the hold time should not go below zero") {
                            REQUIRE(plateControl.get_hold_time().first == 0.0F);
                            REQUIRE(plateControl.get_hold_time().second ==
                                    10.0F);
                        }
                    }
                }
            }
        }
        WHEN("setting a cold target temperature with 10 second hold time") {
            plateControl.set_new_target(COLD_TEMP, input_volume, 10.0F);
            REQUIRE(plateControl.setpoint() == COLD_TEMP);
            THEN("updating control should drive peltiers cold") {
                auto ctrl = plateControl.update_control(UPDATE_RATE_SEC);
                REQUIRE(ctrl.has_value());
                auto controlValues = ctrl.value();
                REQUIRE(controlValues.center_power < 0.0F);
                REQUIRE(controlValues.right_power < 0.0F);
                REQUIRE(controlValues.left_power < 0.0F);
            }
            WHEN("simulating a ramp") {
                auto temperature = plateControl.plate_temp();
                while ((temperature -= 1.0F) > (COLD_TEMP + THRESHOLD)) {
                    set_temp(thermistors, temperature);
                    auto ctrl = plateControl.update_control(UPDATE_RATE_SEC);
                    DYNAMIC_SECTION("temp at " << temperature) {
                        REQUIRE(ctrl.has_value());
                        REQUIRE(plateControl.status() ==
                                plate_control::PlateStatus::INITIAL_COOL);
                    }
                }
                temperature = COLD_TEMP;
                set_temp(thermistors, temperature);

                auto ctrl = plateControl.update_control(UPDATE_RATE_SEC);
                REQUIRE(ctrl.has_value());
                REQUIRE(plateControl.status() ==
                        plate_control::PlateStatus::OVERSHOOT);
            }
            WHEN(
                "the thermistors hit the target temperature and control is "
                "updated") {
                set_temp(thermistors, COLD_TEMP);
                static_cast<void>(plateControl.update_control(UPDATE_RATE_SEC));
                THEN("plate control should be in overshoot mode") {
                    REQUIRE(plateControl.status() ==
                            plate_control::PlateStatus::OVERSHOOT);
                    REQUIRE(!plateControl.temp_within_setpoint());
                }
                AND_WHEN("holding at temperature for >10 seconds") {
                    static_cast<void>(plateControl.update_control(
                        plateControl.OVERSHOOT_TIME));
                    THEN("the plate should move to Holding mode") {
                        REQUIRE(plateControl.status() ==
                                plate_control::PlateStatus::STEADY_STATE);
                        REQUIRE(plateControl.temp_within_setpoint());
                    }
                    static_cast<void>(
                        plateControl.update_control(UPDATE_RATE_SEC));
                    THEN("hold time should decrease") {
                        double remaining_hold, total_hold;
                        std::tie(remaining_hold, total_hold) =
                            plateControl.get_hold_time();
                        REQUIRE_THAT(remaining_hold,
                                     Catch::Matchers::WithinAbs(
                                         total_hold - UPDATE_RATE_SEC, 0.001));
                    }
                    AND_WHEN(
                        "control is updated for long enough to exceed the hold "
                        "time") {
                        static_cast<void>(plateControl.update_control(12.0F));
                        THEN("the hold time should not go below zero") {
                            REQUIRE(plateControl.get_hold_time().first == 0.0F);
                            REQUIRE(plateControl.get_hold_time().second ==
                                    10.0F);
                        }
                    }
                }
            }
        }
    }
}

SCENARIO("PlateControl idle fan control works") {
    GIVEN("a PlateControl object with room temperature thermistors") {
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
        auto plateControl =
            plate_control::PlateControl(left, right, center, fan);
        WHEN("getting fan power for an idle system") {
            WHEN("the heatsink is at room temperature") {
                auto power = plateControl.fan_idle_power();
                THEN("the fan should be turned off") { REQUIRE(power == 0.0F); }
            }
            WHEN("the heatsink is between 68 and 75 degrees") {
                for (double temp = 68.0F; temp < 75.0F; temp += 1.0F) {
                    fan.thermistor.temp_c = temp;
                    auto power = plateControl.fan_idle_power();
                    THEN("the fan power should be temp / 100") {
                        REQUIRE_THAT(power, Catch::Matchers::WithinAbs(
                                                temp / 100.0F, 0.01));
                    }
                }
            }
            WHEN("heatsink temperature is over 75 degrees") {
                fan.thermistor.temp_c = 75.5F;
                auto power = plateControl.fan_idle_power();
                THEN("the fan power should be 0.8") {
                    REQUIRE_THAT(power, Catch::Matchers::WithinAbs(0.8, 0.01));
                }
            }
        }
    }
}

SCENARIO("PlateControl active fan control works") {
    constexpr double input_volume = 25.0F;
    GIVEN(
        "a PlateControl object with room temperature thermistors and FAN pid "
        "of (1,0,0)") {
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
        auto plateControl =
            plate_control::PlateControl(left, right, center, fan);
        GIVEN("a fan in manual mode") {
            fan.manual_control = true;
            WHEN("the setpoint is set to ramp the peltiers down") {
                plateControl.set_new_target(COLD_TEMP, input_volume);
                AND_WHEN("the heatsink is at a reasonable temperature") {
                    THEN("updating control should give fan power of 0") {
                        auto ctrl =
                            plateControl.update_control(UPDATE_RATE_SEC);
                        REQUIRE(ctrl.has_value());
                        REQUIRE(ctrl.value().fan_power == 0.0F);
                        REQUIRE(fan.manual_control);
                    }
                }
                AND_WHEN("the heatsink is dangerously warm") {
                    fan.thermistor.temp_c = HOT_TEMP;
                    THEN("updating control forces the fan out of manual mode") {
                        auto ctrl =
                            plateControl.update_control(UPDATE_RATE_SEC);
                        REQUIRE(ctrl.has_value());
                        REQUIRE(ctrl.value().fan_power > 0.0F);
                        REQUIRE(!fan.manual_control);
                    }
                }
            }
        }
        WHEN("the setpoint is set to ramp the peltiers down") {
            plateControl.set_new_target(COLD_TEMP, input_volume);
            // We need the undershot target to accurately asses the control
            const auto undershot_target =
                plateControl.calculate_undershoot(COLD_TEMP, input_volume);
            auto ctrl = plateControl.update_control(UPDATE_RATE_SEC);
            REQUIRE(ctrl.has_value());
            THEN("the fan should drive at exactly 0.7") {
                REQUIRE(ctrl.value().fan_power == 0.7F);
            }
            AND_WHEN("the target temperature is reached with heatsink at 60") {
                set_temp(thermistors, undershot_target, 60.0F);
                ctrl = plateControl.update_control(UPDATE_RATE_SEC);
                REQUIRE(ctrl.has_value());
                THEN("the fan is driven between 0.35 and 0.55") {
                    REQUIRE(ctrl.value().fan_power >= 0.35);
                    REQUIRE(ctrl.value().fan_power <= 0.55);
                }
            }
        }
        WHEN("the setpoint is set to ramp the peltiers up to a warm temp") {
            plateControl.set_new_target(WARM_TEMP, input_volume);
            auto ctrl = plateControl.update_control(UPDATE_RATE_SEC);
            REQUIRE(ctrl.has_value());
            THEN("the fan should drive at exactly 0.15") {
                REQUIRE(ctrl.value().fan_power == 0.15F);
            }
            AND_WHEN(
                "the target temperature is reached with heatsink at target + "
                "2") {
                set_temp(thermistors, WARM_TEMP, WARM_TEMP + 2.0F);
                ctrl = plateControl.update_control(UPDATE_RATE_SEC);
                REQUIRE(ctrl.has_value());
                THEN("the fan is driven between 0.35 and 0.55") {
                    REQUIRE_THAT(fan.temp_target,
                                 Catch::Matchers::WithinAbs(
                                     WARM_TEMP + plate_control::PlateControl::
                                                     FAN_SETPOINT_OFFSET,
                                     0.1));
                    REQUIRE(ctrl.value().fan_power >= 0.35);
                    REQUIRE(ctrl.value().fan_power <= 0.55);
                }
            }
        }
        WHEN("the setpoint is set to ramp the peltiers up to a hot temp") {
            plateControl.set_new_target(HOT_TEMP, input_volume);
            auto ctrl = plateControl.update_control(UPDATE_RATE_SEC);
            REQUIRE(ctrl.has_value());
            THEN("the fan should drive at exactly 0.15") {
                REQUIRE(ctrl.value().fan_power == 0.15F);
            }
            AND_WHEN("the target temperature is reached with heatsink at 73") {
                set_temp(thermistors, HOT_TEMP, 73);
                ctrl = plateControl.update_control(UPDATE_RATE_SEC);
                REQUIRE(ctrl.has_value());
                THEN("the fan is driven between 0.30 and 0.55") {
                    REQUIRE_THAT(fan.temp_target,
                                 Catch::Matchers::WithinAbs(70.0F, 0.1));
                    REQUIRE(ctrl.value().fan_power >= 0.30);
                    REQUIRE(ctrl.value().fan_power <= 0.55);
                }
            }
            AND_WHEN(
                "the target temperature is reached with heatsink at HOT_TEMP") {
                set_temp(thermistors, HOT_TEMP);
                ctrl = plateControl.update_control(UPDATE_RATE_SEC);
                REQUIRE(ctrl.has_value());
                THEN("the fan is driven at 0.8") {
                    REQUIRE_THAT(ctrl.value().fan_power,
                                 Catch::Matchers::WithinAbs(0.8, 0.01));
                }
            }
        }
    }
}
