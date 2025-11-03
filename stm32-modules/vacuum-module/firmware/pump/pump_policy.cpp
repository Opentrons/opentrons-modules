#include "firmware/pump_policy.hpp"

#include <cstdint>

#include "FreeRTOS.h"
#include "firmware/pump_hardware.h"
#include "projdefs.h"
#include "task.h"

using namespace pump_policy;

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto start_pump_motor(bool start) -> void { hw_start_pump_motor(start); }

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto set_pump_duty_cycle(uint16_t duty) -> void {
    hw_set_pump_duty_cycle(duty);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto get_pump_duty_cycle() -> uint16_t { return hw_get_pump_duty_cycle(); }

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto get_pump_rpm() -> double { return hw_get_pump_rpm(); }
