/*
 * firmware-specific internals and hooks for the plate task
 */

#include "firmware/freertos_thermal_plate_task.hpp"

#include <variant>

#include "FreeRTOS.h"
#include "core/ads1115.hpp"
#include "firmware/thermal_adc_policy.hpp"
#include "firmware/thermal_hardware.h"
#include "firmware/thermal_plate_policy.hpp"
#include "thermocycler-gen2/thermal_general.hpp"
#include "thermocycler-gen2/thermal_plate_task.hpp"

namespace thermal_plate_control_task {

using ADC_t = ADS1115::ADC<thermal_adc_policy::AdcPolicy>;

static constexpr uint8_t MAX_RETRIES = 5;

enum class Notifications : uint8_t {
    INCOMING_MESSAGE = 1,
};

struct ADCPinMap {
    ADC_ID_t adc_index;
    uint8_t adc_pin;
};

static FreeRTOSMessageQueue<thermal_plate_task::Message>
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    _thermal_plate_queue(static_cast<uint8_t>(Notifications::INCOMING_MESSAGE),
                         "Thermal Plate Queue");

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto _main_task =
    thermal_plate_task::ThermalPlateTask(_thermal_plate_queue);

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

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static std::array<ADC_t, ADC_ITR_NUM> _adc = {
    ADC_t(thermal_adc_policy::get_adc_1_policy()),
    ADC_t(thermal_adc_policy::get_adc_2_policy())};

// This array follows the definition of the ThermistorID enumeration
static constexpr std::array<ADCPinMap, thermal_general::ThermistorID::THERM_LID>
    _adc_map = {{
        // On rev1 boards, net names for right/left are swapped
        {ADC_FRONT, 3},  // Front right
        {ADC_FRONT, 1},  // Front left
        {ADC_FRONT, 2},  // Front center
        {ADC_REAR, 2},   // Back right
        {ADC_REAR, 0},   // Back left
        {ADC_REAR, 3},   // Back center
        {ADC_FRONT, 0}   // Heat sink
    }};

// Internal FreeRTOS data structure for the task
static StaticTask_t
    data;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

/**
 * Performs a conversion from an ADC pin and returns the value read.
 * @param[in] pin The pin to read
 * @return The value read by the ADC in counts. Will return 0 if the
 * ADC cannot be read.
 */
static auto read_thermistor(const ADCPinMap &pin) -> uint16_t {
    uint8_t retries = 0;
    bool done = false;
    // Keep trying to read
    auto result =
        _adc.at(static_cast<uint8_t>(pin.adc_index)).read(pin.adc_pin);
    while (!done) {
        if (std::holds_alternative<uint16_t>(result)) {
            done = true;
        } else if (++retries < MAX_RETRIES) {
            // Short delay for reliability
            vTaskDelay(pdMS_TO_TICKS(5));
            result =
                _adc.at(static_cast<uint8_t>(pin.adc_index)).read(pin.adc_pin);
        } else {
            // Retries expired
            return static_cast<uint16_t>(std::get<ADS1115::Error>(result));
        }
    }

    return std::get<uint16_t>(result);
}

static void run(void *param) {
    thermal_hardware_wait_for_init();
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    auto *task = reinterpret_cast<decltype(_main_task) *>(param);
    auto policy = plate_policy::ThermalPlatePolicy();
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
    _adc[ADC_FRONT].initialize();
    _adc[ADC_REAR].initialize();
    auto last_wake_time = xTaskGetTickCount();
    messages::ThermalPlateTempReadComplete readings{};
    while (true) {
        vTaskDelayUntil(
            &last_wake_time,
            // NOLINTNEXTLINE(readability-static-accessed-through-instance)
            _main_task.CONTROL_PERIOD_TICKS);
        readings.front_right = read_thermistor(
            _adc_map[thermal_general::ThermistorID::THERM_FRONT_RIGHT]);
        readings.front_left = read_thermistor(
            _adc_map[thermal_general::ThermistorID::THERM_FRONT_LEFT]);
        readings.front_center = read_thermistor(
            _adc_map[thermal_general::ThermistorID::THERM_FRONT_CENTER]);
        readings.back_left = read_thermistor(
            _adc_map[thermal_general::ThermistorID::THERM_BACK_LEFT]);
        readings.back_right = read_thermistor(
            _adc_map[thermal_general::ThermistorID::THERM_BACK_RIGHT]);
        readings.back_center = read_thermistor(
            _adc_map[thermal_general::ThermistorID::THERM_BACK_CENTER]);
        readings.heat_sink = read_thermistor(
            _adc_map[thermal_general::ThermistorID::THERM_HEATSINK]);
        readings.timestamp_ms = xTaskGetTickCount();

        auto send_ret = _main_task.get_message_queue().try_send(readings);
        static_cast<void>(
            send_ret);  // Not much we can do if messages won't send
    }
}

// Function that spins up the task
auto start()
    -> tasks::Task<TaskHandle_t,
                   thermal_plate_task::ThermalPlateTask<FreeRTOSMessageQueue>> {
    auto *handle = xTaskCreateStatic(run, "ThermalPlate", _stack.size(),
                                     &_main_task, 1, _stack.data(), &data);
    _thermal_plate_queue.provide_handle(handle);
    auto *thermistor_handle = xTaskCreateStatic(
        run_thermistor_task, "LidHeaterThermistors", _thermistor_stack.size(),
        &_main_task, 1, _thermistor_stack.data(), &_thermistor_data);
    configASSERT(thermistor_handle != nullptr);
    return tasks::Task<TaskHandle_t, decltype(_main_task)>{.handle = handle,
                                                           .task = &_main_task};
}
}  // namespace thermal_plate_control_task
