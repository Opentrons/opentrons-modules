/*
 * firmware-specific internals and hooks for the plate task
 */

#include "firmware/freertos_thermal_plate_task.hpp"

#include <variant>

#include "FreeRTOS.h"
#include "firmware/ads1115.hpp"
#include "firmware/thermal_hardware.h"
#include "thermal_plate_policy.hpp"
#include "thermocycler-refresh/thermal_general.hpp"
#include "thermocycler-refresh/thermal_plate_task.hpp"

namespace thermal_plate_control_task {

enum class ADCAddress : uint8_t {
    ADC_FRONT = ((0x48) << 1),  // AKA ADC1
    ADC_REAR = ((0x49) << 1)    // AKA ADC2
};

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

static auto _main_task =
    thermal_plate_task::ThermalPlateTask(_thermal_plate_queue);

static constexpr uint32_t _stack_size = 500;
// Stack as a std::array because why not. Quiet lint because, well, we have to
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static std::array<StackType_t, _stack_size> _stack;

static constexpr uint32_t _thermistor_stack_size = 128;
// Stack as a std::array because why not. Quiet lint because, well, we have to
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static std::array<StackType_t, _thermistor_stack_size> _thermistor_stack;
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static StaticTask_t _thermistor_data;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static std::array<ADS1115::ADC, ADC_ITR_NUM> _adc = {
    ADS1115::ADC(static_cast<uint8_t>(ADCAddress::ADC_FRONT), ADC1_ITR),
    ADS1115::ADC(static_cast<uint8_t>(ADCAddress::ADC_REAR), ADC2_ITR)};

// This array follows the definition of the ThermistorID enumeration
static constexpr std::array<ADCPinMap, ThermistorID::THERM_LID> _adc_map = {{
    {ADC_FRONT, 1},  // Front right
    {ADC_FRONT, 3},  // Front left
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
static uint16_t read_thermistor(const ADCPinMap &pin) {
    auto result = _adc[static_cast<uint8_t>(pin.adc_index)].read(pin.adc_pin);
    if (std::holds_alternative<ADS1115::Error>(result)) {
        return 0;
    } else {
        return std::get<uint16_t>(result);
    }
}

static void run(void *param) {
    thermal_hardware_wait_for_init();
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    auto *task = reinterpret_cast<decltype(_main_task) *>(param);
    auto policy = ThermalPlatePolicy();
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
    thermal_hardware_setup();
    _adc[ADC_FRONT].initialize();
    _adc[ADC_REAR].initialize();
    auto last_wake_time = xTaskGetTickCount();
    messages::ThermalPlateTempReadComplete readings;
    while (true) {
        vTaskDelayUntil(
            &last_wake_time,
            // NOLINTNEXTLINE(readability-static-accessed-through-instance)
            _main_task.CONTROL_PERIOD_TICKS);

        readings.front_right = read_thermistor(_adc_map[THERM_FRONT_RIGHT]);
        readings.front_left = read_thermistor(_adc_map[THERM_FRONT_LEFT]);
        readings.front_center = read_thermistor(_adc_map[THERM_FRONT_CENTER]);
        readings.back_left = read_thermistor(_adc_map[THERM_BACK_RIGHT]);
        readings.back_right = read_thermistor(_adc_map[THERM_BACK_LEFT]);
        readings.back_center = read_thermistor(_adc_map[THERM_BACK_CENTER]);
        readings.heat_sink = read_thermistor(_adc_map[THERM_HEATSINK]);

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
