#pragma once

#include "systemwide.h"

namespace vent_policy {

class VentPolicy {
  public:
    auto open_vent(bool open) -> void;
    auto get_vent_fault() -> bool;
};
}  // namespace vent_policy
