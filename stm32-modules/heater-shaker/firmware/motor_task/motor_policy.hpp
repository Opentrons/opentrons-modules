#pragma once

#include <cstdint>

#include "firmware/serial.hpp"
#include "heater-shaker/errors.hpp"
#include "systemwide.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvolatile"
#pragma GCC diagnostic ignored "-Wregister"
#include "mc_interface.h"
#include "motor_hardware.h"
#include "stm32f3xx_hal.h"
#include "system_serial_number.h"
#pragma GCC diagnostic pop

class MotorPolicy {
  private:
    static constexpr std::size_t SYSTEM_SERIAL_NUMBER_LENGTH =
        SYSTEM_WIDE_SERIAL_NUMBER_LENGTH;
    Serial _serial{};

  public:
    static constexpr int32_t DEFAULT_RAMP_RATE_RPM_PER_S = 1000;
    static constexpr int32_t MAX_RAMP_RATE_RPM_PER_S = 20000;
    static constexpr int32_t MIN_RAMP_RATE_RPM_PER_S = 1;
    MotorPolicy() = delete;
    explicit MotorPolicy(motor_hardware_handles* handles);
    auto set_rpm(int16_t rpm) -> errors::ErrorCode;
    auto set_pid_constants(double kp, double ki, double kd) -> void;
    [[nodiscard]] auto get_current_rpm() const -> int16_t;
    [[nodiscard]] auto get_target_rpm() const -> int16_t;
    auto stop() -> void;
    auto set_ramp_rate(int32_t rpm_per_s) -> errors::ErrorCode;

    auto homing_solenoid_disengage() -> void;
    auto homing_solenoid_engage(uint16_t current_ma) -> void;

    auto delay_ticks(uint16_t ticks) -> void;

    auto plate_lock_set_power(float power) -> void;
    auto plate_lock_disable() -> void;
    auto plate_lock_brake() -> void;
    auto plate_lock_open_sensor_read() -> bool;
    auto plate_lock_closed_sensor_read() -> bool;
    auto get_serial_number(void)
        -> std::array<char, SYSTEM_SERIAL_NUMBER_LENGTH>;

  private:
    static constexpr uint16_t MAX_SOLENOID_CURRENT_MA = 330;
    double ramp_rate_rpm_per_ms =
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
        static_cast<double>(DEFAULT_RAMP_RATE_RPM_PER_S) / 1000.0;
    motor_hardware_handles* hw_handles;
};
