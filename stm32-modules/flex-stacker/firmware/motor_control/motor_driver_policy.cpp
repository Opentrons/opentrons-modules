#include "firmware/motor_driver_policy.hpp"

#include "firmware/motor_hardware.h"

using namespace motor_driver_policy;

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MotorDriverPolicy::tmc2160_transmit_receive(MotorID motor_id,
                                                 MessageT& data) -> RxTxReturn {
    MessageT retBuf = {0};
    if (motor_spi_sendreceive(motor_id, data.data(), retBuf.data(),
                              data.size())) {
        return RxTxReturn(retBuf);
    }
    return RxTxReturn();
}

auto MotorDriverPolicy::start_stream(MotorID motor_id, MessageT& data) -> bool {
    return start_spi_stream(motor_id, data.data());
}

auto MotorDriverPolicy::stop_stream() -> bool { return stop_spi_stream(); }