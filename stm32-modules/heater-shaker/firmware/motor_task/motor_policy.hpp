#pragma once

#include <stdint.h>
#include "mc_interface.h"

class MotorPolicy {
    public:
        static constexpr uint32_t RAMP_MS_PER_RPM = 1;
        MotorPolicy() = delete;
        explicit MotorPolicy(MCI_Handle_t *handle);
        auto set_rpm(int16_t rpm) -> void;
        [[nodiscard]] auto get_current_rpm() const -> int16_t;
        [[nodiscard]] auto get_target_rpm() const -> int16_t;
        auto stop() -> void;
    private:
        MCI_Handle_t *motor_handle;
};
