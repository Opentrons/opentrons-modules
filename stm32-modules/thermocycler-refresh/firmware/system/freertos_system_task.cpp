/*
 * firmware-specific functions and data for ui control task
 */
#include "firmware/freertos_system_task.hpp"

#include <array>

#include "FreeRTOS.h"
#include "core/timer.hpp"
#include "firmware/freertos_message_queue.hpp"
#include "firmware/freertos_timer.hpp"
#include "firmware/system_hardware.h"
#include "firmware/system_led_hardware.h"
#include "firmware/system_policy.hpp"
#include "task.h"
#include "thermocycler-refresh/board_revision.hpp"
#include "thermocycler-refresh/system_task.hpp"
#include "thermocycler-refresh/tasks.hpp"

namespace system_control_task {

enum class Notifications : uint8_t {
    INCOMING_MESSAGE = 1,
};

static FreeRTOSMessageQueue<system_task::Message>
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    _system_queue(static_cast<uint8_t>(Notifications::INCOMING_MESSAGE),
                  "UI Message Queue");

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto _task = system_task::SystemTask(_system_queue);

static constexpr uint32_t stack_size = 500;
// Stack as a std::array because why not. Quiet lint because, well, we have to
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static std::array<StackType_t, stack_size> stack;

// Internal FreeRTOS data structure for the task
static StaticTask_t
    data;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

// Periodic timer for UI updates
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static timer::GenericTimer<freertos_timer::FreeRTOSTimer> _led_timer(
    "led timer", decltype(_task)::LED_UPDATE_PERIOD_MS, true,
    [ObjectPtr = &_task] { ObjectPtr->led_timer_callback(); });

// One shot timer for front button events.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static freertos_timer::FreeRTOSTimer _front_button_timer(
    "button timer", FRONT_BUTTON_DEBOUNCE_MS, false, [] {
        // TODO(Frank, 2/14/2022): Update this to send a message to the
        // system task instead of just turning the button on/off
        auto pressed = system_front_button_pressed();
        system_front_button_led_set(!pressed);
        if (pressed) {
            _front_button_timer.start();
        }
    });

/**
 * @brief This is the DIRECT callback from .c file that will start the
 * Front Button Timer to notify the main task of a new button press
 * event.
 *
 */
static auto front_button_callback() -> void {
    _front_button_timer.start_from_isr();
}

// Actual function that runs inside the task, unused param because we don't get
// to pick the function type
static void run(void *param) {
    using namespace board_revision;
    system_hardware_setup(
        BoardRevisionIface::get() == BoardRevision::BOARD_REV_1,
        front_button_callback);
    system_led_initialize();
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    auto *task = reinterpret_cast<decltype(_task) *>(param);
    auto policy = SystemPolicy();
    _led_timer.start();
    while (true) {
        task->run_once(policy);
    }
}

// Function that spins up the task
auto start() -> tasks::Task<TaskHandle_t,
                            system_task::SystemTask<FreeRTOSMessageQueue>> {
    auto *handle = xTaskCreateStatic(run, "SystemControl", stack.size(), &_task,
                                     3, stack.data(), &data);
    _system_queue.provide_handle(handle);
    return tasks::Task<TaskHandle_t, decltype(_task)>{.handle = handle,
                                                      .task = &_task};
}
}  // namespace system_control_task
