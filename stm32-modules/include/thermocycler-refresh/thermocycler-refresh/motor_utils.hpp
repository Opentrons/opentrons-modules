/**
 * @file motor_utils.hpp
 * @brief Provides some utility functions for motor control on the
 * Thermocycler Refresh
 *
 */

#pragma once

#include <algorithm>

namespace motor_util {

class LidStepper {
  private:
    // Max current the DAC can output, in milliamperes
    constexpr static double DAC_MAX_CURRENT = 330;
    // Max register value for the dac
    constexpr static uint32_t DAC_MAX_VALUE = 0xFF;

    // Steps per 360 degrees
    constexpr static double STEPS_PER_REV = 200.0 / 360.0;
    // We are using 1/32 microstepping
    constexpr static double MICROSTEPPING = 32;
    // Gear ratio factor
    constexpr static double GEAR_RATIO_SCALAR = 99.5;
    // Total factor to multiply from angle to degrees
    constexpr static double DEGREES_TO_MICROSTEPS =
        STEPS_PER_REV * MICROSTEPPING * GEAR_RATIO_SCALAR;
    constexpr static double ROTATION_TO_STEPS = DEGREES_TO_MICROSTEPS * 360;

  public:
    /**
     * @brief Convert a current value in milliamperes to a DAC value.
     *
     * @param mamps Milliampere drive value to set for the current control
     * @return uint8_t containing the DAC register value for this voltage
     */
    static auto current_to_dac(double mamps) -> uint8_t {
        // hardware has a 1ohm sense resistor and the driver has an implicit 10x
        // divider. the dac can express a max of 3.3V, so the maximum current we
        // can drive is 330mA at 3.3V/dac fullscale of 255. we can therefore
        // clamp the current input to 330 current_ma =
        //    ((current_ma > MAX_SOLENOID_CURRENT_MA) ? MAX_SOLENOID_CURRENT_MA
        //                                            : current_ma);
        // and then rescale into 8 bits with 330 ending up at 255
        auto dac_intermediate =
            std::min(mamps, DAC_MAX_CURRENT) * DAC_MAX_VALUE;
        return static_cast<uint8_t>(
            static_cast<uint32_t>(dac_intermediate / DAC_MAX_CURRENT) &
            DAC_MAX_VALUE);
    }

    /**
     * @brief Convert an angle to a number of microsteps
     *
     * @param angle The angle in degrees. Can be positive or negative
     * @return int32_t count of steps to reach this angle
     */
    static auto angle_to_microsteps(double angle) -> int32_t {
        return static_cast<int32_t>(angle * DEGREES_TO_MICROSTEPS);
    }
};

}  // namespace motor_util
