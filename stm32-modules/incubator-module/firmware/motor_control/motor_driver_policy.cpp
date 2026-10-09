#include "firmware/motor_driver_policy.hpp"

#include "firmware/motor_hardware.h"
#include "incubator-module/tmc5160_interface.hpp"

using namespace motor_driver_policy;

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MotorDriverPolicy::tmc5160_transmit_receive(MotorID motor_id,
                                                 tmc5160::MessageT& data)
    -> RxTxReturn {
    tmc5160::MessageT retBuf = {0};
    if (motor_spi_sendreceive(motor_id, data.data(), retBuf.data(),
                              data.size())) {
        return {retBuf};
    }
    return {};
}
