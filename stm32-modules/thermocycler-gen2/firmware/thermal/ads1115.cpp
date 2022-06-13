/**
 * @file ads1115.cpp
 * @brief
 * Driver file for the ADS1115 ADC
 * @details
 * This file provides functionality to control ADS1115 Analog to Digital
 * Converter IC's. Each of these chips provides four channels of 16-bit
 * analog conversion.
 */

#include "firmware/ads1115.hpp"

#include <array>
#include <atomic>

#include "FreeRTOS.h"
#include "firmware/freertos_synchronization.hpp"
#include "semphr.h"
#include "task.h"

namespace ADS1115 {

/** This structure defines meta-info about each ADC.
 * This is stored statically instead of per-instance so
 * that multiple objects can be created pointing to the
 * same physical ADC, and each can be read across multiple
 * threads.
 */
struct adc_hardware {
    // Track whether this ADC has been initialized.
    std::atomic<bool> _initialization_started = false;
    bool _initialization_done = false;
    // Semaphore information for the ADC
    freertos_synchronization::FreeRTOSMutex _mutex =
        freertos_synchronization::FreeRTOSMutex();
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static std::array<adc_hardware, ADC_ITR_NUM> _adc_hardware;

ADC::ADC(uint8_t addr, ADC_ITR_T id) : _addr(addr), _id(id), _last_result(0) {}

void ADC::initialize() {
    bool initialization_started =
        _adc_hardware.at(_id)._initialization_started.exchange(true);
    if (initialization_started) {
        // Spin until initialization is completed by other thread
        while (!initialized()) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast)
            taskYIELD();
        }
    } else {
        // Check that the mutex is configured fine
        configASSERT(_adc_hardware.at(_id)._mutex.get_count() == 1);

        // Write to the Lo and Hi threshold registers first to enable the ALERT
        // pin
        thermal_i2c_write_16(_addr, lo_thresh_addr, lo_thresh_default);
        thermal_i2c_write_16(_addr, hi_thresh_addr, hi_thresh_default);

        thermal_i2c_write_16(_addr, config_addr, config_default);

        _adc_hardware.at(_id)._initialization_done = true;
    }
}

auto ADC::read(uint16_t pin) -> ADC::ReadVal {
    uint32_t notification_val = 0;
    bool i2c_ret = false;
    const TickType_t max_block_time = pdMS_TO_TICKS(500);
    if (!initialized()) {
        return ReadVal(Error::ADCInit);
    }
    if (!(pin < pin_count)) {
        return ReadVal(Error::ADCPin);
    }

    if (!get_lock()) {
        return ReadVal(Error::ADCTimeout);
    }

    i2c_ret = thermal_arm_adc_for_read(_id);
    if (!i2c_ret) {
        static_cast<void>(release_lock());
        return ReadVal(Error::DoubleArm);
    }
    // This kicks off the conversion on the selected pin
    i2c_ret = thermal_i2c_write_16(
        _addr, config_addr,
        config_default | (pin << config_mux_shift) | config_start_read);

    if (i2c_ret) {
        // thermal_hardware.c will notify this task once the correct GPIO sends
        // a pulse indicating ADC READY.
        notification_val = ulTaskNotifyTake(pdTRUE, max_block_time);

        if (notification_val == 1) {
            i2c_ret =
                thermal_i2c_read_16(_addr, conversion_addr, &_last_result);
        }
    }

    static_cast<void>(release_lock());
    if ((!i2c_ret) || (notification_val == 0)) {
        auto err = (i2c_ret) ? Error::ADCTimeout : Error::I2CTimeout;
        return ReadVal(err);
    }

    return ReadVal(_last_result);
}

auto ADC::initialized() -> bool {
    return (_adc_hardware.at(_id)._initialization_done);
}

auto ADC::get_lock() -> bool {
    if (!initialized()) {
        return false;
    }
    _adc_hardware.at(_id)._mutex.acquire();
    return true;
}

auto ADC::release_lock() -> bool {
    if (!initialized()) {
        return false;
    }
    _adc_hardware.at(_id)._mutex.release();
    return true;
}

}  // namespace ADS1115
