#include "firmware/vent_policy.hpp"

#include <cstdint>

#include "FreeRTOS.h"
#include "firmware/vent_hardware.h"
#include "projdefs.h"
#include "task.h"

using namespace vent_policy;

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto VentPolicy::open_vent(bool open) -> void { hw_open_vent(open); }
