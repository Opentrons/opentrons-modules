#include "firmware/vent_policy.hpp"

#include "firmware/vent_hardware.h"

using namespace vent_policy;

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto vent_policy::VentPolicy::open_vent(bool open) -> void { hw_open_vent(open); }

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto vent_policy::VentPolicy::get_vent_fault() -> bool { return hw_vent_fault_detected(); }
