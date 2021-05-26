#include "system_policy.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvolatile"
#include "system_hardware.h"
#pragma GCC diagnostic pop

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto SystemPolicy::enter_bootloader() -> void {
    system_hardware_enter_bootloader();
}
