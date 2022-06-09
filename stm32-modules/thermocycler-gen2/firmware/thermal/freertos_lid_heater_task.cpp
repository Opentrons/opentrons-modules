/*
 * firmware-specific internals and hooks for the lid-heater task
 */

#include "firmware/freertos_lid_heater_task.hpp"

#include "FreeRTOS.h"
#include "core/ads1115.hpp"
#include "firmware/lid_heater_policy.hpp"
#include "firmware/thermal_hardware.h"
#include "thermocycler-gen2/lid_heater_task.hpp"

namespace lid_heater_control_task {

constexpr uint8_t _adc_address = (0x49 << 1);

constexpr uint8_t _adc_lid_pin = 1;

enum class Notifications : uint8_t {
    INCOMING_MESSAGE = 1,
};

static FreeRTOSMessageQueue<lid_heater_task::Message>
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    _lid_heater_queue(static_cast<uint8_t>(Notifications::INCOMING_MESSAGE),
                      "Lid Heater Queue");

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto _main_task = lid_heater_task::LidHeaterTask(_lid_heater_queue);

static constexpr uint32_t _stack_size = 512;
// Stack as a std::array because why not. Quiet lint because, well, we have to
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static std::array<StackType_t, _stack_size> _stack;

static constexpr uint32_t _thermistor_stack_size = 256;
// Stack as a std::array because why not. Quiet lint because, well, we have to
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static std::array<StackType_t, _thermistor_stack_size> _thermistor_stack;
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static StaticTask_t _thermistor_data;

// Internal FreeRTOS data structure for the task
static StaticTask_t
    data;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static void run(void *param) {
    thermal_hardware_wait_for_init();
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    auto *task = reinterpret_cast<decltype(_main_task) *>(param);
    auto policy = LidHeaterPolicy();
    while (true) {
        task->run_once(policy);
    }
}

/**
 * The thermistor task exists to kick off ADC conversions and, implicitly,
 * drive the timing of the control loop. The main lid heater task reacts to
 * the message sent by updating its control loop.
 */
static void run_thermistor_task(void *param) {
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
    static_assert(configTICK_RATE_HZ == 1000,
                  "FreeRTOS tickrate must be at 1000 Hz");
    static_cast<void>(param);
    thermal_hardware_setup();
    ADS1115::ADC adc(_adc_address, ADC2_ITR);
    adc.initialize();
    auto last_wake_time = xTaskGetTickCount();
    messages::LidTempReadComplete readings{};
    while (true) {
        vTaskDelayUntil(
            &last_wake_time,
            // NOLINTNEXTLINE(readability-static-accessed-through-instance)
            _main_task.CONTROL_PERIOD_TICKS);
        auto result = adc.read(_adc_lid_pin);
        if (std::holds_alternative<ADS1115::Error>(result)) {
            readings.lid_temp = 0;
        } else {
            readings.lid_temp = std::get<uint16_t>(result);
        }
        readings.timestamp_ms = xTaskGetTickCount();
        auto send_ret = _main_task.get_message_queue().try_send(readings);
        static_cast<void>(
            send_ret);  // Not much we can do if messages won't send
    }
}

// Function that spins up the task
auto start()
    -> tasks::Task<TaskHandle_t,
                   lid_heater_task::LidHeaterTask<FreeRTOSMessageQueue>> {
    auto *handle = xTaskCreateStatic(run, "LidHeater", _stack.size(),
                                     &_main_task, 1, _stack.data(), &data);
    _lid_heater_queue.provide_handle(handle);
    auto *thermistor_handle = xTaskCreateStatic(
        run_thermistor_task, "LidHeaterThermistors", _thermistor_stack.size(),
        &_main_task, 1, _thermistor_stack.data(), &_thermistor_data);
    configASSERT(thermistor_handle != nullptr);
    return tasks::Task<TaskHandle_t, decltype(_main_task)>{.handle = handle,
                                                           .task = &_main_task};
}
}  // namespace lid_heater_control_task
