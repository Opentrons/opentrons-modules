#pragma once

#include <atomic>
#include <optional>

#include "FreeRTOS.h"
#include "firmware/freertos_synchronization.hpp"
#include "firmware/thermal_hardware.h"
#include "semphr.h"
#include "task.h"

namespace thermal_adc_policy {

static auto get_adc_1_policy() -> ADCPolicy&;

static auto get_adc_2_policy() -> ADCPolicy&;

/**
 * The policy implementation for the ADC objects uses singletons for each
 * of the ADC instances on the board.
 *
 * Note that the implementation of `get_adc_1` and `get_adc_2` is thread
 * safe under C++11 because static variable initialization is thread-safe.
 */
class AdcPolicy {
  public:
    static constexpr const uint8_t ADC_1_ADDRESS = (0x48) << 1;
    static constexpr const uint8_t ADC_2_ADDRESS = (0x49) << 1;

    AdcPolicy(uint8_t address, ADC_ITR_T id);
    AdcPolicy(AdcPolicy const&) = delete;
    void operator=(AdcPolicy const&) = delete;

    auto ads1115_mark_initialized() -> void;

    auto ads1115_check_initialized() -> bool;

    auto ads1115_get_lock() -> void;

    auto ads1115_release_lock() -> void;

    auto ads1115_arm_for_read() -> bool;

    auto ads1115_i2c_write_16(uint8_t reg, uint16_t value) -> bool;

    auto ads1115_i2c_read_16(uint8_t reg) -> std::optional<uint16_t>;

    auto ads1115_wait_for_pulse(uint32_t max_wait_ms) -> bool;

    auto task_yield() -> void;

  private:
    // I2C address for communication
    const uint8_t _i2c_address;
    // ID enumeration for arming task notifications
    const ADC_ITR_T _id;
    // Whether this instance has been initialized
    std::atomic_bool _initialized;
    // Mutex lock for this instance
    freertos_synchronization::FreeRTOSMutex _mutex;
};

};  // namespace thermal_adc_policy
