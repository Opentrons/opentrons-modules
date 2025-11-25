#include "firmware/vacuum_pressure_sensor_policy.hpp"

#include <cstdint>

#include "FreeRTOS.h"
#include "firmware/hardware_iface.hpp"
#include "firmware/vacuum_pressure_sensor_hardware.h"
#include "projdefs.h"
#include "systemwide.h"
#include "task.h"

using namespace vacuum_pressure_sensor::hardware;

auto VacuumPressureSensorPolicy::i2c_write(uint16_t dev_addr, uint16_t reg,
                                           uint8_t* data, uint16_t size)
    -> i2c::hardware::RxTxReturn {
    return i2c_comms->i2c_write(dev_addr, reg, data, size);
}

auto VacuumPressureSensorPolicy::i2c_read(uint16_t dev_addr, uint16_t reg,
                                          uint8_t* data, uint16_t size)
    -> i2c::hardware::RxTxReturn {
    return i2c_comms->i2c_read(dev_addr, reg, data, size);
}

auto VacuumPressureSensorPolicy::i2c_master_write(uint16_t dev_addr,
                                                  uint8_t* data, uint16_t size)
    -> RxTxReturn {
    auto ret = i2c_comms->i2c_master_write(dev_addr, data, size);
    return ret;
}

auto VacuumPressureSensorPolicy::i2c_master_read(uint16_t dev_addr,
                                                 uint8_t* data, uint16_t size)
    -> RxTxReturn {
    auto ret = i2c_comms->i2c_master_read(dev_addr, data, size);
    return ret;
}

auto VacuumPressureSensorPolicy::conversion_ended(
    VacuumPressureSensorId sensor_id) -> bool {
    return sensor_hardware_read_eoc_pin(sensor_id);
}

auto VacuumPressureSensorPolicy::sensor_reset(VacuumPressureSensorId sensor_id)
    -> void {
    sensor_hardware_sensor_reset(sensor_id);
}

auto VacuumPressureSensorPolicy::sleep_ms(uint32_t ms) -> void {
    vTaskDelay(pdMS_TO_TICKS(ms));
}
