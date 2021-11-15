#pragma once

class TestLidHeaterPolicy {
  private:
    bool _enabled = false;

  public:
    auto set_enabled(bool enabled) -> void { _enabled = enabled; }
};
