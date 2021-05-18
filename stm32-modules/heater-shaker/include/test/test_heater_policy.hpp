#pragma once

class TestHeaterPolicy {
  public:
    TestHeaterPolicy();
    explicit TestHeaterPolicy(bool pgood, bool can_reset);
    [[nodiscard]] auto power_good() const -> bool;
    [[nodiscard]] auto try_reset_power_good() -> bool;

    auto set_power_good(bool pgood) -> void;
    auto set_can_reset(bool can_reset) -> void;

  private:
    bool power_good_val;
    bool may_reset;
};
