#include "test/test_system_policy.hpp"

auto TestSystemPolicy::enter_bootloader() -> void { entered = true; }

auto TestSystemPolicy::reset_bootloader_entered() -> void { entered = false; }

auto TestSystemPolicy::bootloader_entered() const -> bool { return entered; }
