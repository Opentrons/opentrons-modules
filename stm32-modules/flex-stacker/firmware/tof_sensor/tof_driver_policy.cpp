#include <stdint.h>

#include "firmware/tof_driver_policy.hpp"
#include "firmware/tof_sensor_hardware.h"
#include "systemwide.h"

using namespace tof::hardware;

auto TOFDriverPolicy::enable_tof_sensor(TOFSensorID sensor_id, bool enable) -> void {
    enable_tof_sensor_write(sensor_id, enable);
}

auto TOFDriverPolicy::i2c_write(uint16_t dev_addr, uint16_t reg, uint8_t* data,
                    uint16_t size) -> RxTxReturn {
    return i2c_comms->i2c_write(dev_addr, reg, data, size);
}

auto TOFDriverPolicy::i2c_read(uint16_t dev_addr, uint16_t reg, uint16_t size)
    -> RxTxReturn {
    return i2c_comms->i2c_read(dev_addr, reg, size);
}
