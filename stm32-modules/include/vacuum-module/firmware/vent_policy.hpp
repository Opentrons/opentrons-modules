#pragma once

#include "systemwide.h"

namespace vent_policy {

class VentPolicy {
  public:
    auto open_vent(bool open) -> void;
};
}  // namespace vent_policy
