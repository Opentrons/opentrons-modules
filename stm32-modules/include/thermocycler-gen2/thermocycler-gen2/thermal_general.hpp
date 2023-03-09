/**
 * This file contains general utilities, structures, and enumerations
 * for the thermal subsystem of the thermocycler-gen2 module
 */
#pragma once

#include <tuple>

#include "core/pid.hpp"
#include "core/thermistor_conversion.hpp"
#include "systemwide.h"
#include "thermocycler-gen2/errors.hpp"
#include "thermocycler-gen2/peltier_filter.hpp"

namespace thermal_general {

/** Enumeration of thermistors on the board.
 * This is specifically arranged to keep all of the plate-related
 * thermistors before the Lid, so mapping from the thermistors here
 * to the values in the Thermal Plate Process can be 1:1 indexing
 */
enum ThermistorID {
    THERM_FRONT_RIGHT,
    THERM_FRONT_LEFT,
    THERM_FRONT_CENTER,
    THERM_BACK_RIGHT,
    THERM_BACK_LEFT,
    THERM_BACK_CENTER,
    THERM_HEATSINK,
    THERM_LID,
    THERM_COUNT
};

// Disabled lint warning because we specifically want the rest
// of the parameters to be initialized by the task constructor
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
struct Thermistor {
    // Last converted temperature (0 if invalid)
    double temp_c = 0;
    // Last ADC result
    uint16_t last_adc = 0;
    // Current error
    errors::ErrorCode error = errors::ErrorCode::NO_ERROR;
    // These constant values should be set when the struct is initialized
    // in order to capture errors specific to a sensor that require
    // a system restart to rectify
    const double overtemp_limit_c;
    const errors::ErrorCode disconnected_error;
    const errors::ErrorCode short_error;
    const errors::ErrorCode overtemp_error;
    const uint8_t error_bit;
};

struct Peltier {
    using ThermistorPair = std::pair<Thermistor &, Thermistor &>;
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    double temp_target = 0.0F;  // Target temperature
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    const PeltierID id;  // ID to match to hardware - set at initialization
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    ThermistorPair thermistors;  // Links to the front & back thermistors
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    PID pid;  // Current PID loop
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    peltier_filter::PeltierFilter filter = peltier_filter::PeltierFilter();

    /** Get the current temperature of this peltier.*/
    [[nodiscard]] auto current_temp() const -> double {
        return (thermistors.first.temp_c + thermistors.second.temp_c) / 2;
    }
    /**
     * Get the difference in temperature between the front/back thermistors
     * for this peltier.
     */
    [[nodiscard]] auto current_temp_delta() const -> double {
        return std::abs(thermistors.first.temp_c + thermistors.second.temp_c);
    }
};

struct HeatsinkFan {
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    double temp_target = 0.0F;  // Target temperature
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    bool manual_control = false;  // Current state (manual or automatic)
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    Thermistor &thermistor;  // Thermistor for reading temperature
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    PID pid;  // Current PID loop
    /** Get the current temperature of the heatsink.*/
    [[nodiscard]] auto current_temp() const -> double {
        return thermistor.temp_c;
    }
};

}  // namespace thermal_general
