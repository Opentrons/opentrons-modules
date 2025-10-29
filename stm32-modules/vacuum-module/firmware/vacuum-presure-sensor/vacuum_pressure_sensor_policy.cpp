#include "firmware/mpr_pressure_sensor_policy.hpp"

#include <cstdint>

#include "FreeRTOS.h"
#include "firmware/hardware_iface.hpp"
#include "firmware/system_hardware.h"
#include "projdefs.h"
#include "systemwide.h"
#include "task.h"

using namespace mpr_pressure_sensor::hardware;

auto VacuumPressureSensorPolicy::i2c_write(uint16_t dev_addr, uint16_t reg, uint8_t* data,
                                uint16_t size) -> i2c::hardware::RxTxReturn {
    return i2c_comms->i2c_write(dev_addr, reg, data, size);
}

auto VacuumPressureSensorPolicy::i2c_read(uint16_t dev_addr, uint16_t reg, uint8_t* data,
                               uint16_t size) -> i2c::hardware::RxTxReturn {
    return i2c_comms->i2c_read(dev_addr, reg, data, size);
}

auto VacuumPressureSensorPolicy::conversion_ended() -> bool {
    return system_hardware_read_eoc_pin();
}

auto VacuumPressureSensorPolicy::sleep_ms(uint32_t ms) -> void {
    vTaskDelay(pdMS_TO_TICKS(ms));
}