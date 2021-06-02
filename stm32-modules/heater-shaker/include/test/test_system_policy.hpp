#pragma once

class TestSystemPolicy {
  public:
    auto enter_bootloader() -> void;
    auto reset_bootloader_entered() -> void;
    auto bootloader_entered() const -> bool;

  private:
    bool entered = false;
};
