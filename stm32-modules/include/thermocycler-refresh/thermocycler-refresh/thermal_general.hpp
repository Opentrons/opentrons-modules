/**
 * This file contains general utilities, structures, and enumerations
 * for the thermal subsystem of the thermocycler-refresh module
 */
#pragma once

#include "core/pid.hpp"
#include "core/thermistor_conversion.hpp"
#include "systemwide.h"
#include "thermocycler-refresh/errors.hpp"

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
    // ID to match to hardware - set at initialization
    const PeltierID id = PELTIER_NUMBER;
    // Current temperature
    double temp_current = 0.0F;
    // Target temperature
    double temp_target = 0.0F;
    // Current PID loop
    PID pid;
};
