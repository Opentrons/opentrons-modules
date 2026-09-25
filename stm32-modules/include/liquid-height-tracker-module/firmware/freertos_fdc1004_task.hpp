#pragma once

#include "firmware/i2c_comms.hpp"

namespace fdc1004::tasks {

/**
 * @brief Spawns the native FreeRTOS task thread for the liquid height tracker module.
 * @param i2c_bus_handle Pointer to the globally initialized i2c::hardware::I2C object.
 */
auto FDC1004_Task_Register(i2c::hardware::I2C* i2c_bus_handle) -> void;

} // namespace fdc1004::tasks