/**
 * The ThermalPlatePolicy class provides firmware implementation
 * of any stubbable hardware interactions needed in the Thermal
 * Plate Task.
 */
#pragma once

class ThermalPlatePolicy {
public:
    ThermalPlatePolicy();

    auto set_enabled(bool enabled) -> void;
};
