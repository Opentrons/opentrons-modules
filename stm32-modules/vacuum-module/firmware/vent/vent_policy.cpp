#include "firmware/vent_policy.hpp"

#include "firmware/vent_hardware.h"

using namespace vent_policy;

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto VentPolicy::open_vent(bool open) -> void { hw_open_vent(open); }
