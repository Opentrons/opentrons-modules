#pragma once

#include <cstdint>

#include "systemwide.h"

namespace vent_policy {

class VentPolicy {
  public:
    auto open_vent(bool open) -> bool;
};
}  // namespace vent_policy
