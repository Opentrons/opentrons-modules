//need any other headers (like heater_policy.cpp or motor_policy.cpp)?

#include <array>
#include <iterator>
#include <ranges>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvolatile"
#include "system_hardware.h"
#include "system_serial_number.h"
#pragma GCC diagnostic pop

#include "system_policy.hpp"

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto SystemPolicy::enter_bootloader() -> void {
    system_hardware_enter_bootloader();
}
