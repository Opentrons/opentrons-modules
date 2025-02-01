#include "firmware/tof_sensor_policy.hpp"

#include <stdint.h>

#include "FreeRTOS.h"
#include "firmware/hardware_iface.hpp"
#include "firmware/tmf8820_image.h"
#include "firmware/tof_sensor_hardware.h"
#include "systemwide.h"
#include "task.h"

using namespace tof::hardware;

auto TOFSensorPolicy::enable_tof_sensor(TOFSensorID sensor_id, bool enable)
    -> void {
    enable_tof_sensor_write(sensor_id, enable);
}

auto TOFSensorPolicy::i2c_write(uint16_t dev_addr, uint16_t reg, uint8_t* data,
                                uint16_t size) -> i2c::hardware::RxTxReturn {
    return i2c_comms->i2c_write(dev_addr, reg, data, size);
}

auto TOFSensorPolicy::i2c_read(uint16_t dev_addr, uint16_t reg, uint16_t size)
    -> i2c::hardware::RxTxReturn {
    return i2c_comms->i2c_read(dev_addr, reg, size);
}

auto TOFSensorPolicy::sleep_ms(uint32_t ms) -> void {
    vTaskDelay(pdMS_TO_TICKS(ms));
}
