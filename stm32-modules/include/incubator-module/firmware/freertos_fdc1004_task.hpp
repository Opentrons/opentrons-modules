#pragma once

#include "firmware/firmware_tasks.hpp"
#include "firmware/i2c_comms.hpp"

namespace fdc1004::tasks {

/**
 * @brief Runs the capacitive sensor task loop.
 * @param aggregator Queue aggregator used for G-code requests and responses.
 * @param i2c_bus_handle Pointer to the initialized I2C hardware object.
 */
auto run(::tasks::FirmwareTasks::QueueAggregator* aggregator,
         i2c::hardware::I2C* i2c_bus_handle) -> void;

}  // namespace fdc1004::tasks